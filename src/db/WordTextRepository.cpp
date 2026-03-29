#include "db/WordTextRepository.h"
#include "db/Utils.h"
#include "sqlite/command.hpp"
#include "sqlite/execute.hpp"
#include "sqlite/query.hpp"
#include "sqlite/transaction.hpp"
#include "utils/Language.h"

namespace db {

WordTextRepository::WordTextRepository(sqlite::connection &dbConn)
    : mDbConn(dbConn) {
  sqlite::execute(mDbConn, Utils::createTableSql<models::WordText>(), true);
}

WordTextRepository::~WordTextRepository() = default;

void WordTextRepository::add(const models::WordText &wordText) {
  sqlite::command insert{
      mDbConn,
      "INSERT INTO word_text(word, text_hash, numOccurrences, language) "
      "VALUES(?, ?, ?, ?)"};

  insert % Utils::toUtf8(wordText.word) % wordText.textHash %
      wordText.numOccurrences % utils::to_string(wordText.language);
  insert();
}

std::optional<models::WordText>
WordTextRepository::getByHash(const int textHash,
                              const utils::Language language) {
  sqlite::query q{mDbConn,
                  "SELECT word, text_hash, language "
                  "FROM word_text WHERE text_hash = ? AND language = ? "
                  "LIMIT 1"};

  q.bind(1, textHash);
  q.bind(2, utils::to_string(language));

  for (auto &row : q.each()) {
    return models::WordText{
        .word = Utils::fromUtf8(row.get<std::string>(0)),
        .textHash = row.get<int>(1),
        .numOccurrences = row.get<int>(2),
        .language = utils::language_from_string(row.get<std::string>(3)),
    };
  }
  return std::nullopt;
}

std::optional<models::WordText>
WordTextRepository::getByWord(const std::wstring &word,
                              const utils::Language language) {
  sqlite::query q{mDbConn, "SELECT word, text_hash, language "
                           "FROM word_text WHERE word = ? AND language = ? "
                           "LIMIT 1"};

  q.bind(1, Utils::toUtf8(word));
  q.bind(2, utils::to_string(language));

  for (auto &row : q.each()) {
    return models::WordText{
        .word = Utils::fromUtf8(row.get<std::string>(0)),
        .textHash = row.get<int>(1),
        .numOccurrences = row.get<int>(2),
        .language = utils::language_from_string(row.get<std::string>(3)),
    };
  }
  return std::nullopt;
}
} // namespace db
