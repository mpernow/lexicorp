#include "db/DatabaseHandler.h"

namespace db {
namespace {
// vsqlite++ caches finished statements without resetting them, so a query
// left before its last row (e.g. returning the first match) would keep its
// read lock for as long as the connection lives: each session has one, and
// other sessions and tools/import_wiktionary.py then can't write.
void disableStatementCache(sqlite::connection &connection) {
  connection.configure_statement_cache({.capacity = 0});
}
} // namespace

DatabaseHandler::DatabaseHandler(std::string dbName)
    : mDbName(dbName), mConnection(mDbName) {
  disableStatementCache(mConnection);
}

DatabaseHandler::DatabaseHandler()
    : mDbName("lexiCorp.db"), mConnection(mDbName) {
  disableStatementCache(mConnection);
}

sqlite::connection &DatabaseHandler::getConnection() { return mConnection; }
} // namespace db
