#include "db/DatabaseHandler.h"
#include "db/SentenceRepository.h"
#include "db/WordSentenceRepository.h"
#include "utils/Language.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {
const auto en = utils::Language::English;
const auto fr = utils::Language::French;

class SentenceRepositoryTests : public ::testing::Test {
protected:
  db::DatabaseHandler mDbHandler{":memory:"};
  db::SentenceRepository mSentences{mDbHandler.getConnection()};
  db::WordSentenceRepository mWordSentences{mDbHandler.getConnection()};

  // Adds a sentence and links each of `words` to it once.
  void addSentence(std::int64_t hash, int ordinal, const std::wstring &text,
                   const std::vector<std::wstring> &words,
                   utils::Language language = en) {
    mSentences.add(db::models::Sentence{hash, ordinal, text, ordinal * 10,
                                        static_cast<int>(text.size())});
    for (const auto &word : words)
      mWordSentences.add(
          db::models::WordSentence{word, language, hash, ordinal, 1});
  }
};
} // namespace

TEST_F(SentenceRepositoryTests, GetByHashReturnsSentencesInOrder) {
  addSentence(7, 1, L"Second.", {L"second"});
  addSentence(7, 0, L"First.", {L"first"});
  addSentence(8, 0, L"Other text.", {L"other"});

  auto sentences = mSentences.getByHash(7);
  ASSERT_EQ(sentences.size(), 2u);
  EXPECT_EQ(sentences[0].sentence, L"First.");
  EXPECT_EQ(sentences[0].ordinal, 0);
  EXPECT_EQ(sentences[1].sentence, L"Second.");
  EXPECT_EQ(sentences[1].start, 10);
  EXPECT_EQ(sentences[1].length, 7);
  EXPECT_EQ(sentences[1].textHash, 7);
}

TEST_F(SentenceRepositoryTests, GetByWordJoinsThroughWordSentence) {
  const std::int64_t bigHash = 0x7123456789ABCDEF;
  addSentence(bigHash, 0, L"The cat sat.", {L"the", L"cat", L"sat"});
  addSentence(bigHash, 1, L"A dog ran.", {L"a", L"dog", L"ran"});
  addSentence(bigHash, 2, L"The cat ran.", {L"the", L"cat", L"ran"});
  addSentence(9, 0, L"Le chat.", {L"chat"}, fr);
  addSentence(9, 1, L"cat", {L"cat"}, fr);

  auto sentences = mSentences.getByWord(L"cat", en);
  ASSERT_EQ(sentences.size(), 2u);
  EXPECT_EQ(sentences[0].sentence, L"The cat sat.");
  EXPECT_EQ(sentences[1].sentence, L"The cat ran.");
  EXPECT_EQ(sentences[1].textHash, bigHash);
  EXPECT_TRUE(mSentences.getByWord(L"chat", en).empty());
}

TEST_F(SentenceRepositoryTests, RemoveByHash) {
  addSentence(1, 0, L"One.", {L"one"});
  addSentence(2, 0, L"Two.", {L"two"});

  mSentences.removeByHash(1);
  mWordSentences.removeByHash(1);

  EXPECT_TRUE(mSentences.getByHash(1).empty());
  EXPECT_TRUE(mWordSentences.getByHash(1).empty());
  EXPECT_EQ(mSentences.getByHash(2).size(), 1u);
  EXPECT_EQ(mWordSentences.getByHash(2).size(), 1u);
}

TEST_F(SentenceRepositoryTests, RemoveByWordKeepsSentences) {
  addSentence(1, 0, L"Cat and dog.", {L"cat", L"and", L"dog"});
  addSentence(2, 0, L"cat", {L"cat"}, fr);

  mWordSentences.removeByWord(L"cat", en);

  EXPECT_TRUE(mSentences.getByWord(L"cat", en).empty());
  EXPECT_EQ(mSentences.getByWord(L"dog", en).size(), 1u);
  EXPECT_EQ(mSentences.getByWord(L"cat", fr).size(), 1u);
  EXPECT_EQ(mWordSentences.getByHash(1).size(), 2u);
}

TEST_F(SentenceRepositoryTests, WordSentenceRoundTrips) {
  mWordSentences.add(db::models::WordSentence{L"کتێب", utils::Language::Kurdish,
                                              5, 3, 2});
  auto rows = mWordSentences.getByHash(5);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].word, L"کتێب");
  EXPECT_EQ(rows[0].language, utils::Language::Kurdish);
  EXPECT_EQ(rows[0].ordinal, 3);
  EXPECT_EQ(rows[0].numOccurrences, 2);
}
