#include "db/DatabaseHandler.h"
#include "db/Savepoint.h"
#include "db/Word.h"
#include "db/WordRepository.h"
#include "utils/Language.h"
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<db::models::Word> oneWord() {
  return {db::models::Word{L"hi", 2, false, utils::Language::English}};
}

} // namespace

TEST(SavepointTests, CommitKeepsChanges) {
  auto dbHandler = db::DatabaseHandler{":memory:"};
  auto words = db::WordRepository{dbHandler.getConnection()};

  {
    db::Savepoint tx{dbHandler.getConnection(), "test"};
    words.add(oneWord().front());
    tx.commit();
  }

  EXPECT_TRUE(words.getByText(L"hi", utils::Language::English).has_value());
}

TEST(SavepointTests, LeavingScopeWithoutCommitRollsBack) {
  auto dbHandler = db::DatabaseHandler{":memory:"};
  auto words = db::WordRepository{dbHandler.getConnection()};

  {
    db::Savepoint tx{dbHandler.getConnection(), "test"};
    words.add(oneWord().front());
  }

  EXPECT_FALSE(words.getByText(L"hi", utils::Language::English).has_value());
}

TEST(SavepointTests, ExceptionRollsBack) {
  auto dbHandler = db::DatabaseHandler{":memory:"};
  auto words = db::WordRepository{dbHandler.getConnection()};

  auto failPartWay = [&] {
    db::Savepoint tx{dbHandler.getConnection(), "test"};
    words.add(oneWord().front());
    throw std::runtime_error("fail part-way");
  };
  EXPECT_THROW(failPartWay(), std::runtime_error);

  EXPECT_FALSE(words.getByText(L"hi", utils::Language::English).has_value());
}

// updateFrequencies opens its own savepoint; it must work inside an outer
// one (InsertTextPage::submit) and be undone with it.
TEST(SavepointTests, UpdateFrequenciesNestsInsideOuterSavepoint) {
  auto dbHandler = db::DatabaseHandler{":memory:"};
  auto words = db::WordRepository{dbHandler.getConnection()};

  {
    db::Savepoint tx{dbHandler.getConnection(), "outer"};
    ASSERT_NO_THROW(words.updateFrequencies(oneWord()));
    EXPECT_TRUE(words.getByText(L"hi", utils::Language::English).has_value());
  }
  EXPECT_FALSE(words.getByText(L"hi", utils::Language::English).has_value());

  {
    db::Savepoint tx{dbHandler.getConnection(), "outer"};
    words.updateFrequencies(oneWord());
    tx.commit();
  }
  auto word = words.getByText(L"hi", utils::Language::English);
  ASSERT_TRUE(word.has_value());
  EXPECT_EQ(word->numOccurrences, 2);
}
