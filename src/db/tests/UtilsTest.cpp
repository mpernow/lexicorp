#include "db/Utils.h"
#include "db/Word.h"
#include <gtest/gtest.h>

TEST(MyLibTest, AdditionWorks) { EXPECT_EQ(2 + 3, 5); }

TEST(MyLibTest, SubtractionWorks) { EXPECT_EQ(5 - 3, 2); }

TEST(MyLibTest, SqlSchemaTest) {
  EXPECT_EQ(db::Utils::createTableSql<db::models::Word>(), "HI");
}
