#pragma once

#include <HalStorage.h>

#include <cstdint>
#include <string>
#include <vector>

#include "StarDictInfo.h"

// Opens one StarDict dictionary for lookup. Building the binary-search cache
// (.cpridx, see docs/file-formats.md) from the StarDict .idx is a one-time,
// streamed (bounded-RAM) operation; after that, word lookup and prefix
// suggestions are done via seeks into the cache + the plain .dict file,
// never loading the whole word list into RAM.
class StarDictionary {
 public:
  // Returns the on-disk cache path for a given dictionary (does not require open()).
  static std::string cachePathFor(const StarDictInfo& info);

  // Builds the .cpridx cache for `info` if missing or stale. Safe to call every
  // time before open() — it's a no-op if a valid cache already exists.
  bool buildCacheIfMissing(const StarDictInfo& info);

  // Opens the dictionary for lookup: loads the cache header and opens both the
  // cache and .dict files for random access. Call buildCacheIfMissing() first.
  bool open(const StarDictInfo& info);
  void close();

  uint32_t wordCount() const { return cacheWordCount; }

  // Case-insensitive exact lookup. Returns false if the word isn't found.
  bool lookup(const std::string& word, std::string& definitionOut);

  // Case-insensitive prefix search, returns up to maxResults words in sorted order.
  std::vector<std::string> suggest(const std::string& prefix, size_t maxResults) const;

 private:
  FsFile cacheFile;
  FsFile dictFile;
  uint32_t cacheWordCount = 0;
  static constexpr uint32_t HEADER_SIZE = 13;  // magic(4) + version(1) + wordCount(4) + lutOffset(4)

  bool readEntry(uint32_t index, std::string& word, uint32_t& defOffset, uint32_t& defSize) const;
  // Leftmost index whose word is >= target (case-insensitive), or wordCount() if none.
  uint32_t lowerBound(const std::string& target) const;
};
