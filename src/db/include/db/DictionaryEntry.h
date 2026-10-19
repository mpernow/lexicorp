#pragma once

#include "utils/Language.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace db::models {
// Where a gloss came from, in order of precedence: a manual entry wins over
// every other source.
enum class DictionarySource { Manual, Wiktionary, Claude, Google };

inline std::string to_string(DictionarySource source) {
  switch (source) {
  case DictionarySource::Manual:
    return "manual";
  case DictionarySource::Wiktionary:
    return "wiktionary";
  case DictionarySource::Claude:
    return "claude";
  case DictionarySource::Google:
    return "google";
  }
  return "manual";
}

inline std::optional<DictionarySource>
dictionary_source_from_string(const std::string &str) {
  for (auto source : {DictionarySource::Manual, DictionarySource::Wiktionary,
                      DictionarySource::Claude, DictionarySource::Google})
    if (to_string(source) == str)
      return source;
  return std::nullopt;
}

// A gloss for a headword (stored normalised like the words in `words`). A
// headword can have several entries: one per sense and source.
struct DictionaryEntry {
  std::int64_t id = 0;
  std::wstring headword;
  utils::Language language = utils::Language::Unknown;
  std::wstring gloss;
  std::wstring partOfSpeech;
  DictionarySource source = DictionarySource::Manual;
  bool aiGenerated = false;
  std::int64_t updatedAt = 0; // Unix time, set by the repository

  static constexpr const std::string tableName = "dictionary";

  static std::vector<std::pair<std::string, std::string>> schema() {
    return {{"id", "INTEGER PRIMARY KEY"},
            {"headword", "TEXT NOT NULL"},
            {"language", "TEXT NOT NULL"},
            {"gloss", "TEXT NOT NULL"},
            {"part_of_speech", "TEXT NOT NULL DEFAULT ''"},
            {"source", "TEXT NOT NULL"},
            {"ai_generated", "INTEGER NOT NULL DEFAULT 0"},
            {"updated_at", "INTEGER NOT NULL"}};
  }
};
} // namespace db::models
