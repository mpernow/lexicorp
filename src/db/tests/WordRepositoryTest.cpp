#include "db/WordRepository.h"
#include "db/DatabaseHandler.h"
#include "db/Word.h"
#include "utils/Language.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <sqlite/connection.hpp>
#include <sqlite/database_exception.hpp>
#include <sqlite/query.hpp>
#include <string>
#include <unordered_map>
#include <vector>

TEST(WordRepositoryTests, InsertWordTest) {
  // Create DatabaseHandler with in-memory database
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordRepository = db::WordRepository{dbHandler.getConnection()};

  db::models::Word word =
      db::models::Word(L"hi", 2, true, utils::Language::French);
  wordRepository.add(word);

  auto dbWord = wordRepository.getByText(L"hi", utils::Language::French);

  ASSERT_TRUE(dbWord.has_value());
  EXPECT_EQ(word.word, dbWord->word);
  EXPECT_EQ(word.numOccurrences, dbWord->numOccurrences);
  EXPECT_EQ(word.known, dbWord->known);
  EXPECT_EQ(word.language, dbWord->language);
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

  auto words = wordRepository.getAll(utils::Language::Unknown);

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

  auto dbWord = wordRepository.getByText(L"hi", utils::Language::Unknown);

  ASSERT_TRUE(dbWord.has_value());
  EXPECT_EQ(word.word, dbWord->word);
  EXPECT_EQ(word.numOccurrences, dbWord->numOccurrences);
  EXPECT_EQ(word.known, dbWord->known);

  db::models::Word wordUpdated = db::models::Word(L"hi", 3, true);
  wordRepository.update(wordUpdated);

  auto dbWordUpdated =
      wordRepository.getByText(L"hi", utils::Language::Unknown);

  ASSERT_TRUE(dbWordUpdated.has_value());
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

  wordRepository.updateFrequencies(freqs, utils::Language::Unknown);

  std::unordered_map<std::wstring, int> expectedFreqs = {
      {L"hello", 3}, {L"world", 2}, {L"çavê", 3}};

  for (const auto &[expectWord, expectFreq] : expectedFreqs) {
    auto dbWord =
        wordRepository.getByText(expectWord, utils::Language::Unknown);
    ASSERT_TRUE(dbWord.has_value()) << "missing word after updateFrequencies";
    EXPECT_EQ(dbWord->numOccurrences, expectFreq);
  }
}

TEST(WordRepositoryTests, UpdateFrequenciesMultipleLanguagesTest) {
  // Create DatabaseHandler with in-memory database
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordRepository = db::WordRepository{dbHandler.getConnection()};

  db::models::Word word =
      db::models::Word(L"hello", 2, false, utils::Language::English);
  wordRepository.add(word);

  std::unordered_map<std::wstring, int> freqs = {
      {L"hello", 1}, {L"world", 2}, {L"çavê", 3}};

  wordRepository.updateFrequencies(freqs, utils::Language::English);

  std::unordered_map<std::wstring, int> freqsUnknownLang = {
      {L"hello", 11}, {L"world", 22}, {L"çavê", 33}};

  wordRepository.updateFrequencies(freqsUnknownLang, utils::Language::Unknown);

  std::unordered_map<std::wstring, int> expectedFreqs = {
      {L"hello", 3}, {L"world", 2}, {L"çavê", 3}};

  for (const auto &[expectWord, expectFreq] : expectedFreqs) {
    auto dbWord =
        wordRepository.getByText(expectWord, utils::Language::English);
    ASSERT_TRUE(dbWord.has_value());
    EXPECT_EQ(dbWord->numOccurrences, expectFreq);
  }

  for (const auto &[expectWord, expectFreq] : freqsUnknownLang) {
    auto dbWord =
        wordRepository.getByText(expectWord, utils::Language::Unknown);
    ASSERT_TRUE(dbWord.has_value());
    EXPECT_EQ(dbWord->numOccurrences, expectFreq);
  }
}

TEST(WordRepositoryTests, GetByTextMissingReturnsNullopt) {
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordRepository = db::WordRepository{dbHandler.getConnection()};

  EXPECT_FALSE(
      wordRepository.getByText(L"absent", utils::Language::English)
          .has_value());
}

