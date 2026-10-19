#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace db::models {
// A sentence of a stored text; `ordinal` is its position among the text's
// sentences, `start` / `length` its place in the text (in code points).
struct Sentence {
  std::int64_t textHash = 0;
  int ordinal = 0;
  std::wstring sentence;
  int start = 0;
  int length = 0;

  static constexpr const std::string tableName = "sentence";

  static std::vector<std::pair<std::string, std::string>> schema() {
    return {{"text_hash", "INTEGER NOT NULL"},
            {"ordinal", "INTEGER NOT NULL"},
            {"sentence", "TEXT NOT NULL"},
            {"start", "INTEGER NOT NULL"},
            {"length", "INTEGER NOT NULL"},
            {"PRIMARY", "KEY(text_hash, ordinal)"}};
  }
};
} // namespace db::models
