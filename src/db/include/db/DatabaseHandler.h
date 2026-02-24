#pragma once
#include <sqlite/connection.hpp>
#include <string>

namespace db {

class DatabaseHandler {
public:
  explicit DatabaseHandler();
  explicit DatabaseHandler(std::string dbName);

  sqlite::connection &getConnection();

private:
  std::string mDbName;
  sqlite::connection mConnection;
};
} // namespace db
