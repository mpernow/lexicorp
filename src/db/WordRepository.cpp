#include "db/WordRepository.h"
#include "db/Utils.h"
#include "sqlite/command.hpp"
#include "sqlite/execute.hpp"
#include "sqlite/query.hpp"

namespace db {

WordRepository::WordRepository(sqlite::connection &dbConn) : mDbConn(dbConn) {
  sqlite::execute(mDbConn, Utils::createTableSql<models::Word>(), true);
}

WordRepository::~WordRepository() = default;

void WordRepository::add(const models::Word &word) {
  sqlite::command insert{mDbConn,
                         "INSERT INTO words(word, numOccurences, known) "
                         "VALUES(?, ?, ?)"};

  insert % word.word % word.numOccurences % word.known;
}
std::optional<models::Word> WordRepository::getByText(const std::string &text) {
  sqlite::query q{mDbConn, "SELECT word, numOccurences, known "
                           "FROM words WHERE word = ?"
                           "LIMIT 1"};

  q % text;

  for (auto &row : q.each(text)) {
    return models::Word{.word = row.get<std::string>(0),
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
    words.emplace_back(models::Word{res->get<std::string>(0), res->get<int>(1),
                                    res->get<bool>(2)});
  }

  return words;
}

void WordRepository::update(const models::Word &word) {
  sqlite::command update{mDbConn, "UPDATE words "
                                  "SET numOccurences = :count, "
                                  "    known = :known "
                                  "WHERE word = :word"};

  update % sqlite::named(":count", word.numOccurences) %
      sqlite::named(":known", word.known) % sqlite::named(":word", word.word);
}

} // namespace db
