#include "db/WordRepository.h"
#include "db/DatabaseHandler.h"
#include "db/Word.h"
#include <gtest/gtest.h>
#include <sqlite/connection.hpp>
#include <sqlite/query.hpp>
#include <string>

TEST(WordRepositoryTests, InsertWordTest) {
  // Create DatabaseHandler with in-memory database
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordRepository = db::WordRepository{dbHandler.getConnection()};

  db::models::Word word = db::models::Word("hi", 2, true);
  wordRepository.add(word);

  auto dbWord = wordRepository.getByText("hi");

  EXPECT_EQ(word.word, dbWord->word);
  EXPECT_EQ(word.numOccurences, dbWord->numOccurences);
  EXPECT_EQ(word.known, dbWord->known);
}

TEST(WordRepositoryTests, InsertMultipleWordsTest) {
  // Create DatabaseHandler with in-memory database
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordRepository = db::WordRepository{dbHandler.getConnection()};

  db::models::Word word = db::models::Word("hi", 2, true);
  db::models::Word word2 = db::models::Word("hello", 3, false);
  wordRepository.add(word);
  wordRepository.add(word2);

  auto words = wordRepository.getAll();

  ASSERT_EQ(words.size(), 2);

  auto itHi = std::find_if(words.begin(), words.end(),
                           [](const auto &w) { return w.word == "hi"; });

  auto itHello = std::find_if(words.begin(), words.end(),
                              [](const auto &w) { return w.word == "hello"; });

  ASSERT_NE(itHi, words.end());
  ASSERT_NE(itHello, words.end());

  EXPECT_EQ(itHi->numOccurences, 2);
  EXPECT_EQ(itHi->known, true);

  EXPECT_EQ(itHello->numOccurences, 3);
  EXPECT_EQ(itHello->known, false);
}

TEST(WordRepositoryTests, UpdateWordTest) {
  // Create DatabaseHandler with in-memory database
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordRepository = db::WordRepository{dbHandler.getConnection()};

  db::models::Word word = db::models::Word("hi", 2, false);
  wordRepository.add(word);

  auto dbWord = wordRepository.getByText("hi");

  EXPECT_EQ(word.word, dbWord->word);
  EXPECT_EQ(word.numOccurences, dbWord->numOccurences);
  EXPECT_EQ(word.known, dbWord->known);

  db::models::Word wordUpdated = db::models::Word("hi", 3, true);
  wordRepository.update(wordUpdated);

  auto dbWordUpdated = wordRepository.getByText("hi");

  EXPECT_EQ(wordUpdated.word, dbWordUpdated->word);
  EXPECT_EQ(wordUpdated.numOccurences, dbWordUpdated->numOccurences);
  EXPECT_EQ(wordUpdated.known, dbWordUpdated->known);
}
