#include "StarDictionary.h"

#include <Logging.h>
#include <Serialization.h>
#include <ZipFile.h>

#include <algorithm>
#include <cctype>

namespace {
constexpr uint32_t DICT_CACHE_MAGIC = 0x44494346;  // arbitrary, distinct from other cache formats
constexpr uint8_t DICT_CACHE_VERSION = 1;
constexpr char CACHE_DIR[] = "/.crosspoint";
constexpr size_t MAX_WORD_LEN = 127;           // bounds stack usage while parsing .idx
constexpr size_t MAX_DEFINITION_SIZE = 16384;  // defensive cap against a corrupt/oversized cache entry
constexpr size_t IDX_READ_CHUNK = 512;

char toLowerAscii(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

int compareCaseInsensitive(const std::string& a, const std::string& b) {
  const size_t n = std::min(a.size(), b.size());
  for (size_t i = 0; i < n; ++i) {
    const char ca = toLowerAscii(a[i]);
    const char cb = toLowerAscii(b[i]);
    if (ca != cb) return ca < cb ? -1 : 1;
  }
  if (a.size() == b.size()) return 0;
  return a.size() < b.size() ? -1 : 1;
}

// Small buffered sequential reader over an already-open FsFile, avoiding
// per-byte SD reads while never buffering the whole .idx file.
class ChunkedIdxReader {
  FsFile& file;
  uint8_t buffer[IDX_READ_CHUNK];
  size_t bufLen = 0;
  size_t bufPos = 0;

  bool refill() {
    bufPos = 0;
    bufLen = static_cast<size_t>(std::max(0, file.read(buffer, sizeof(buffer))));
    return bufLen > 0;
  }

 public:
  explicit ChunkedIdxReader(FsFile& file) : file(file) {}

  // Returns false at EOF.
  bool readByte(uint8_t& out) {
    if (bufPos >= bufLen && !refill()) return false;
    out = buffer[bufPos++];
    return true;
  }

  bool readExact(uint8_t* dst, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      if (!readByte(dst[i])) return false;
    }
    return true;
  }

  // Reads a NUL-terminated word (NUL consumed, not stored). false at EOF before any byte.
  bool readWord(std::string& out) {
    out.clear();
    uint8_t b;
    bool any = false;
    while (readByte(b)) {
      any = true;
      if (b == '\0') return true;
      if (out.size() < MAX_WORD_LEN) out.push_back(static_cast<char>(b));
    }
    return any && false;  // ran out of data mid-word: treat as EOF/malformed
  }

  bool readU32BigEndian(uint32_t& out) {
    uint8_t b[4];
    if (!readExact(b, 4)) return false;
    out = (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) |
          (static_cast<uint32_t>(b[2]) << 8) | static_cast<uint32_t>(b[3]);
    return true;
  }
};
}  // namespace

std::string StarDictionary::cachePathFor(const StarDictInfo& info) {
  return std::string(CACHE_DIR) + "/dict_" +
         std::to_string(ZipFile::fnvHash64(info.dirPath.c_str(), info.dirPath.size())) + ".cpridx";
}

