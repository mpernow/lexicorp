#include "db/ExplanationRepository.h"
#include "db/Utils.h"
#include "sqlite/command.hpp"
#include "sqlite/execute.hpp"
#include "sqlite/query.hpp"

namespace db {

ExplanationRepository::ExplanationRepository(sqlite::connection &dbConn)
    : mDbConn(dbConn) {
  sqlite::execute(mDbConn, Utils::createTableSql<models::Explanation>(), true);
}

ExplanationRepository::~ExplanationRepository() = default;

void ExplanationRepository::save(const models::Explanation &explanation) {
  sqlite::command upsert{
      mDbConn, "INSERT INTO explanation(sentence_hash, sentence, answer, "
               "model, created_at) "
               "VALUES(?, ?, ?, ?, CAST(strftime('%s', 'now') AS INTEGER)) "
               "ON CONFLICT(sentence_hash) DO UPDATE SET "
               "sentence = excluded.sentence, answer = excluded.answer, "
               "model = excluded.model, created_at = excluded.created_at"};

  upsert % explanation.sentenceHash % Utils::toUtf8(explanation.sentence) %
      explanation.answerJson % explanation.model;
  upsert();
}

std::optional<models::Explanation>
ExplanationRepository::get(const std::int64_t sentenceHash) {
  sqlite::query q{mDbConn,
                  "SELECT sentence_hash, sentence, answer, model, created_at "
                  "FROM explanation WHERE sentence_hash = ? LIMIT 1"};
  q.bind(1, sentenceHash);

  for (auto &row : q.each())
    return models::Explanation{
        .sentenceHash = row.get<std::int64_t>(0),
        .sentence = Utils::fromUtf8(row.get<std::string>(1)),
        .answerJson = row.get<std::string>(2),
        .model = row.get<std::string>(3),
        .createdAt = row.get<std::int64_t>(4),
    };
  return std::nullopt;
}

} // namespace db
