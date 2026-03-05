#include "db/WordRepository.h"
#include "db/Utils.h"
#include "sqlite/command.hpp"
#include "sqlite/execute.hpp"
#include "sqlite/query.hpp"
#include "sqlite/transaction.hpp"
#include "utils/Language.h"

namespace db {

WordRepository::WordRepository(sqlite::connection &dbConn) : mDbConn(dbConn) {
  sqlite::execute(mDbConn, Utils::createTableSql<models::Word>(), true);
}

WordRepository::~WordRepository() = default;

void WordRepository::add(const models::Word &word) {
  sqlite::command insert{
      mDbConn, "INSERT INTO words(word, numOccurrences, known, language) "
               "VALUES(?, ?, ?, ?)"};

  insert % Utils::toUtf8(word.word) % word.numOccurrences % word.known %
      utils::to_string(word.language);
  insert();
}

std::optional<models::Word>
WordRepository::getByText(const std::wstring &text,
                          const utils::Language &language) {
  sqlite::query q{mDbConn, "SELECT word, numOccurrences, known, language "
                           "FROM words WHERE word = ? AND language = ? "
                           "LIMIT 1"};

  q.bind(1, Utils::toUtf8(text));
  q.bind(2, utils::to_string(language));

  for (auto &row : q.each()) {
    return models::Word{
        .word = Utils::fromUtf8(row.get<std::string>(0)),
        .numOccurrences = row.get<int>(1),
        .known = static_cast<bool>(row.get<int>(2)),
        .language = utils::language_from_string(row.get<std::string>(3))};
  }
  return std::nullopt;
}

std::vector<models::Word> WordRepository::getAll() {
  sqlite::query q{mDbConn,
                  "SELECT word, numOccurrences, known, language FROM words"};
  sqlite::result_type res = q.get_result();

  std::vector<models::Word> words;

  while (res->next_row()) {
    words.emplace_back(
        models::Word{Utils::fromUtf8(res->get<std::string>(0)),
                     res->get<int>(1), res->get<bool>(2),
                     utils::language_from_string(res->get<std::string>(3))});
  }

  return words;
}

void WordRepository::update(const models::Word &word) {
  sqlite::command update{mDbConn, "UPDATE words "
                                  "SET numOccurrences = :count, "
                                  "    known = :known "
                                  "WHERE word = :word "
                                  "AND language = :language"};

  update % sqlite::named(":count", word.numOccurrences) %
      sqlite::named(":known", word.known) %
      sqlite::named(":word", Utils::toUtf8(word.word)) %
      sqlite::named(":language", utils::to_string(word.language));
  update();
}

void WordRepository::updateFrequencies(
    const std::unordered_map<std::wstring, int> &frequencies,
    const utils::Language &language) {

  auto tx = sqlite::transaction{mDbConn};

  for (const auto &[word, freq] : frequencies) {
    std::string utf8Word = Utils::toUtf8(word);

    // Create command locally inside the loop
    // TODO: This is needed due to some lifetime issue, investigate!
    sqlite::command upsert{
        mDbConn,
        "INSERT INTO words(word, numOccurrences, known, language) "
        "VALUES(?, ?, 0, ?) "
        "ON CONFLICT(word, language) DO UPDATE "
        "SET numOccurrences = words.numOccurrences + excluded.numOccurrences"};

    upsert % utf8Word % freq % utils::to_string(language);
    upsert();
  }

  tx.commit();
}

} // namespace db
