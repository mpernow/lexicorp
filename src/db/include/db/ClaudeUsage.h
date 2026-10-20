#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace db::models {
// What one Claude API request was billed for.
struct ClaudeUsage {
  std::int64_t id = 0;
  std::int64_t createdAt = 0; // Unix time, set by the repository
  std::string model;
  std::string purpose; // e.g. "define_word"
  std::int64_t inputTokens = 0;
  std::int64_t outputTokens = 0;
  std::int64_t cacheReadTokens = 0;
  std::int64_t cacheCreationTokens = 0;

  static constexpr const std::string tableName = "claude_usage";

  static std::vector<std::pair<std::string, std::string>> schema() {
    return {{"id", "INTEGER PRIMARY KEY"},
            {"created_at", "INTEGER NOT NULL"},
            {"model", "TEXT NOT NULL"},
            {"purpose", "TEXT NOT NULL"},
            {"input_tokens", "INTEGER NOT NULL DEFAULT 0"},
            {"output_tokens", "INTEGER NOT NULL DEFAULT 0"},
            {"cache_read_tokens", "INTEGER NOT NULL DEFAULT 0"},
            {"cache_creation_tokens", "INTEGER NOT NULL DEFAULT 0"}};
  }
};

// The requests of one model over a period, summed.
struct ClaudeUsageTotal {
  std::string model;
  std::int64_t requests = 0;
  std::int64_t inputTokens = 0;
  std::int64_t outputTokens = 0;
  std::int64_t cacheReadTokens = 0;
  std::int64_t cacheCreationTokens = 0;
};
} // namespace db::models
