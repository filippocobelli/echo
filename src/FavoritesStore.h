#pragma once
#include <string>
#include <vector>

// Favorite books, identified by full SD-card path, persisted to
// /.crosspoint/favorites.json (flat array of paths).
class FavoritesStore {
  static FavoritesStore instance;
  std::vector<std::string> paths;

 public:
  static constexpr const char* kPath = "/.crosspoint/favorites.json";

  static FavoritesStore& getInstance() { return instance; }

  bool contains(const std::string& path) const;
  // Adds if absent, removes if present. Persists. Returns the new state (true = now favorited).
  bool toggle(const std::string& path);

  bool loadFromFile();
  bool saveToFile() const;
};

#define FAVORITES FavoritesStore::getInstance()
