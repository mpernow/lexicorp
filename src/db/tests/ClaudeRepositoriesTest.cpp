#include "db/ClaudeUsageRepository.h"
#include "db/DatabaseHandler.h"
#include "db/ExpressionWordRepository.h"
#include "utils/Language.h"
#include <gtest/gtest.h>
#include <sqlite/command.hpp>
#include <string>
#include <vector>

namespace {
const auto ckb = utils::Language::Kurdish;

class ClaudeRepositoriesTests : public ::testing::Test {
protected:
  db::DatabaseHandler mDbHandler{":memory:"};
  db::ClaudeUsageRepository mUsage{mDbHandler.getConnection()};
  db::ExpressionWordRepository mExpressions{mDbHandler.getConnection()};

  void addUsage(const std::string &model, std::int64_t input,
                std::int64_t output) {
    mUsage.add(db::models::ClaudeUsage{.model = model,
                                       .purpose = "define_word",
                                       .inputTokens = input,
                                       .outputTokens = output,
                                       .cacheReadTokens = 10});
  }
};
} // namespace

TEST_F(ClaudeRepositoriesTests, TotalsArePerModel) {
  addUsage("claude-sonnet-5-5", 100, 20);
  addUsage("claude-sonnet-5-5", 50, 5);
  addUsage("claude-haiku-5-5", 7, 3);

  auto totals = mUsage.totalsSince(0);
  ASSERT_EQ(totals.size(), 2u);
  EXPECT_EQ(totals[0].model, "claude-haiku-5-5");
  EXPECT_EQ(totals[0].requests, 1);
  EXPECT_EQ(totals[1].model, "claude-sonnet-5-5");
  EXPECT_EQ(totals[1].requests, 2);
  EXPECT_EQ(totals[1].inputTokens, 150);
  EXPECT_EQ(totals[1].outputTokens, 25);
  EXPECT_EQ(totals[1].cacheReadTokens, 20);
}

TEST_F(ClaudeRepositoriesTests, TotalsSkipOlderRequests) {
  addUsage("claude-sonnet-5-5", 100, 20);
  sqlite::command backdate{mDbHandler.getConnection(),
                           "UPDATE claude_usage SET created_at = 1000"};
  backdate();
  addUsage("claude-sonnet-5-5", 50, 5);

  auto totals = mUsage.totalsSince(2000);
  ASSERT_EQ(totals.size(), 1u);
  EXPECT_EQ(totals[0].requests, 1);
  EXPECT_EQ(totals[0].inputTokens, 50);
  EXPECT_TRUE(mUsage.totalsSince(4'000'000'000).empty());
}

TEST_F(ClaudeRepositoriesTests, ExpressionLinksAreUnique) {
  mExpressions.add({L"پێم", ckb, L"پێم وایە"});
  mExpressions.add({L"پێم", ckb, L"پێم وایە"});
  mExpressions.add({L"پێم", ckb, L"پێم بڵێ"});
  mExpressions.add({L"وایە", ckb, L"پێم وایە"});

  EXPECT_EQ(mExpressions.getByWord(L"پێم", ckb),
            (std::vector<std::wstring>{L"پێم بڵێ", L"پێم وایە"}));
  EXPECT_EQ(mExpressions.getByWord(L"وایە", ckb),
            (std::vector<std::wstring>{L"پێم وایە"}));
  EXPECT_TRUE(mExpressions.getByWord(L"پێم", utils::Language::English).empty());
}
