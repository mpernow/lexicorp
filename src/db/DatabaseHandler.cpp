#include "db/DatabaseHandler.h"

namespace db {
DatabaseHandler::DatabaseHandler(std::string dbName)
    : mDbName(dbName), mConnection(mDbName) {}

DatabaseHandler::DatabaseHandler()
    : mDbName("lexiCorp.db"), mConnection(mDbName) {}

sqlite::connection &DatabaseHandler::getConnection() { return mConnection; }
} // namespace db
