#include "db/WordTextRepository.h"
#include "db/DatabaseHandler.h"
#include "db/WordText.h"
#include "utils/Language.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <sqlite/connection.hpp>
#include <sqlite/database_exception.hpp>
#include <sqlite/query.hpp>
#include <string>
#include <vector>

TEST(WordTextRepositoryTests, InsertWordTextTest) {
  // Create DatabaseHandler with in-memory database
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordTextRepository = db::WordTextRepository{dbHandler.getConnection()};

  db::models::WordText wordText = db::models::WordText(
      L"Some text goes here", 12345, 1, utils::Language::English);
  wordTextRepository.add(wordText);

  auto dbWordText =
      wordTextRepository.getByHash(12345, utils::Language::English);

  ASSERT_TRUE(dbWordText.has_value());
  EXPECT_EQ(wordText.word, dbWordText->word);
  EXPECT_EQ(wordText.textHash, dbWordText->textHash);
  EXPECT_EQ(wordText.numOccurrences, dbWordText->numOccurrences);
  EXPECT_EQ(wordText.language, dbWordText->language);
}

TEST(WordTextRepositoryTests, InsertMultipleLanguagesTest) {
  // Create DatabaseHandler with in-memory database
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordTextRepository = db::WordTextRepository{dbHandler.getConnection()};

  db::models::WordText wordText1 =
      db::models::WordText(L"cinema", 12345, 2, utils::Language::English);
  db::models::WordText wordText2 =
      db::models::WordText(L"cinema", 54321, 3, utils::Language::French);
  wordTextRepository.add(wordText1);
  wordTextRepository.add(wordText2);

  auto dbWordText1 =
      wordTextRepository.getByHash(12345, utils::Language::English);
  auto dbWordText2 =
      wordTextRepository.getByHash(54321, utils::Language::French);

  ASSERT_TRUE(dbWordText1.has_value());
  ASSERT_TRUE(dbWordText2.has_value());
  EXPECT_EQ(dbWordText1->word, wordText1.word);
  EXPECT_EQ(dbWordText1->textHash, wordText1.textHash);

  EXPECT_EQ(dbWordText2->word, wordText2.word);
  EXPECT_EQ(dbWordText2->textHash, wordText2.textHash);

  // TODO: better tests for vector-valued type
  auto englishByWord =
      wordTextRepository.getByWord(L"cinema", utils::Language::English);
  auto frenchByWord =
      wordTextRepository.getByWord(L"cinema", utils::Language::French);
  ASSERT_EQ(englishByWord.size(), 1u);
  ASSERT_EQ(frenchByWord.size(), 1u);
  auto dbWordText11 = englishByWord[0];
  auto dbWordText22 = frenchByWord[0];

  EXPECT_EQ(dbWordText11.word, wordText1.word);
  EXPECT_EQ(dbWordText11.textHash, wordText1.textHash);

  EXPECT_EQ(dbWordText22.word, wordText2.word);
  EXPECT_EQ(dbWordText22.textHash, wordText2.textHash);

  auto dbWordTextEmpty1 =
      wordTextRepository.getByHash(12345, utils::Language::French);
  auto dbWordTextEmpty2 =
      wordTextRepository.getByHash(54321, utils::Language::English);

  EXPECT_FALSE(dbWordTextEmpty1.has_value());
  EXPECT_FALSE(dbWordTextEmpty2.has_value());
}

TEST(WordTextRepositoryTests, GetByHashMissingReturnsNullopt) {
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordTextRepository = db::WordTextRepository{dbHandler.getConnection()};

  EXPECT_FALSE(
      wordTextRepository.getByHash(999, utils::Language::English).has_value());
}

TEST(WordTextRepositoryTests, GetByWordMissingReturnsEmpty) {
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordTextRepository = db::WordTextRepository{dbHandler.getConnection()};

  EXPECT_TRUE(
      wordTextRepository.getByWord(L"absent", utils::Language::English).empty());
}

