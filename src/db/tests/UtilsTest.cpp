#include "db/Utils.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>

struct TestDbModel {
  std::string field1;
  int field2 = 0;
  bool field3 = false;

  static constexpr const std::string tableName = "TestTableName";

  static std::vector<std::pair<std::string, std::string>> schema() {
    return {{"field1", "TEXT PRIMARY KEY"},
            {"field2", "INTEGER"},
            {"field3", "INTEGER"}};
  }
};

struct NotADbModel {
  std::string field1;
};

struct TestDbModelWrongTableName {
  std::string field1;

  static constexpr const int tableName = 1;

  static std::vector<std::pair<std::string, std::string>> schema() {
    return {{"field1", "TEXT PRIMARY KEY"}};
  }
};

struct TestDbModelWrongSchema {
  std::string field1;

  static constexpr const std::string tableName = "TestTableName";

  static std::vector<std::string> schema() {
    return {"field1", "TEXT PRIMARY KEY"};
  }
};

TEST(dbUtilsSchemaTests, CreateSqlSchema) {
  EXPECT_EQ(db::Utils::createTableSql<TestDbModel>(),
            "CREATE TABLE IF NOT EXISTS TestTableName(field1 TEXT PRIMARY KEY, "
            "field2 INTEGER, field3 INTEGER);");
}

TEST(dbUtilsTemplateConceptTests, Pass) {
  static_assert(db::Utils::IsDbModel<TestDbModel>);
}

TEST(dbUtilsTemplateConceptTests, MissingNameAndSchema) {
  static_assert(!db::Utils::IsDbModel<NotADbModel>);
}

TEST(dbUtilsTemplateConceptTests, WrongTableName) {
  static_assert(!db::Utils::IsDbModel<TestDbModelWrongTableName>);
}

TEST(dbUtilsTemplateConceptTests, WrongSchema) {
  static_assert(!db::Utils::IsDbModel<TestDbModelWrongSchema>);
}

TEST(UnicodeConversionTests, ToUtf8Only) {
  std::wstring original = L"café naïve résumé";

  std::string utf8 = db::Utils::toUtf8(original);

  const std::string expectedUtf8(
      reinterpret_cast<const char *>(u8"café naïve résumé"));

  EXPECT_EQ(utf8, expectedUtf8);
}

TEST(UnicodeConversionTests, ToUtf8Kurdish) {
  std::wstring original = L"چاوە";

  std::string utf8 = db::Utils::toUtf8(original);

  const std::string expectedUtf8(reinterpret_cast<const char *>(u8"چاوە"));
  EXPECT_EQ(utf8, expectedUtf8);
}

TEST(UnicodeConversionTests, AsciiRoundTrip) {
  std::wstring original = L"hello world";
  std::string utf8 = db::Utils::toUtf8(original);
  std::wstring result = db::Utils::fromUtf8(utf8);

  EXPECT_EQ(original, result);
}

TEST(UnicodeConversionTests, AccentedCharacters) {
  std::wstring original = L"café naïve résumé";
  std::string utf8 = db::Utils::toUtf8(original);
  std::wstring result = db::Utils::fromUtf8(utf8);

  EXPECT_EQ(original, result);
}

TEST(UnicodeConversionTests, KurdishText) {
  std::wstring original = L"چاوە";
  std::string utf8 = db::Utils::toUtf8(original);
  std::wstring result = db::Utils::fromUtf8(utf8);

  EXPECT_EQ(original, result);
}

TEST(UnicodeConversionTests, EmptyString) {
  std::wstring original;
  std::string utf8 = db::Utils::toUtf8(original);
  std::wstring result = db::Utils::fromUtf8(utf8);

  EXPECT_EQ(original, result);
}
