#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace db::models {
// Claude's explanation of a sentence, kept so each sentence is paid for
// once, even when it occurs in several texts.
struct Explanation {
  // TextProcessor::hashWstring of the sentence, as stored in `sentence`.
  std::int64_t sentenceHash = 0;
  std::wstring sentence;
  std::string answerJson;     // claude::SentenceExplanation::answerJson
  std::string model;          // the one that answered
  std::int64_t createdAt = 0; // Unix time, set by the repository

  static constexpr const std::string tableName = "explanation";

  static std::vector<std::pair<std::string, std::string>> schema() {
    return {{"sentence_hash", "INTEGER PRIMARY KEY"},
            {"sentence", "TEXT NOT NULL"},
            {"answer", "TEXT NOT NULL"},
            {"model", "TEXT NOT NULL"},
            {"created_at", "INTEGER NOT NULL"}};
  }
};
} // namespace db::models
