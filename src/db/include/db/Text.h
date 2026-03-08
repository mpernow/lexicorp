#pragma once

#include "utils/Language.h"
#include <string>
#include <vector>

namespace db::models {
struct Text {
  std::wstring text;
  int hash = 0;
  utils::Language language = utils::Language::Unknown;
  std::wstring source;

  static constexpr const std::string tableName = "text";

  static std::vector<std::pair<std::string, std::string>> schema() {
    return {{"text", "TEXT NOT NULL"},
            {"hash", "INTEGER PRIMARY KEY"},
            {"language", "TEXT NOT NULL"},
            {"source", "TEXT NOT NULL"}};
  }
};
} // namespace db::models
