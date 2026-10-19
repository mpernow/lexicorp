#include "db/TextRepository.h"
#include "db/DatabaseHandler.h"
#include "db/Text.h"
#include "utils/Language.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <sqlite/connection.hpp>
#include <sqlite/database_exception.hpp>
#include <sqlite/query.hpp>
#include <string>

TEST(TextRepositoryTests, InsertTextTest) {
  // Create DatabaseHandler with in-memory database
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto textRepository = db::TextRepository{dbHandler.getConnection()};

  db::models::Text text =
      db::models::Text(L"Some text goes here", 12345, utils::Language::English,
                       L"https://www.source.com");
  textRepository.add(text);

  auto dbText = textRepository.getByHash(12345);

  ASSERT_TRUE(dbText.has_value());
  EXPECT_EQ(text.text, dbText->text);
  EXPECT_EQ(text.hash, dbText->hash);
  EXPECT_EQ(text.language, dbText->language);
  EXPECT_EQ(text.source, dbText->source);
}

TEST(TextRepositoryTests, InsertMultipleTextsTest) {
  // Create DatabaseHandler with in-memory database
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto textRepository = db::TextRepository{dbHandler.getConnection()};

  db::models::Text text1 =
      db::models::Text(L"Some text goes here", 12345, utils::Language::English,
                       L"https://www.source.com");
  db::models::Text text2 =
      db::models::Text(L"Une texte va içi", 54321, utils::Language::French,
                       L"https://www.source.fr");
  textRepository.add(text1);
  textRepository.add(text2);

  auto texts = textRepository.getAll();

  ASSERT_EQ(texts.size(), 2);

  auto it1 = std::find_if(texts.begin(), texts.end(),
                          [](const auto &t) { return t.hash == 12345; });

  auto it2 = std::find_if(texts.begin(), texts.end(),
                          [](const auto &t) { return t.hash == 54321; });

  ASSERT_NE(it1, texts.end());
  ASSERT_NE(it2, texts.end());

  EXPECT_EQ(it1->text, L"Some text goes here");
  EXPECT_EQ(it1->language, utils::Language::English);

  EXPECT_EQ(it2->text, L"Une texte va içi");
  EXPECT_EQ(it2->language, utils::Language::French);
}

TEST(TextRepositoryTests, GetByHashMissingReturnsNullopt) {
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto textRepository = db::TextRepository{dbHandler.getConnection()};

  EXPECT_FALSE(textRepository.getByHash(404).has_value());
}

TEST(TextRepositoryTests, GetAllOnEmptyTableReturnsEmpty) {
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto textRepository = db::TextRepository{dbHandler.getConnection()};

  EXPECT_TRUE(textRepository.getAll().empty());
}

// `hash` is the primary key, so re-inserting the same text (same hash) must
// surface as an exception, not a silent no-op or a duplicate row.
TEST(TextRepositoryTests, DuplicateHashInsertThrows) {
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto textRepository = db::TextRepository{dbHandler.getConnection()};

  db::models::Text text(L"body", 777, utils::Language::English, L"src");
  textRepository.add(text);

  EXPECT_THROW(textRepository.add(text), sqlite::database_exception);
}

TEST(TextRepositoryTests, GetAllByLanguageTest) {
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto textRepository = db::TextRepository{dbHandler.getConnection()};

  textRepository.add(db::models::Text(L"Second English text", 2,
                                      utils::Language::English, L"b-source"));
  textRepository.add(db::models::Text(L"Une texte", 3, utils::Language::French,
                                      L"fr-source"));
  textRepository.add(db::models::Text(L"First English text", 1,
                                      utils::Language::English, L"a-source"));

  auto english = textRepository.getAll(utils::Language::English);

  // Only English texts, ordered by source label.
  ASSERT_EQ(english.size(), 2);
  EXPECT_EQ(english[0].source, L"a-source");
  EXPECT_EQ(english[1].source, L"b-source");

  EXPECT_TRUE(textRepository.getAll(utils::Language::Kurdish).empty());
}

TEST(TextRepositoryTests, RemoveTextTest) {
  auto dbHandler = db::DatabaseHandler{":memory:"};
  auto textRepository = db::TextRepository{dbHandler.getConnection()};

  textRepository.add(
      db::models::Text{L"first", 1, utils::Language::English, L"a"});
  textRepository.add(
      db::models::Text{L"second", 2, utils::Language::English, L"b"});

  textRepository.remove(1);

  EXPECT_FALSE(textRepository.getByHash(1).has_value());
  EXPECT_TRUE(textRepository.getByHash(2).has_value());
}

TEST(TextRepositoryTests, KeepsFull64BitHash) {
  auto dbHandler = db::DatabaseHandler{":memory:"};
  auto textRepository = db::TextRepository{dbHandler.getConnection()};
  const std::int64_t big = 0x7123456789ABCDEF;
  const std::int64_t negative = -0x7123456789ABCDEF;

  textRepository.add(db::models::Text{L"a", big, utils::Language::English});
  textRepository.add(
      db::models::Text{L"b", negative, utils::Language::English});

  ASSERT_TRUE(textRepository.getByHash(big).has_value());
  EXPECT_EQ(textRepository.getByHash(big)->text, L"a");
  EXPECT_EQ(textRepository.getByHash(negative)->text, L"b");
  EXPECT_FALSE(
      textRepository.getByHash(static_cast<std::int32_t>(big)).has_value());
}