bool StarDictionary::buildCacheIfMissing(const StarDictInfo& info) {
  const std::string cachePath = cachePathFor(info);

  if (Storage.exists(cachePath.c_str())) {
    // Validate the header; a bad/older cache is rebuilt below.
    FsFile existing;
    if (Storage.openFileForRead("StarDict", cachePath, existing)) {
      uint32_t magic = 0;
      uint8_t version = 0;
      const bool ok = serialization::tryReadPod(existing, magic) && serialization::tryReadPod(existing, version) &&
                      magic == DICT_CACHE_MAGIC && version == DICT_CACHE_VERSION;
      existing.close();
      if (ok) return true;
    }
    Storage.remove(cachePath.c_str());
  }

  Storage.mkdir(CACHE_DIR);

  const std::string entriesTmpPath = cachePath + ".entries.tmp";
  const std::string lutTmpPath = cachePath + ".lut.tmp";

  FsFile idxFile;
  if (!Storage.openFileForRead("StarDict", info.idxPath, idxFile)) {
    LOG_ERR("StarDict", "Failed to open .idx: %s", info.idxPath.c_str());
    return false;
  }

  FsFile entriesFile;
  if (!Storage.openFileForWrite("StarDict", entriesTmpPath, entriesFile)) {
    idxFile.close();
    return false;
  }
  FsFile lutFile;
  if (!Storage.openFileForWrite("StarDict", lutTmpPath, lutFile)) {
    idxFile.close();
    entriesFile.close();
    return false;
  }

  ChunkedIdxReader reader(idxFile);
  uint32_t wordCount = 0;
  std::string word;
  bool malformed = false;

  while (reader.readWord(word)) {
    uint32_t defOffset = 0;
    uint32_t defSize = 0;
    if (!reader.readU32BigEndian(defOffset) || !reader.readU32BigEndian(defSize)) {
      malformed = true;
      break;
    }

    const uint32_t entryPos = static_cast<uint32_t>(entriesFile.position());
    serialization::tryWritePod(lutFile, entryPos);

    const uint8_t wordLen = static_cast<uint8_t>(std::min(word.size(), MAX_WORD_LEN));
    serialization::tryWritePod(entriesFile, wordLen);
    entriesFile.write(reinterpret_cast<const uint8_t*>(word.data()), wordLen);
    serialization::tryWritePod(entriesFile, defOffset);
    serialization::tryWritePod(entriesFile, defSize);

    wordCount++;
  }

  idxFile.close();
  entriesFile.close();
  lutFile.close();

  if (malformed || wordCount == 0) {
    LOG_ERR("StarDict", "Failed to parse .idx (malformed or empty): %s", info.idxPath.c_str());
    Storage.remove(entriesTmpPath.c_str());
    Storage.remove(lutTmpPath.c_str());
    return false;
  }

  // Final assembly: header, then LUT (offsets rebased into the final file), then entries.
  FsFile outFile;
  if (!Storage.openFileForWrite("StarDict", cachePath, outFile)) {
    Storage.remove(entriesTmpPath.c_str());
    Storage.remove(lutTmpPath.c_str());
    return false;
  }

  const uint32_t lutSize = wordCount * sizeof(uint32_t);
  const uint32_t entriesStart = HEADER_SIZE + lutSize;

  serialization::writePod(outFile, DICT_CACHE_MAGIC);
  serialization::writePod(outFile, DICT_CACHE_VERSION);
  serialization::writePod(outFile, wordCount);
  serialization::writePod(outFile, HEADER_SIZE);  // lutOffset, always right after the header

  FsFile lutIn;
  Storage.openFileForRead("StarDict", lutTmpPath, lutIn);
  for (uint32_t i = 0; i < wordCount; ++i) {
    uint32_t relativeOffset = 0;
    serialization::tryReadPod(lutIn, relativeOffset);
    const uint32_t absoluteOffset = relativeOffset + entriesStart;
    serialization::writePod(outFile, absoluteOffset);
  }
  lutIn.close();

  FsFile entriesIn;
  Storage.openFileForRead("StarDict", entriesTmpPath, entriesIn);
  uint8_t copyBuf[512];
  int readLen;
  while ((readLen = entriesIn.read(copyBuf, sizeof(copyBuf))) > 0) {
    outFile.write(copyBuf, static_cast<size_t>(readLen));
  }
  entriesIn.close();

  outFile.close();

  Storage.remove(entriesTmpPath.c_str());
  Storage.remove(lutTmpPath.c_str());

  LOG_INF("StarDict", "Built cache for %s: %u words", info.bookname.c_str(), wordCount);
  return true;
}

