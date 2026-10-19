#pragma once

#include "utils/Language.h"
#include <string>
#include <vector>

namespace db::models {
// An inflected or alternative form of a dictionary headword (its lemma), e.g.
// a conjugated verb. Both are stored normalised like the words in `words`.
// Lets a word without an entry of its own borrow its lemma's gloss.
struct WordForm {
  std::wstring form;
  utils::Language language = utils::Language::Unknown;
  std::wstring lemma;
  std::wstring tags; // space-separated, e.g. "past singular"

  static constexpr const std::string tableName = "word_form";

  static std::vector<std::pair<std::string, std::string>> schema() {
    return {{"form", "TEXT NOT NULL"},
            {"language", "TEXT NOT NULL"},
            {"lemma", "TEXT NOT NULL"},
            {"tags", "TEXT NOT NULL DEFAULT ''"},
            {"PRIMARY", "KEY(form, language, lemma)"}};
  }
};
} // namespace db::models
