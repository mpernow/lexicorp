#include "db/WordTextRepository.h"
#include "db/DatabaseHandler.h"
#include "db/WordText.h"
#include "utils/Language.h"
#include <gtest/gtest.h>
#include <sqlite/connection.hpp>
#include <sqlite/query.hpp>
#include <string>

TEST(TextRepositoryTests, InsertWordTextTest) {
  // Create DatabaseHandler with in-memory database
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordTextRepository = db::WordTextRepository{dbHandler.getConnection()};

  db::models::WordText wordText = db::models::WordText(
      L"Some text goes here", 12345, utils::Language::English);
  wordTextRepository.add(wordText);

  auto dbWordText =
      wordTextRepository.getByHash(12345, utils::Language::English);

  EXPECT_EQ(wordText.word, dbWordText->word);
  EXPECT_EQ(wordText.textHash, dbWordText->textHash);
  EXPECT_EQ(wordText.language, dbWordText->language);
}

TEST(TextRepositoryTests, InsertMultipleLanguagesTest) {
  // Create DatabaseHandler with in-memory database
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordTextRepository = db::WordTextRepository{dbHandler.getConnection()};

  db::models::WordText wordText1 =
      db::models::WordText(L"cinema", 12345, utils::Language::English);
  db::models::WordText wordText2 =
      db::models::WordText(L"cinema", 54321, utils::Language::French);
  wordTextRepository.add(wordText1);
  wordTextRepository.add(wordText2);

  auto dbWordText1 =
      wordTextRepository.getByHash(12345, utils::Language::English);
  auto dbWordText2 =
      wordTextRepository.getByHash(54321, utils::Language::French);

  EXPECT_EQ(dbWordText1->word, wordText1.word);
  EXPECT_EQ(dbWordText1->textHash, wordText1.textHash);

  EXPECT_EQ(dbWordText2->word, wordText2.word);
  EXPECT_EQ(dbWordText2->textHash, wordText2.textHash);

  auto dbWordText11 =
      wordTextRepository.getByWord(L"cinema", utils::Language::English);
  auto dbWordText22 =
      wordTextRepository.getByWord(L"cinema", utils::Language::French);

  EXPECT_EQ(dbWordText11->word, wordText1.word);
  EXPECT_EQ(dbWordText11->textHash, wordText1.textHash);

  EXPECT_EQ(dbWordText22->word, wordText2.word);
  EXPECT_EQ(dbWordText22->textHash, wordText2.textHash);

  auto dbWordTextEmpty1 =
      wordTextRepository.getByHash(12345, utils::Language::French);
  auto dbWordTextEmpty2 =
      wordTextRepository.getByHash(54321, utils::Language::English);

  EXPECT_FALSE(dbWordTextEmpty1.has_value());
  EXPECT_FALSE(dbWordTextEmpty2.has_value());
}
