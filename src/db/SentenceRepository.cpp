#include "db/SentenceRepository.h"
#include "db/Utils.h"
#include "sqlite/command.hpp"
#include "sqlite/execute.hpp"
#include "sqlite/query.hpp"
#include "utils/Language.h"

namespace db {

namespace {
// Columns in the order sentenceFromRow() reads them.
constexpr const char *kColumns = "s.text_hash, s.ordinal, s.sentence, "
                                 "s.start, s.length";

template <typename Row> models::Sentence sentenceFromRow(Row &row) {
  return models::Sentence{
      .textHash = row.template get<std::int64_t>(0),
      .ordinal = row.template get<int>(1),
      .sentence = Utils::fromUtf8(row.template get<std::string>(2)),
      .start = row.template get<int>(3),
      .length = row.template get<int>(4),
  };
}
} // namespace

SentenceRepository::SentenceRepository(sqlite::connection &dbConn)
    : mDbConn(dbConn) {
  sqlite::execute(mDbConn, Utils::createTableSql<models::Sentence>(), true);
}

SentenceRepository::~SentenceRepository() = default;

void SentenceRepository::add(const models::Sentence &sentence) {
  sqlite::command insert{
      mDbConn, "INSERT INTO sentence(text_hash, ordinal, sentence, start, "
               "length) VALUES(?, ?, ?, ?, ?)"};

  insert % sentence.textHash % sentence.ordinal %
      Utils::toUtf8(sentence.sentence) % sentence.start % sentence.length;
  insert();
}

std::vector<models::Sentence>
SentenceRepository::getByHash(const std::int64_t textHash) {
  sqlite::query q{mDbConn, std::string("SELECT ") + kColumns +
                               " FROM sentence s WHERE s.text_hash = ? "
                               "ORDER BY s.ordinal"};
  q.bind(1, textHash);

  std::vector<models::Sentence> results;
  for (auto &row : q.each())
    results.push_back(sentenceFromRow(row));
  return results;
}

std::vector<models::Sentence>
SentenceRepository::getByWord(const std::wstring &word,
                              const utils::Language language) {
  sqlite::query q{mDbConn,
                  std::string("SELECT ") + kColumns +
                      " FROM word_sentence ws JOIN sentence s "
                      "ON s.text_hash = ws.text_hash AND s.ordinal = "
                      "ws.ordinal "
                      "WHERE ws.word = ? AND ws.language = ? "
                      "ORDER BY s.text_hash, s.ordinal"};
  q.bind(1, Utils::toUtf8(word));
  q.bind(2, utils::to_string(language));

  std::vector<models::Sentence> results;
  for (auto &row : q.each())
    results.push_back(sentenceFromRow(row));
  return results;
}

void SentenceRepository::removeByHash(const std::int64_t textHash) {
  sqlite::command remove{mDbConn, "DELETE FROM sentence WHERE text_hash = ?"};
  remove % textHash;
  remove();
}

} // namespace db
