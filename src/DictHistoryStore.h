#pragma once
#include <string>
#include <vector>

struct DictHistoryEntry {
  std::string word;
  std::string dictId;  // StarDictInfo::dirPath of the dictionary the lookup was made in
};

// Small ring-buffer of recent dictionary lookups, most-recent-first, capped at
// kMaxEntries. Persisted to /.crosspoint/dict_history.json.
class DictHistoryStore {
  static DictHistoryStore instance;
  std::vector<DictHistoryEntry> entries;

 public:
  static constexpr size_t kMaxEntries = 20;
  static constexpr const char* kPath = "/.crosspoint/dict_history.json";

  static DictHistoryStore& getInstance() { return instance; }

  const std::vector<DictHistoryEntry>& getEntries() const { return entries; }

  // Moves an existing entry to the front, or inserts a new one; trims to kMaxEntries. Persists.
  void addOrPromote(const std::string& word, const std::string& dictId);

  bool loadFromFile();
  bool saveToFile() const;
};

#define DICT_HISTORY DictHistoryStore::getInstance()
