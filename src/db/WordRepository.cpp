#include "db/WordRepository.h"
#include "db/Utils.h"
#include "sqlite/command.hpp"
#include "sqlite/execute.hpp"
#include "sqlite/query.hpp"
#include "sqlite/transaction.hpp"
#include <iostream>

namespace db {

WordRepository::WordRepository(sqlite::connection &dbConn) : mDbConn(dbConn) {
  sqlite::execute(mDbConn, Utils::createTableSql<models::Word>(), true);
}

WordRepository::~WordRepository() = default;

void WordRepository::add(const models::Word &word) {
  sqlite::command insert{mDbConn,
                         "INSERT INTO words(word, numOccurences, known) "
                         "VALUES(?, ?, ?)"};

  insert % Utils::toUtf8(word.word) % word.numOccurences % word.known;
  insert();
}
std::optional<models::Word>
WordRepository::getByText(const std::wstring &text) {
  sqlite::query q{mDbConn, "SELECT word, numOccurences, known "
                           "FROM words WHERE word = ? "
                           "LIMIT 1"};

  std::wstring word;
  for (auto &row : q.each(Utils::toUtf8(text))) {
    return models::Word{.word = Utils::fromUtf8(row.get<std::string>(0)),
                        .numOccurences = row.get<int>(1),
                        .known = static_cast<bool>(row.get<int>(2))};
  }
  return std::nullopt;
}

std::vector<models::Word> WordRepository::getAll() {
  sqlite::query q{mDbConn, "SELECT word, numOccurences, known FROM words"};
  sqlite::result_type res = q.get_result();

  std::vector<models::Word> words;

  while (res->next_row()) {
    words.emplace_back(models::Word{Utils::fromUtf8(res->get<std::string>(0)),
                                    res->get<int>(1), res->get<bool>(2)});
  }

  return words;
}

void WordRepository::update(const models::Word &word) {
  sqlite::command update{mDbConn, "UPDATE words "
                                  "SET numOccurences = :count, "
                                  "    known = :known "
                                  "WHERE word = :word"};

  update % sqlite::named(":count", word.numOccurences) %
      sqlite::named(":known", word.known) %
      sqlite::named(":word", Utils::toUtf8(word.word));
  update();
}

void WordRepository::updateFrequencies(
    const std::unordered_map<std::wstring, int> &frequencies) {

  auto tx = sqlite::transaction{mDbConn};

  for (const auto &[word, freq] : frequencies) {
    std::string utf8Word = Utils::toUtf8(word);

    // Create command locally inside the loop
    // TODO: This is needed due to some lifetime issue, investigate!
    sqlite::command upsert{
        mDbConn,
        "INSERT INTO words(word, numOccurences, known) "
        "VALUES(?, ?, 0) "
        "ON CONFLICT(word) DO UPDATE "
        "SET numOccurences = words.numOccurences + excluded.numOccurences"};

    upsert % utf8Word % freq;
    upsert();
  }

  tx.commit();
}

} // namespace db
