#pragma once

#include "utils/Language.h"
#include <cstdint>
#include <string>
#include <vector>

namespace db::models {
// Join of `words` and `sentence`: how often a word occurs in a sentence.
struct WordSentence {
  std::wstring word;
  utils::Language language = utils::Language::Unknown;
  std::int64_t textHash = 0;
  int ordinal = 0;
  int numOccurrences = 0;

  static constexpr const std::string tableName = "word_sentence";

  static std::vector<std::pair<std::string, std::string>> schema() {
    return {{"word", "TEXT NOT NULL"},
            {"language", "TEXT NOT NULL"},
            {"text_hash", "INTEGER NOT NULL"},
            {"ordinal", "INTEGER NOT NULL"},
            {"numOccurrences", "INTEGER NOT NULL"},
            {"PRIMARY", "KEY(word, language, text_hash, ordinal)"}};
  }
};
} // namespace db::models
