#include "processor/TextProcessor.h"
#include <gtest/gtest.h>
#include <string>

TEST(TextProcessorTests, ComputeFrequenciesTest) {
  std::wstring text = LR"(
        hello, how are you?
        hi, hello, bonjour


        some more text.
        hi!!!
        HI!
    )";
  auto textProcessor = processor::TextProcessor();

  auto frequencies = textProcessor.computeFrequencies(text);

  EXPECT_EQ(frequencies.at(L"hi"), 3);
  EXPECT_EQ(frequencies.at(L"hello"), 2);
  EXPECT_EQ(frequencies.at(L"bonjour"), 1);
}

TEST(TextProcessorTests, ComputeFrequenciesAccentsTest) {
  std::wstring text = LR"(
        - Bonjour, comment ça va?
        - Salut! Ça va bien, et toi?
        - Oui, ça va.
        - Être et être devraient être le même.
    )";
  auto textProcessor = processor::TextProcessor();

  auto frequencies = textProcessor.computeFrequencies(text);

  EXPECT_EQ(frequencies.at(L"ça"), 3);
  EXPECT_FALSE(frequencies.contains(L"Ça"));
  EXPECT_EQ(frequencies.at(L"va"), 3);
  EXPECT_EQ(frequencies.at(L"comment"), 1);
  EXPECT_EQ(frequencies.at(L"être"), 3);
}

TEST(TextProcessorTests, ComputeFrequenciesKurdishTest) {
  std::wstring text = LR"(
    - سڵاو، چۆنی؟
    - سڵاو! باشم، تۆ چۆنی؟
    - منیش باشم، سوپاس.
    )";
  auto textProcessor = processor::TextProcessor();

  auto frequencies = textProcessor.computeFrequencies(text);

  EXPECT_EQ(frequencies.at(L"سڵاو"), 2);
  EXPECT_FALSE(frequencies.contains(L"سڵاۆ"));
  EXPECT_EQ(frequencies.at(L"باشم"), 2);
  EXPECT_EQ(frequencies.at(L"چۆنی"), 2);
  EXPECT_EQ(frequencies.at(L"سوپاس"), 1);
}

TEST(TextProcessorTests, HashTestReproducible) {
  std::wstring text = LR"(
        hello, how are you?
        hi, hello, bonjour


        some more text.
        hi!!!
        HI!
    )";
  auto textProcessor = processor::TextProcessor();

  uint64_t hash1 = textProcessor.hashWstring(text);
  uint64_t hash2 = textProcessor.hashWstring(text);

  EXPECT_EQ(hash1, hash2);
}

TEST(TextProcessorTests, HashTestDifferent) {
  std::wstring text1 = LR"(
        hello, how are you?
        hi, hello, bonjour


        some more text.
        hi!!!
        HI!
    )";

  // Slightly different: trailing space
  std::wstring text2 = LR"(
        hello, how are you?
        hi, hello, bonjour


        some more text.
        hi!!!
        HI! 
    )";
  auto textProcessor = processor::TextProcessor();

  uint64_t hash1 = textProcessor.hashWstring(text1);
  uint64_t hash2 = textProcessor.hashWstring(text2);

  EXPECT_NE(hash1, hash2);
}

TEST(TextProcessorTests, HashTestKurdish) {
  std::wstring text = LR"(
    - سڵاو، چۆنی؟
    - سڵاو! باشم، تۆ چۆنی؟
    - منیش باشم، سوپاس.
    )";
  auto textProcessor = processor::TextProcessor();

  uint64_t hash1 = textProcessor.hashWstring(text);
  uint64_t hash2 = textProcessor.hashWstring(text);

  EXPECT_EQ(hash1, hash2);
}