// getByWord must return every text a word appears in, for the requested
// language only.
TEST(WordTextRepositoryTests, GetByWordReturnsAllMatchingTexts) {
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordTextRepository = db::WordTextRepository{dbHandler.getConnection()};

  wordTextRepository.add(
      db::models::WordText(L"chat", 1, 2, utils::Language::French));
  wordTextRepository.add(
      db::models::WordText(L"chat", 2, 5, utils::Language::French));
  wordTextRepository.add(
      db::models::WordText(L"chat", 3, 9, utils::Language::English));

  auto french = wordTextRepository.getByWord(L"chat", utils::Language::French);
  ASSERT_EQ(french.size(), 2u);

  std::vector<int> hashes{french[0].textHash, french[1].textHash};
  std::sort(hashes.begin(), hashes.end());
  EXPECT_EQ(hashes, (std::vector<int>{1, 2}));
}

// getByHash with several words for the same text currently returns exactly one
// arbitrary row (SELECT ... LIMIT 1). Pin that behaviour.
TEST(WordTextRepositoryTests, GetByHashWithManyWordsReturnsOneRow) {
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordTextRepository = db::WordTextRepository{dbHandler.getConnection()};

  wordTextRepository.add(
      db::models::WordText(L"le", 7, 3, utils::Language::French));
  wordTextRepository.add(
      db::models::WordText(L"chat", 7, 1, utils::Language::French));

  auto row = wordTextRepository.getByHash(7, utils::Language::French);
  ASSERT_TRUE(row.has_value());
  EXPECT_EQ(row->textHash, 7);
  EXPECT_TRUE(row->word == L"le" || row->word == L"chat");
}

// A repeated (word, text_hash) violates the primary key. Document that the
// repository surfaces this as an exception rather than swallowing it.
TEST(WordTextRepositoryTests, DuplicateInsertThrows) {
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordTextRepository = db::WordTextRepository{dbHandler.getConnection()};

  db::models::WordText wordText(L"chat", 42, 1, utils::Language::French);
  wordTextRepository.add(wordText);

  EXPECT_THROW(wordTextRepository.add(wordText), sqlite::database_exception);
}

TEST(WordTextRepositoryTests, GetAndRemoveByHashTest) {
  auto dbHandler = db::DatabaseHandler{":memory:"};
  auto wordTextRepository = db::WordTextRepository{dbHandler.getConnection()};
  const auto en = utils::Language::English;

  wordTextRepository.add(db::models::WordText{L"cat", 1, 2, en});
  wordTextRepository.add(db::models::WordText{L"sat", 1, 1, en});
  wordTextRepository.add(db::models::WordText{L"cat", 2, 1, en});

  EXPECT_EQ(wordTextRepository.getAllByHash(1).size(), 2u);

  wordTextRepository.removeByHash(1);

  EXPECT_TRUE(wordTextRepository.getAllByHash(1).empty());
  EXPECT_EQ(wordTextRepository.getAllByHash(2).size(), 1u);
}

TEST(WordTextRepositoryTests, RemoveByWordTest) {
  auto dbHandler = db::DatabaseHandler{":memory:"};
  auto wordTextRepository = db::WordTextRepository{dbHandler.getConnection()};

  wordTextRepository.add(
      db::models::WordText{L"chat", 1, 1, utils::Language::English});
  wordTextRepository.add(
      db::models::WordText{L"sat", 1, 1, utils::Language::English});
  wordTextRepository.add(
      db::models::WordText{L"chat", 2, 1, utils::Language::French});

  wordTextRepository.removeByWord(L"chat", utils::Language::English);

  EXPECT_TRUE(
      wordTextRepository.getByWord(L"chat", utils::Language::English).empty());
  EXPECT_EQ(wordTextRepository.getAllByHash(1).size(), 1u);
  EXPECT_EQ(
      wordTextRepository.getByWord(L"chat", utils::Language::French).size(),
      1u);
}
