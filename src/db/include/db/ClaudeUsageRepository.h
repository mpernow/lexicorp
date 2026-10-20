#pragma once
#include "sqlite/connection.hpp"

#include "db/ClaudeUsage.h"
#include "db/IClaudeUsageRepository.h"

namespace db {
class ClaudeUsageRepository : public IClaudeUsageRepository {
public:
  ClaudeUsageRepository(sqlite::connection &dbConn);
  ~ClaudeUsageRepository();

  void add(const models::ClaudeUsage &usage) override;
  std::vector<models::ClaudeUsageTotal>
  totalsSince(const std::int64_t since) override;

private:
  sqlite::connection &mDbConn;
};

} // namespace db
