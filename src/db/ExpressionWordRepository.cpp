#include "db/ExpressionWordRepository.h"
#include "db/Utils.h"
#include "sqlite/command.hpp"
#include "sqlite/execute.hpp"
#include "sqlite/query.hpp"
#include "utils/Language.h"

namespace db {

ExpressionWordRepository::ExpressionWordRepository(sqlite::connection &dbConn)
    : mDbConn(dbConn) {
  sqlite::execute(mDbConn, Utils::createTableSql<models::ExpressionWord>(),
                  true);
}

ExpressionWordRepository::~ExpressionWordRepository() = default;

void ExpressionWordRepository::add(
    const models::ExpressionWord &expressionWord) {
  sqlite::command insert{mDbConn,
                         "INSERT OR IGNORE INTO expression_word(word, "
                         "language, expression) VALUES(?, ?, ?)"};

  insert % Utils::toUtf8(expressionWord.word) %
      utils::to_string(expressionWord.language) %
      Utils::toUtf8(expressionWord.expression);
  insert();
}

std::vector<std::wstring>
ExpressionWordRepository::getByWord(const std::wstring &word,
                                    const utils::Language language) {
  sqlite::query q{mDbConn, "SELECT expression FROM expression_word "
                           "WHERE word = ? AND language = ? "
                           "ORDER BY expression"};
  q.bind(1, Utils::toUtf8(word));
  q.bind(2, utils::to_string(language));

  std::vector<std::wstring> results;
  for (auto &row : q.each())
    results.push_back(Utils::fromUtf8(row.get<std::string>(0)));
  return results;
}

std::vector<std::wstring>
ExpressionWordRepository::getWords(const std::wstring &expression,
                                   const utils::Language language) {
  sqlite::query q{mDbConn, "SELECT word FROM expression_word "
                           "WHERE expression = ? AND language = ? "
                           "ORDER BY word"};
  q.bind(1, Utils::toUtf8(expression));
  q.bind(2, utils::to_string(language));

  std::vector<std::wstring> results;
  for (auto &row : q.each())
    results.push_back(Utils::fromUtf8(row.get<std::string>(0)));
  return results;
}

} // namespace db
