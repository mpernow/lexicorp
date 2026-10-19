#include "db/WordSentenceRepository.h"
#include "db/Utils.h"
#include "sqlite/command.hpp"
#include "sqlite/execute.hpp"
#include "sqlite/query.hpp"
#include "utils/Language.h"

namespace db {

WordSentenceRepository::WordSentenceRepository(sqlite::connection &dbConn)
    : mDbConn(dbConn) {
  sqlite::execute(mDbConn, Utils::createTableSql<models::WordSentence>(),
                  true);
  // The primary key starts with the word; deleting a text looks rows up by
  // text instead.
  sqlite::execute(mDbConn,
                  "CREATE INDEX IF NOT EXISTS word_sentence_text_hash "
                  "ON word_sentence(text_hash)",
                  true);
}

WordSentenceRepository::~WordSentenceRepository() = default;

void WordSentenceRepository::add(const models::WordSentence &wordSentence) {
  sqlite::command insert{
      mDbConn, "INSERT INTO word_sentence(word, language, text_hash, "
               "ordinal, numOccurrences) VALUES(?, ?, ?, ?, ?)"};

  insert % Utils::toUtf8(wordSentence.word) %
      utils::to_string(wordSentence.language) % wordSentence.textHash %
      wordSentence.ordinal % wordSentence.numOccurrences;
  insert();
}

std::vector<models::WordSentence>
WordSentenceRepository::getByHash(const std::int64_t textHash) {
  sqlite::query q{mDbConn, "SELECT word, language, text_hash, ordinal, "
                           "numOccurrences FROM word_sentence "
                           "WHERE text_hash = ? ORDER BY ordinal, word"};
  q.bind(1, textHash);

  std::vector<models::WordSentence> results;
  for (auto &row : q.each()) {
    results.push_back(models::WordSentence{
        .word = Utils::fromUtf8(row.get<std::string>(0)),
        .language = utils::language_from_string(row.get<std::string>(1)),
        .textHash = row.get<std::int64_t>(2),
        .ordinal = row.get<int>(3),
        .numOccurrences = row.get<int>(4),
    });
  }
  return results;
}

void WordSentenceRepository::removeByHash(const std::int64_t textHash) {
  sqlite::command remove{mDbConn,
                         "DELETE FROM word_sentence WHERE text_hash = ?"};
  remove % textHash;
  remove();
}

void WordSentenceRepository::removeByWord(const std::wstring &word,
                                          const utils::Language language) {
  sqlite::command remove{
      mDbConn, "DELETE FROM word_sentence WHERE word = ? AND language = ?"};
  remove % Utils::toUtf8(word) % utils::to_string(language);
  remove();
}

} // namespace db
