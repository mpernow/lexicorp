#pragma once
#include "db/ClaudeUsage.h"

#include <cstdint>
#include <vector>

namespace db {
class IClaudeUsageRepository {
public:
  virtual ~IClaudeUsageRepository() = default;

  virtual void add(const models::ClaudeUsage &usage) = 0;
  // Per model, the requests made at or after `since` (Unix time), ordered by
  // model.
  virtual std::vector<models::ClaudeUsageTotal>
  totalsSince(const std::int64_t since) = 0;
  // Each request made at or after `since`, oldest first (for prices that
  // depend on a request's size).
  virtual std::vector<models::ClaudeUsage>
  requestsSince(const std::int64_t since) = 0;
};
} // namespace db
