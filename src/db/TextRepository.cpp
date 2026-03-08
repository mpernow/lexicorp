#include "db/TextRepository.h"
#include "db/Utils.h"
#include "sqlite/command.hpp"
#include "sqlite/execute.hpp"
#include "sqlite/query.hpp"
#include "sqlite/transaction.hpp"
#include "utils/Language.h"

namespace db {

TextRepository::TextRepository(sqlite::connection &dbConn) : mDbConn(dbConn) {
  sqlite::execute(mDbConn, Utils::createTableSql<models::Text>(), true);
}

TextRepository::~TextRepository() = default;

void TextRepository::add(const models::Text &text) {
  sqlite::command insert{mDbConn,
                         "INSERT INTO text(text, hash, language, source) "
                         "VALUES(?, ?, ?, ?)"};

  insert % Utils::toUtf8(text.text) % text.hash %
      utils::to_string(text.language) % Utils::toUtf8(text.source);
  insert();
}

std::optional<models::Text> TextRepository::getByHash(const int hash) {
  sqlite::query q{mDbConn, "SELECT text, hash, language, source "
                           "FROM text WHERE hash = ? "
                           "LIMIT 1"};

  q.bind(1, hash);

  for (auto &row : q.each()) {
    return models::Text{
        .text = Utils::fromUtf8(row.get<std::string>(0)),
        .hash = row.get<int>(1),
        .language = utils::language_from_string(row.get<std::string>(2)),
        .source = Utils::fromUtf8(row.get<std::string>(3)),
    };
  }
  return std::nullopt;
}

std::vector<models::Text> TextRepository::getAll() {
  sqlite::query q{mDbConn, "SELECT text, hash, language, source FROM text"};
  sqlite::result_type res = q.get_result();

  std::vector<models::Text> text;

  while (res->next_row()) {
    text.emplace_back(models::Text{
        Utils::fromUtf8(res->get<std::string>(0)), res->get<int>(1),
        utils::language_from_string(res->get<std::string>(2)),
        Utils::fromUtf8(res->get<std::string>(3))});
  }

  return text;
}

} // namespace db
