#include "utils/Language.h"
#include <gtest/gtest.h>
#include <string>

TEST(LanguageTests, ToStringKnownValues) {
  EXPECT_EQ(utils::to_string(utils::Language::English), "English");
  EXPECT_EQ(utils::to_string(utils::Language::French), "French");
  EXPECT_EQ(utils::to_string(utils::Language::Kurdish), "Kurdish");
  EXPECT_EQ(utils::to_string(utils::Language::Unknown), "Unknown");
}

TEST(LanguageTests, RoundTripThroughString) {
  for (auto lang : {utils::Language::English, utils::Language::French,
                    utils::Language::Kurdish, utils::Language::Unknown}) {
    EXPECT_EQ(utils::language_from_string(utils::to_string(lang)), lang);
  }
}

TEST(LanguageTests, UnrecognisedStringMapsToUnknown) {
  EXPECT_EQ(utils::language_from_string(""), utils::Language::Unknown);
  EXPECT_EQ(utils::language_from_string("Klingon"), utils::Language::Unknown);
  EXPECT_EQ(utils::language_from_string("english"), utils::Language::Unknown)
      << "matching is case-sensitive";
}
