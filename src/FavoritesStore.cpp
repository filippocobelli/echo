#include "FavoritesStore.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>

FavoritesStore FavoritesStore::instance;

bool FavoritesStore::contains(const std::string& path) const {
  return std::find(paths.begin(), paths.end(), path) != paths.end();
}

bool FavoritesStore::toggle(const std::string& path) {
  const auto it = std::find(paths.begin(), paths.end(), path);
  bool nowFavorited;
  if (it != paths.end()) {
    paths.erase(it);
    nowFavorited = false;
  } else {
    paths.push_back(path);
    nowFavorited = true;
  }
  saveToFile();
  return nowFavorited;
}

bool FavoritesStore::loadFromFile() {
  paths.clear();
  if (!Storage.exists(kPath)) return true;

  const String raw = Storage.readFile(kPath);
  if (raw.length() == 0) return true;

  JsonDocument doc;
  const auto error = deserializeJson(doc, raw.c_str());
  if (error) {
    LOG_ERR("Favorites", "JSON parse error: %s", error.c_str());
    return false;
  }

  const JsonArrayConst arr = doc["favorites"].as<JsonArrayConst>();
  for (JsonVariantConst v : arr) {
    const std::string path = v.as<std::string>();
    if (!path.empty()) paths.push_back(path);
  }
  return true;
}

bool FavoritesStore::saveToFile() const {
  Storage.mkdir("/.crosspoint");

  JsonDocument doc;
  JsonArray arr = doc["favorites"].to<JsonArray>();
  for (const auto& path : paths) {
    arr.add(path);
  }

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(kPath, json);
}
