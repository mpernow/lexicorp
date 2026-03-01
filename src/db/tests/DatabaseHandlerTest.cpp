#include "db/DatabaseHandler.h"
#include <gtest/gtest.h>
#include <sqlite/connection.hpp>
#include <sqlite/query.hpp>
#include <string>

TEST(DatabaseHandlerTests, CreateDatabase) {
  std::string dbName = "test.db";
  {
    auto dbHandler = db::DatabaseHandler{dbName};
  }
  ASSERT_TRUE(std::filesystem::exists(dbName));

  // Clean up
  std::filesystem::remove(dbName);
}
