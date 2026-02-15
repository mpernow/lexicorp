#include <sqlite/connection.hpp>
#include <sqlite/execute.hpp>

#include "db/DatabaseHandler.h"
#include "db/Utils.h"
#include "db/Word.h"

namespace db {
DatabaseHandler::DatabaseHandler() {
  sqlite::connection con("test.db");
  sqlite::execute(con, Utils::createTableSql<models::Word>(), true);
}
} // namespace db