bool StarDictionary::open(const StarDictInfo& info) {
  close();

  const std::string cachePath = cachePathFor(info);
  if (!Storage.openFileForRead("StarDict", cachePath, cacheFile)) {
    return false;
  }

  uint32_t magic = 0;
  uint8_t version = 0;
  uint32_t lutOffset = 0;
  if (!serialization::tryReadPod(cacheFile, magic) || !serialization::tryReadPod(cacheFile, version) ||
      !serialization::tryReadPod(cacheFile, cacheWordCount) || !serialization::tryReadPod(cacheFile, lutOffset) ||
      magic != DICT_CACHE_MAGIC || version != DICT_CACHE_VERSION) {
    cacheFile.close();
    cacheWordCount = 0;
    return false;
  }

  if (!Storage.openFileForRead("StarDict", info.dictPath, dictFile)) {
    cacheFile.close();
    cacheWordCount = 0;
    return false;
  }

  return true;
}

void StarDictionary::close() {
  if (cacheFile.isOpen()) cacheFile.close();
  if (dictFile.isOpen()) dictFile.close();
  cacheWordCount = 0;
}

bool StarDictionary::readEntry(const uint32_t index, std::string& word, uint32_t& defOffset, uint32_t& defSize) const {
  if (index >= cacheWordCount) return false;

  FsFile& file = const_cast<FsFile&>(cacheFile);
  if (!file.seek(HEADER_SIZE + static_cast<size_t>(index) * sizeof(uint32_t))) return false;

  uint32_t entryOffset = 0;
  if (!serialization::tryReadPod(file, entryOffset)) return false;
  if (!file.seek(entryOffset)) return false;

  uint8_t wordLen = 0;
  if (!serialization::tryReadPod(file, wordLen)) return false;
  word.resize(wordLen);
  if (wordLen > 0 && file.read(&word[0], wordLen) != wordLen) return false;

  if (!serialization::tryReadPod(file, defOffset)) return false;
  if (!serialization::tryReadPod(file, defSize)) return false;
  return true;
}

uint32_t StarDictionary::lowerBound(const std::string& target) const {
  uint32_t lo = 0;
  uint32_t hi = cacheWordCount;
  std::string word;
  uint32_t defOffset, defSize;
  while (lo < hi) {
    const uint32_t mid = lo + (hi - lo) / 2;
    if (!readEntry(mid, word, defOffset, defSize)) return cacheWordCount;
    if (compareCaseInsensitive(word, target) < 0) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return lo;
}

bool StarDictionary::lookup(const std::string& word, std::string& definitionOut) {
  if (!cacheFile.isOpen() || !dictFile.isOpen()) return false;

  const uint32_t idx = lowerBound(word);
  std::string foundWord;
  uint32_t defOffset, defSize;
  if (idx >= cacheWordCount || !readEntry(idx, foundWord, defOffset, defSize)) return false;
  if (compareCaseInsensitive(foundWord, word) != 0) return false;

  if (defSize == 0 || defSize > MAX_DEFINITION_SIZE) return false;
  if (!dictFile.seek(defOffset)) return false;

  definitionOut.resize(defSize);
  const int readLen = dictFile.read(&definitionOut[0], defSize);
  if (readLen < 0 || static_cast<uint32_t>(readLen) != defSize) {
    definitionOut.clear();
    return false;
  }
  return true;
}

std::vector<std::string> StarDictionary::suggest(const std::string& prefix, const size_t maxResults) const {
  std::vector<std::string> results;
  if (!cacheFile.isOpen() || prefix.empty()) return results;

  results.reserve(std::min(maxResults, static_cast<size_t>(16)));

  uint32_t idx = lowerBound(prefix);
  std::string word;
  uint32_t defOffset, defSize;
  while (idx < cacheWordCount && results.size() < maxResults) {
    if (!readEntry(idx, word, defOffset, defSize)) break;
    if (word.size() < prefix.size() || compareCaseInsensitive(word.substr(0, prefix.size()), prefix) != 0) break;
    results.push_back(word);
    idx++;
  }
  return results;
}
