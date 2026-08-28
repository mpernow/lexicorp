#include "db/DatabaseHandler.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <sqlite/connection.hpp>
#include <sqlite/execute.hpp>
#include <sqlite/query.hpp>
#include <string>

TEST(DatabaseHandlerTests, CreateDatabase) {
  // Use a temp-dir path rather than the working directory, and clear any file
  // left behind by an earlier crashed run so this test is not flaky.
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "lexicorp_dbhandler_test.db";
  std::filesystem::remove(path);

  {
    auto dbHandler = db::DatabaseHandler{path.string()};
  }
  ASSERT_TRUE(std::filesystem::exists(path));

  std::filesystem::remove(path);
}

TEST(DatabaseHandlerTests, ConnectionIsUsable) {
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};

  sqlite::connection &conn = dbHandler.getConnection();
  sqlite::execute(conn, "CREATE TABLE t(x INTEGER)", true);
  sqlite::execute(conn, "INSERT INTO t(x) VALUES(42)", true);

  sqlite::query q{conn, "SELECT x FROM t"};
  auto res = q.get_result();
  ASSERT_TRUE(res->next_row());
  EXPECT_EQ(res->get<int>(0), 42);
}

// The default constructor is what production uses (via AppContext). It must open
// a usable connection against "lexiCorp.db" in the working directory.
TEST(DatabaseHandlerTests, DefaultConstructorOpensDefaultFile) {
  const std::filesystem::path defaultPath = "lexiCorp.db";
  const bool preexisting = std::filesystem::exists(defaultPath);

  {
    auto dbHandler = db::DatabaseHandler{};
    sqlite::execute(dbHandler.getConnection(),
                    "CREATE TABLE IF NOT EXISTS probe(x INTEGER)", true);
  }

  EXPECT_TRUE(std::filesystem::exists(defaultPath));

  // Only remove the file if this test created it.
  if (!preexisting) {
    std::filesystem::remove(defaultPath);
  }
}
