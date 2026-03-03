#pragma once

#include <string>
#include <vector>

namespace db::models {
struct Word {
  std::wstring word;
  int numOccurences = 0;
  bool known = false;

  static constexpr const std::string tableName = "words";

  static std::vector<std::pair<std::string, std::string>> schema() {
    return {{"word", "TEXT PRIMARY KEY"},
            {"numOccurences", "INTEGER NOT NULL"},
            {"known", "INTEGER NOT NULL"}};
  }
};
} // namespace db::models
