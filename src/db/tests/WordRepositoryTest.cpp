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

  db::models::Word word = db::models::Word(L"hi", 2, true);
  wordRepository.add(word);

  auto dbWord = wordRepository.getByText(L"hi");

  EXPECT_EQ(word.word, dbWord->word);
  EXPECT_EQ(word.numOccurrences, dbWord->numOccurrences);
  EXPECT_EQ(word.known, dbWord->known);
}

TEST(WordRepositoryTests, InsertMultipleWordsTest) {
  // Create DatabaseHandler with in-memory database
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordRepository = db::WordRepository{dbHandler.getConnection()};

  db::models::Word word = db::models::Word(L"hi", 2, true);
  db::models::Word word2 = db::models::Word(L"hello", 3, false);
  wordRepository.add(word);
  wordRepository.add(word2);

  auto words = wordRepository.getAll();

  ASSERT_EQ(words.size(), 2);

  auto itHi = std::find_if(words.begin(), words.end(),
                           [](const auto &w) { return w.word == L"hi"; });

  auto itHello = std::find_if(words.begin(), words.end(),
                              [](const auto &w) { return w.word == L"hello"; });

  ASSERT_NE(itHi, words.end());
  ASSERT_NE(itHello, words.end());

  EXPECT_EQ(itHi->numOccurrences, 2);
  EXPECT_EQ(itHi->known, true);

  EXPECT_EQ(itHello->numOccurrences, 3);
  EXPECT_EQ(itHello->known, false);
}

TEST(WordRepositoryTests, UpdateWordTest) {
  // Create DatabaseHandler with in-memory database
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordRepository = db::WordRepository{dbHandler.getConnection()};

  db::models::Word word = db::models::Word(L"hi", 2, false);
  wordRepository.add(word);

  auto dbWord = wordRepository.getByText(L"hi");

  EXPECT_EQ(word.word, dbWord->word);
  EXPECT_EQ(word.numOccurrences, dbWord->numOccurrences);
  EXPECT_EQ(word.known, dbWord->known);

  db::models::Word wordUpdated = db::models::Word(L"hi", 3, true);
  wordRepository.update(wordUpdated);

  auto dbWordUpdated = wordRepository.getByText(L"hi");

  EXPECT_EQ(wordUpdated.word, dbWordUpdated->word);
  EXPECT_EQ(wordUpdated.numOccurrences, dbWordUpdated->numOccurrences);
  EXPECT_EQ(wordUpdated.known, dbWordUpdated->known);
}

TEST(WordRepositoryTests, UpdateFrequenciesTest) {
  // Create DatabaseHandler with in-memory database
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordRepository = db::WordRepository{dbHandler.getConnection()};

  db::models::Word word = db::models::Word(L"hello", 2, false);
  wordRepository.add(word);

  std::unordered_map<std::wstring, int> freqs = {
      {L"hello", 1}, {L"world", 2}, {L"çavê", 3}};

  wordRepository.updateFrequencies(freqs);

  std::unordered_map<std::wstring, int> expectedFreqs = {
      {L"hello", 3}, {L"world", 2}, {L"çavê", 3}};

  for (auto &[word, freq] : freqs) {
    auto dbWord = wordRepository.getByText(word);
    EXPECT_EQ(dbWord->numOccurrences, expectedFreqs.at(word));
  }
}
