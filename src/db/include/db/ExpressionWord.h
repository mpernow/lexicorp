#pragma once

#include "utils/Language.h"
#include <string>
#include <vector>

namespace db::models {
// Links a word to an idiom, fixed expression or compound verb it is part of,
// e.g. "پێم" and "وایە" to "پێم وایە" ("I think"), or "تاوتوێی" and "کرد" to
// "تاوتوێ کردن" ("to discuss"). The expression's gloss is a `dictionary` entry
// with its dictionary form (words normalised like `words`, joined by single
// spaces) as headword.
struct ExpressionWord {
  std::wstring word;
  utils::Language language = utils::Language::Unknown;
  std::wstring expression;

  static constexpr const std::string tableName = "expression_word";

  static std::vector<std::pair<std::string, std::string>> schema() {
    return {{"word", "TEXT NOT NULL"},
            {"language", "TEXT NOT NULL"},
            {"expression", "TEXT NOT NULL"},
            {"PRIMARY", "KEY(word, language, expression)"}};
  }
};
} // namespace db::models
