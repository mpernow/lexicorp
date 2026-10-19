#include "processor/TextProcessor.h"
#include <gtest/gtest.h>
#include <string>
#include <unordered_map>

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

TEST(TextProcessorTests, ComputeFrequenciesEmptyInput) {
  processor::TextProcessor tp;
  EXPECT_TRUE(tp.computeFrequencies(L"").empty());
}

TEST(TextProcessorTests, ComputeFrequenciesPunctuationOnly) {
  processor::TextProcessor tp;
  EXPECT_TRUE(tp.computeFrequencies(L"!!! ??? ... ,,, --- ;;;").empty());
}

TEST(TextProcessorTests, ComputeFrequenciesCountsDigitTokensAsWords) {
  processor::TextProcessor tp;
  auto f = tp.computeFrequencies(L"123 456 123 3rd");
  EXPECT_EQ(f.at(L"123"), 2);
  EXPECT_EQ(f.at(L"456"), 1);
  EXPECT_EQ(f.at(L"3rd"), 1);
}

TEST(TextProcessorTests, ComputeFrequenciesKeepsInternalApostrophes) {
  processor::TextProcessor tp;
  // U+0027 and U+2019 are explicitly excluded from the strip set, so elided /
  // contracted forms stay as one token.
  auto f = tp.computeFrequencies(L"don't l'homme it’s");
  EXPECT_EQ(f.at(L"don't"), 1);
  EXPECT_EQ(f.at(L"l'homme"), 1);
  EXPECT_EQ(f.at(L"it’s"), 1);
}

TEST(TextProcessorTests, ComputeFrequenciesStripsHyphensBeforeSegmenting) {
  processor::TextProcessor tp;
  // The strip pass runs before word segmentation, so a hyphenated compound
  // collapses into a single token rather than splitting.
  auto f = tp.computeFrequencies(L"well-known state-of-the-art");
  EXPECT_EQ(f.at(L"wellknown"), 1);
  EXPECT_EQ(f.at(L"stateoftheart"), 1);
  EXPECT_FALSE(f.contains(L"well"));
}

TEST(TextProcessorTests, ComputeFrequenciesNfkcFoldsLigatures) {
  processor::TextProcessor tp;
  auto f = tp.computeFrequencies(L"ﬁle office"); // U+FB01 = "ﬁ"
  EXPECT_EQ(f.at(L"file"), 1);
  EXPECT_FALSE(f.contains(L"ﬁle"));
}

TEST(TextProcessorTests, ComputeFrequenciesCaseFoldsEszett) {
  processor::TextProcessor tp;
  auto f = tp.computeFrequencies(L"straße GROß"); // ß
  EXPECT_EQ(f.at(L"strasse"), 1);
  EXPECT_EQ(f.at(L"gross"), 1);
}

TEST(TextProcessorTests, HashStableAcrossInstances) {
  const std::wstring text = L"a stable sentence.";
  EXPECT_EQ(processor::TextProcessor{}.hashWstring(text),
            processor::TextProcessor{}.hashWstring(text));
}

// Golden values: hashWstring is SHA-256 over the UTF-8 encoding with the first
// 8 bytes reinterpreted as a little-endian uint64. If normalization, the
// digest, or the byte width changes, these break.
TEST(TextProcessorTests, HashGoldenValues) {
  processor::TextProcessor tp;
  EXPECT_EQ(tp.hashWstring(L""), 1449310910991872227ULL);
  EXPECT_EQ(tp.hashWstring(L"hello"), 1054880662928880172ULL);
}

TEST(TextProcessorTests, ComputeFrequenciesKurdishQuestionMarkSeparatesWords) {
  processor::TextProcessor tp;
  auto f = tp.computeFrequencies(L"چۆنی؟\nباشم، سوپاس؟منیش");
  EXPECT_EQ(f.at(L"چۆنی"), 1);
  EXPECT_EQ(f.at(L"باشم"), 1);
  EXPECT_EQ(f.at(L"سوپاس"), 1);
  EXPECT_EQ(f.at(L"منیش"), 1);
  EXPECT_EQ(f.size(), 4u);
}

TEST(TextProcessorTests, ComputeFrequenciesLineBreaksAndPunctuationSeparate) {
  processor::TextProcessor tp;
  auto f = tp.computeFrequencies(L"you?\nhi\nthere,friend\tand—more");
  for (const auto *w : {L"you", L"hi", L"there", L"friend", L"and", L"more"})
    EXPECT_EQ(f.at(w), 1) << w;
  EXPECT_EQ(f.size(), 6u);
}
