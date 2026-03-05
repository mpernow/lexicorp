#pragma once

#include "utils/Language.h"
#include <string>
#include <vector>

namespace db::models {
struct Word {
  std::wstring word;
  int numOccurrences = 0;
  bool known = false;
  utils::Language language = utils::Language::Unknown;

  static constexpr const std::string tableName = "words";

  static std::vector<std::pair<std::string, std::string>> schema() {
    return {{"word", "TEXT NOT NULL"},
            {"numOccurrences", "INTEGER NOT NULL"},
            {"known", "INTEGER NOT NULL"},
            {"language", "TEXT NOT NULL"},
            {"PRIMARY", "KEY(word, language)"}};
  }
};
} // namespace db::models
