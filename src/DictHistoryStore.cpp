#include "DictHistoryStore.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>

DictHistoryStore DictHistoryStore::instance;

void DictHistoryStore::addOrPromote(const std::string& word, const std::string& dictId) {
  const auto it = std::find_if(entries.begin(), entries.end(),
                               [&](const DictHistoryEntry& e) { return e.word == word && e.dictId == dictId; });
  if (it != entries.end()) {
    entries.erase(it);
  }
  entries.insert(entries.begin(), DictHistoryEntry{word, dictId});
  if (entries.size() > kMaxEntries) {
    entries.resize(kMaxEntries);
  }
  saveToFile();
}

bool DictHistoryStore::loadFromFile() {
  entries.clear();
  if (!Storage.exists(kPath)) return true;

  const String raw = Storage.readFile(kPath);
  if (raw.length() == 0) return true;

  JsonDocument doc;
  const auto error = deserializeJson(doc, raw.c_str());
  if (error) {
    LOG_ERR("DictHistory", "JSON parse error: %s", error.c_str());
    return false;
  }

  const JsonArrayConst arr = doc["history"].as<JsonArrayConst>();
  for (JsonObjectConst obj : arr) {
    DictHistoryEntry entry;
    entry.word = obj["word"] | std::string("");
    entry.dictId = obj["dict"] | std::string("");
    if (!entry.word.empty()) entries.push_back(std::move(entry));
    if (entries.size() >= kMaxEntries) break;
  }
  return true;
}

bool DictHistoryStore::saveToFile() const {
  Storage.mkdir("/.crosspoint");

  JsonDocument doc;
  JsonArray arr = doc["history"].to<JsonArray>();
  for (const auto& entry : entries) {
    JsonObject obj = arr.add<JsonObject>();
    obj["word"] = entry.word;
    obj["dict"] = entry.dictId;
  }

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(kPath, json);
}
