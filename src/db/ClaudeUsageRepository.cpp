#include "db/ClaudeUsageRepository.h"
#include "db/Utils.h"
#include "sqlite/command.hpp"
#include "sqlite/execute.hpp"
#include "sqlite/query.hpp"

namespace db {

ClaudeUsageRepository::ClaudeUsageRepository(sqlite::connection &dbConn)
    : mDbConn(dbConn) {
  sqlite::execute(mDbConn, Utils::createTableSql<models::ClaudeUsage>(), true);
}

ClaudeUsageRepository::~ClaudeUsageRepository() = default;

void ClaudeUsageRepository::add(const models::ClaudeUsage &usage) {
  sqlite::command insert{
      mDbConn, "INSERT INTO claude_usage(created_at, model, purpose, "
               "input_tokens, output_tokens, cache_read_tokens, "
               "cache_creation_tokens) "
               "VALUES(CAST(strftime('%s', 'now') AS INTEGER), "
               "?, ?, ?, ?, ?, ?)"};

  insert % usage.model % usage.purpose % usage.inputTokens %
      usage.outputTokens % usage.cacheReadTokens % usage.cacheCreationTokens;
  insert();
}

std::vector<models::ClaudeUsageTotal>
ClaudeUsageRepository::totalsSince(const std::int64_t since) {
  sqlite::query q{mDbConn,
                  "SELECT model, COUNT(*), SUM(input_tokens), "
                  "SUM(output_tokens), SUM(cache_read_tokens), "
                  "SUM(cache_creation_tokens) FROM claude_usage "
                  "WHERE created_at >= ? GROUP BY model ORDER BY model"};
  q.bind(1, since);

  std::vector<models::ClaudeUsageTotal> results;
  for (auto &row : q.each())
    results.push_back(models::ClaudeUsageTotal{
        .model = row.get<std::string>(0),
        .requests = row.get<std::int64_t>(1),
        .inputTokens = row.get<std::int64_t>(2),
        .outputTokens = row.get<std::int64_t>(3),
        .cacheReadTokens = row.get<std::int64_t>(4),
        .cacheCreationTokens = row.get<std::int64_t>(5),
    });
  return results;
}

} // namespace db
