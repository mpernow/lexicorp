#include "processor/TextProcessor.h"
#include <gtest/gtest.h>
#include <iostream>
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
    )";
  auto textProcessor = processor::TextProcessor();

  auto frequencies = textProcessor.computeFrequencies(text);

  // TODO: make these tests pass. The normalise function does not convert Ç to ç
  EXPECT_EQ(frequencies.at(L"ça"), 3);
  EXPECT_FALSE(frequencies.contains(L"Ça"));
  EXPECT_EQ(frequencies.at(L"va"), 3);
  EXPECT_EQ(frequencies.at(L"comment"), 1);
}

TEST(TextProcessorTests, ComputeFrequenciesKurdishTest) {
  std::wstring text = LR"(
    - سڵاو، چۆنی؟
    - سڵاو! باشم، تۆ چۆنی؟
    - منیش باشم، سوپاس.
    )";
  auto textProcessor = processor::TextProcessor();

  auto frequencies = textProcessor.computeFrequencies(text);

  // TODO: Get the locale working so the following pass
  EXPECT_EQ(frequencies.at(L"سڵاو"), 2);
  EXPECT_FALSE(frequencies.contains(L"سڵاۆ"));
  EXPECT_EQ(frequencies.at(L"باشم"), 2);
  EXPECT_EQ(frequencies.at(L"چۆنی"), 2);
  EXPECT_EQ(frequencies.at(L"سوپاس"), 1);
}
