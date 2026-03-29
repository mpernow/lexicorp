#pragma once

#include "utils/Language.h"
#include <string>
#include <vector>

namespace db::models {
struct WordText {
  std::wstring word;
  int textHash = 0;
  int numOccurrences = 0;
  utils::Language language = utils::Language::Unknown;

  static constexpr const std::string tableName = "word_text";

  static std::vector<std::pair<std::string, std::string>> schema() {
    return {{"word", "TEXT NOT NULL"},
            {"text_hash", "INTEGER"},
            {"numOccurrences", "INTEGER"},
            {"language", "TEXT NOT NULL"},
            {"PRIMARY", "KEY(word, text_hash)"}};
  }
};
} // namespace db::models
