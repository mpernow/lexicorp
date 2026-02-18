#include <sqlite/connection.hpp>
#include <sqlite/execute.hpp>

#include "db/DatabaseHandler.h"
#include "db/Utils.h"
#include "db/Word.h"

namespace db {
DatabaseHandler::DatabaseHandler(std::string dbName) : mDbName(dbName) {
  setUpDb();
}

DatabaseHandler::DatabaseHandler() : mDbName("lexiCorp.db") { setUpDb(); }

void DatabaseHandler::setUpDb() {
  sqlite::connection con(mDbName);
  sqlite::execute(con, Utils::createTableSql<models::Word>(), true);
}
} // namespace db
