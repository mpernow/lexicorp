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

  std::string getSchemaCommand =
      "SELECT sql FROM sqlite_schema WHERE name = 'Word';";

  sqlite::connection con(dbName);
  sqlite::query q(con, getSchemaCommand);
  sqlite::result_type res = q.get_result();

  std::string schema;
  std::string expectedSchema = "CREATE TABLE Word(word TEXT PRIMARY KEY, "
                               "numOccurences INTEGER, known INTEGER)";
  while (res->next_row()) {
    schema = res->get<std::string>(0);
  }
  EXPECT_EQ(schema, expectedSchema);

  // Clean up
  std::filesystem::remove(dbName);
}
