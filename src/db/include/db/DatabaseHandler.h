#pragma once
#include <string>

namespace db {
class DatabaseHandler {
public:
  DatabaseHandler();
  DatabaseHandler(std::string dbName);

private:
  void setUpDb();

  std::string mDbName;
};
} // namespace db
