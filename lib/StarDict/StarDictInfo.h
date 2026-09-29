#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Metadata for one StarDict dictionary found on the SD card, following the
// layout documented in FEATURE spec: /dictionaries/<lang>/<name>.ifo + .idx + .dict [+ .syn]
struct StarDictInfo {
  std::string dirPath;  // e.g. "/dictionaries/en-en"
  std::string ifoPath;
  std::string idxPath;
  std::string dictPath;
  std::string synPath;   // empty if no .syn present
  std::string bookname;  // from .ifo "bookname="
  uint32_t wordCount = 0;

  // Stable identifier used for settings (dictionaryActive) and history entries.
  const std::string& id() const { return dirPath; }
};

namespace StarDict {

// Scans rootDir for subfolders containing a *.ifo file with a matching plain
// (uncompressed) .dict file, and returns the ones that are usable. Dictionaries
// shipped only as .dict.dz (dictzip-compressed) are skipped — see USER_GUIDE.md.
std::vector<StarDictInfo> discoverDictionaries(const std::string& rootDir = "/dictionaries");

// Parses one .ifo file (plain "key=value" text) into `outInfo`. `outInfo.dirPath`/
// `ifoPath`/`idxPath`/`dictPath`/`synPath` must already be populated by the caller;
// this only fills bookname/wordCount from the file contents.
bool parseIfo(const std::string& ifoPath, StarDictInfo& outInfo);

}  // namespace StarDict
