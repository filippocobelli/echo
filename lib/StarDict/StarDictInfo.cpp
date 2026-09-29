#include "StarDictInfo.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>

namespace {
std::string stemWithoutExtension(const std::string& path, const char* extension) {
  const size_t extLen = strlen(extension);
  if (path.size() >= extLen) {
    return path.substr(0, path.size() - extLen);
  }
  return path;
}
}  // namespace

namespace StarDict {

bool parseIfo(const std::string& ifoPath, StarDictInfo& outInfo) {
  const String raw = Storage.readFile(ifoPath.c_str());
  if (raw.length() == 0) {
    LOG_ERR("StarDict", "Failed to read .ifo: %s", ifoPath.c_str());
    return false;
  }

  const std::string content{raw.c_str(), raw.length()};
  size_t pos = 0;
  while (pos < content.size()) {
    const size_t nl = content.find('\n', pos);
    const size_t lineLen = (nl == std::string::npos) ? content.size() - pos : nl - pos;
    std::string line = content.substr(pos, lineLen);
    if (!line.empty() && line.back() == '\r') line.pop_back();

    const size_t eq = line.find('=');
    if (eq != std::string::npos) {
      const std::string key = line.substr(0, eq);
      const std::string value = line.substr(eq + 1);
      if (key == "bookname") {
        outInfo.bookname = value;
      } else if (key == "wordcount") {
        outInfo.wordCount = static_cast<uint32_t>(strtoul(value.c_str(), nullptr, 10));
      } else if (key == "idxoffsetbits" && value != "32") {
        // We only support the common 32-bit offset format; documented in USER_GUIDE.md.
        LOG_ERR("StarDict", "Unsupported idxoffsetbits=%s in %s", value.c_str(), ifoPath.c_str());
        return false;
      }
    }

    pos = (nl == std::string::npos) ? content.size() : nl + 1;
  }

  if (outInfo.bookname.empty()) {
    outInfo.bookname = outInfo.dirPath;
  }
  return true;
}

std::vector<StarDictInfo> discoverDictionaries(const std::string& rootDir) {
  std::vector<StarDictInfo> results;

  auto root = Storage.open(rootDir.c_str());
  if (!root || !root.isDirectory()) {
    return results;
  }

  char dirName[256];
  for (auto dirEntry = root.openNextFile(); dirEntry; dirEntry = root.openNextFile()) {
    if (!dirEntry.isDirectory()) {
      dirEntry.close();
      continue;
    }
    dirEntry.getName(dirName, sizeof(dirName));
    const std::string subDirPath = rootDir + "/" + dirName;
    dirEntry.close();

    auto subDir = Storage.open(subDirPath.c_str());
    if (!subDir || !subDir.isDirectory()) {
      continue;
    }

    char fileName[256];
    for (auto file = subDir.openNextFile(); file; file = subDir.openNextFile()) {
      if (file.isDirectory()) {
        file.close();
        continue;
      }
      file.getName(fileName, sizeof(fileName));
      const std::string_view fileNameView{fileName};
      if (!FsHelpers::checkFileExtension(fileNameView, ".ifo")) {
        file.close();
        continue;
      }
      file.close();

      StarDictInfo info;
      info.dirPath = subDirPath;
      info.ifoPath = subDirPath + "/" + fileName;
      const std::string stem = stemWithoutExtension(info.ifoPath, ".ifo");
      info.idxPath = stem + ".idx";
      info.dictPath = stem + ".dict";
      const std::string synCandidate = stem + ".syn";

      if (!Storage.exists(info.idxPath.c_str())) {
        LOG_INF("StarDict", "Skipping %s: missing .idx", info.ifoPath.c_str());
        continue;
      }
      if (!Storage.exists(info.dictPath.c_str())) {
        // Only plain (uncompressed) .dict is supported; .dict.dz is not.
        LOG_INF("StarDict", "Skipping %s: no plain .dict found (only .dict.dz supported dictionaries are unsupported)",
                info.ifoPath.c_str());
        continue;
      }
      if (Storage.exists(synCandidate.c_str())) {
        info.synPath = synCandidate;
      }

      if (!parseIfo(info.ifoPath, info)) {
        continue;
      }

      results.push_back(std::move(info));
    }
    subDir.close();
  }
  root.close();

  return results;
}

}  // namespace StarDict
