#include "db/TextRepository.h"
#include "db/DatabaseHandler.h"
#include "db/Text.h"
#include "utils/Language.h"
#include <gtest/gtest.h>
#include <sqlite/connection.hpp>
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

  EXPECT_EQ(it2->text, L"Une texte va içi");
}