// (word, language) is the primary key: the same spelling under two languages
// must be two independent rows.
TEST(WordRepositoryTests, KeyIsWordPlusLanguage) {
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordRepository = db::WordRepository{dbHandler.getConnection()};

  wordRepository.add(db::models::Word(L"chat", 4, false, utils::Language::French));
  wordRepository.add(
      db::models::Word(L"chat", 9, true, utils::Language::English));

  auto fr = wordRepository.getByText(L"chat", utils::Language::French);
  auto en = wordRepository.getByText(L"chat", utils::Language::English);

  ASSERT_TRUE(fr.has_value());
  ASSERT_TRUE(en.has_value());
  EXPECT_EQ(fr->numOccurrences, 4);
  EXPECT_FALSE(fr->known);
  EXPECT_EQ(en->numOccurrences, 9);
  EXPECT_TRUE(en->known);

  EXPECT_EQ(wordRepository.getAll(utils::Language::French).size(), 1u);
  EXPECT_EQ(wordRepository.getAll(utils::Language::English).size(), 1u);
}

// Re-adding an existing (word, language) violates the primary key; the
// repository must surface that rather than swallow it.
TEST(WordRepositoryTests, DuplicateAddThrows) {
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordRepository = db::WordRepository{dbHandler.getConnection()};

  db::models::Word word(L"hi", 1, false, utils::Language::English);
  wordRepository.add(word);

  EXPECT_THROW(wordRepository.add(word), sqlite::database_exception);
}

// The vector overload of updateFrequencies is what InsertTextPage::submit()
// calls. It adds occurrence counts and OR-merges the `known` flag in both
// directions (once known, stays known).
TEST(WordRepositoryTests, UpdateFrequenciesVectorOverloadMergesKnown) {
  std::string dbName = ":memory:";
  auto dbHandler = db::DatabaseHandler{dbName};
  auto wordRepository = db::WordRepository{dbHandler.getConnection()};

  wordRepository.add(
      db::models::Word(L"alpha", 5, true, utils::Language::English));
  wordRepository.add(
      db::models::Word(L"beta", 5, false, utils::Language::English));

  std::vector<db::models::Word> batch = {
      db::models::Word(L"alpha", 2, false, utils::Language::English),
      db::models::Word(L"beta", 3, true, utils::Language::English),
      db::models::Word(L"gamma", 7, false, utils::Language::English),
  };
  wordRepository.updateFrequencies(batch);

  auto alpha = wordRepository.getByText(L"alpha", utils::Language::English);
  auto beta = wordRepository.getByText(L"beta", utils::Language::English);
  auto gamma = wordRepository.getByText(L"gamma", utils::Language::English);

  ASSERT_TRUE(alpha.has_value());
  ASSERT_TRUE(beta.has_value());
  ASSERT_TRUE(gamma.has_value());

  EXPECT_EQ(alpha->numOccurrences, 7);
  EXPECT_TRUE(alpha->known) << "known must survive an incoming known=false";

  EXPECT_EQ(beta->numOccurrences, 8);
  EXPECT_TRUE(beta->known) << "incoming known=true must set the flag";

  EXPECT_EQ(gamma->numOccurrences, 7);
  EXPECT_FALSE(gamma->known);
}

TEST(WordRepositoryTests, RemoveWordTest) {
  auto dbHandler = db::DatabaseHandler{":memory:"};
  auto wordRepository = db::WordRepository{dbHandler.getConnection()};

  wordRepository.add(db::models::Word{L"chat", 1, false,
                                      utils::Language::English});
  wordRepository.add(db::models::Word{L"chat", 1, false,
                                      utils::Language::French});

  wordRepository.remove(L"chat", utils::Language::English);

  EXPECT_FALSE(
      wordRepository.getByText(L"chat", utils::Language::English).has_value());
  EXPECT_TRUE(
      wordRepository.getByText(L"chat", utils::Language::French).has_value());
}

TEST(WordRepositoryTests, SubtractFrequenciesTest) {
  auto dbHandler = db::DatabaseHandler{":memory:"};
  auto wordRepository = db::WordRepository{dbHandler.getConnection()};
  const auto en = utils::Language::English;

  wordRepository.add(db::models::Word{L"common", 5, false, en});
  wordRepository.add(db::models::Word{L"rare", 2, false, en});
  wordRepository.add(db::models::Word{L"learnt", 2, true, en});

  wordRepository.subtractFrequencies({{L"common", 2, false, en},
                                      {L"rare", 2, false, en},
                                      {L"learnt", 3, false, en},
                                      {L"absent", 1, false, en}});

  EXPECT_EQ(wordRepository.getByText(L"common", en)->numOccurrences, 3);
  // Unknown words left without occurrences are removed...
  EXPECT_FALSE(wordRepository.getByText(L"rare", en).has_value());
  // ...known ones are kept, never going below zero.
  auto learnt = wordRepository.getByText(L"learnt", en);
  ASSERT_TRUE(learnt.has_value());
  EXPECT_EQ(learnt->numOccurrences, 0);
  EXPECT_TRUE(learnt->known);
  EXPECT_FALSE(wordRepository.getByText(L"absent", en).has_value());
}
