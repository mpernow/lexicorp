#include "processor/TextProcessor.h"
#include "utils/Language.h"
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

  auto frequencies = textProcessor.computeFrequencies(text,
                                                      utils::Language::English);

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

  auto frequencies = textProcessor.computeFrequencies(text,
                                                      utils::Language::English);

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

  auto frequencies = textProcessor.computeFrequencies(text,
                                                      utils::Language::Kurdish);

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
  EXPECT_TRUE(tp.computeFrequencies(L"", utils::Language::English).empty());
}

TEST(TextProcessorTests, ComputeFrequenciesPunctuationOnly) {
  processor::TextProcessor tp;
  EXPECT_TRUE(tp.computeFrequencies(L"!!! ??? ... ,,, --- ;;;",
                                     utils::Language::English).empty());
}

TEST(TextProcessorTests, ComputeFrequenciesCountsDigitTokensAsWords) {
  processor::TextProcessor tp;
  auto f = tp.computeFrequencies(L"123 456 123 3rd", utils::Language::English);
  EXPECT_EQ(f.at(L"123"), 2);
  EXPECT_EQ(f.at(L"456"), 1);
  EXPECT_EQ(f.at(L"3rd"), 1);
}

TEST(TextProcessorTests, ComputeFrequenciesKeepsInternalApostrophes) {
  processor::TextProcessor tp;
  // U+0027 and U+2019 are explicitly excluded from the strip set, so elided /
  // contracted forms stay as one token.
  auto f = tp.computeFrequencies(L"don't l'homme it’s",
                                 utils::Language::English);
  EXPECT_EQ(f.at(L"don't"), 1);
  EXPECT_EQ(f.at(L"l'homme"), 1);
  EXPECT_EQ(f.at(L"it’s"), 1);
}

TEST(TextProcessorTests, ComputeFrequenciesStripsHyphensBeforeSegmenting) {
  processor::TextProcessor tp;
  // The strip pass runs before word segmentation, so a hyphenated compound
  // collapses into a single token rather than splitting.
  auto f = tp.computeFrequencies(L"well-known state-of-the-art",
                                 utils::Language::English);
  EXPECT_EQ(f.at(L"wellknown"), 1);
  EXPECT_EQ(f.at(L"stateoftheart"), 1);
  EXPECT_FALSE(f.contains(L"well"));
}

TEST(TextProcessorTests, ComputeFrequenciesNfkcFoldsLigatures) {
  processor::TextProcessor tp;
  auto f = tp.computeFrequencies(L"ﬁle office", // U+FB01 = "ﬁ"
                                 utils::Language::English);
  EXPECT_EQ(f.at(L"file"), 1);
  EXPECT_FALSE(f.contains(L"ﬁle"));
}

TEST(TextProcessorTests, ComputeFrequenciesCaseFoldsEszett) {
  processor::TextProcessor tp;
  auto f = tp.computeFrequencies(L"straße GROß", utils::Language::English); // ß
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
  auto f = tp.computeFrequencies(L"چۆنی؟\nباشم، سوپاس؟منیش",
                                 utils::Language::Kurdish);
  EXPECT_EQ(f.at(L"چۆنی"), 1);
  EXPECT_EQ(f.at(L"باشم"), 1);
  EXPECT_EQ(f.at(L"سوپاس"), 1);
  EXPECT_EQ(f.at(L"منیش"), 1);
  EXPECT_EQ(f.size(), 4u);
}

TEST(TextProcessorTests, ComputeFrequenciesLineBreaksAndPunctuationSeparate) {
  processor::TextProcessor tp;
  auto f = tp.computeFrequencies(L"you?\nhi\nthere,friend\tand—more",
                                 utils::Language::English);
  for (const auto *w : {L"you", L"hi", L"there", L"friend", L"and", L"more"})
    EXPECT_EQ(f.at(w), 1) << w;
  EXPECT_EQ(f.size(), 6u);
}

// Variant letters typed on Arabic or older keyboards count as the standard
// Sorani spelling.
TEST(TextProcessorTests, ComputeFrequenciesNormalizesKurdishLetters) {
  processor::TextProcessor tp;
  const auto ckb = utils::Language::Kurdish;

  // Arabic kaf / yeh / alef maksura -> Kurdish keheh / farsi yeh.
  auto f = tp.computeFrequencies(L"\u0643\u062A\u06CE\u0628 "      // كتێب
                                 L"\u06A9\u062A\u06CE\u0628 "      // کتێب
                                 L"\u0628\u0627\u0634\u064A "      // باشي
                                 L"\u0628\u0627\u0634\u0649 "      // باشى
                                 L"\u0628\u0627\u0634\u06CC",      // باشی
                                 ckb);
  EXPECT_EQ(f.at(L"\u06A9\u062A\u06CE\u0628"), 2);
  EXPECT_EQ(f.at(L"\u0628\u0627\u0634\u06CC"), 3);
  EXPECT_EQ(f.size(), 2u);

  // ھ -> ه; heh + ZWNJ and teh marbuta -> ە.
  f = tp.computeFrequencies(L"\u06BE\u0627\u062A "                 // ھات
                            L"\u0647\u0627\u062A "                 // هات
                            L"\u0645\u0627\u0644\u0647\u200C "   // ماله‌
                            L"\u0645\u0627\u0644\u0629 "          // مالة
                            L"\u0645\u0627\u0644\u06D5",          // مالە
                            ckb);
  EXPECT_EQ(f.at(L"\u0647\u0627\u062A"), 2);
  EXPECT_EQ(f.at(L"\u0645\u0627\u0644\u06D5"), 3);
  EXPECT_EQ(f.size(), 2u);
}

TEST(TextProcessorTests, ComputeFrequenciesKeepsWordFinalHeh) {
  processor::TextProcessor tp;
  // A bare final ه may be a real /h/, so it isn't turned into ە.
  auto f = tp.computeFrequencies(L"\u0645\u0627\u0644\u0647",
                                 utils::Language::Kurdish);
  EXPECT_EQ(f.at(L"\u0645\u0627\u0644\u0647"), 1);
}

TEST(TextProcessorTests, KurdishLettersOnlyNormalizedForKurdish) {
  processor::TextProcessor tp;
  auto f = tp.computeFrequencies(L"\u0643\u062A\u0628",
                                 utils::Language::English);
  EXPECT_EQ(f.at(L"\u0643\u062A\u0628"), 1);
}

namespace {
std::vector<std::wstring> sentenceTexts(const std::wstring &input,
                                        utils::Language language) {
  std::vector<std::wstring> texts;
  for (const auto &s : processor::TextProcessor{}.extractSentences(input,
                                                                   language))
    texts.push_back(s.text);
  return texts;
}
} // namespace

TEST(TextProcessorTests, ExtractSentencesSplitsAndCountsWords) {
  processor::TextProcessor tp;
  auto sentences = tp.extractSentences(L"  The cat sat. The cat ran!  Why? ",
                                       utils::Language::English);
  ASSERT_EQ(sentences.size(), 3u);
  EXPECT_EQ(sentences[0].text, L"The cat sat.");
  EXPECT_EQ(sentences[1].text, L"The cat ran!");
  EXPECT_EQ(sentences[2].text, L"Why?");
  EXPECT_EQ(sentences[0].words.at(L"the"), 1);
  EXPECT_EQ(sentences[1].words.at(L"ran"), 1);
  EXPECT_FALSE(sentences[1].words.contains(L"sat"));
}

TEST(TextProcessorTests, ExtractSentencesGivesPositionsInInput) {
  processor::TextProcessor tp;
  // Positions are in code points; 𝒜 (U+1D49C) is two UTF-16 units.
  const std::wstring input = L"𝒜 b.\n\n  Second one. ";
  auto sentences = tp.extractSentences(input, utils::Language::English);
  ASSERT_EQ(sentences.size(), 2u);
  for (const auto &s : sentences)
    EXPECT_EQ(input.substr(s.start, s.length), s.text);
  EXPECT_EQ(sentences[1].start, 8);
}

TEST(TextProcessorTests, ExtractSentencesSkipsSentencesWithoutWords) {
  // Trailing punctuation stays with its sentence ("Hi. ..."); a line of only
  // punctuation is a sentence of its own, and is dropped.
  EXPECT_EQ(sentenceTexts(L"Hi. ...\n* * *\n\nBye.", utils::Language::English),
            (std::vector<std::wstring>{L"Hi. ...", L"Bye."}));
}

TEST(TextProcessorTests, ExtractSentencesKeepsAbbreviations) {
  EXPECT_EQ(sentenceTexts(L"Mr. Smith left. He was tired.",
                          utils::Language::English),
            (std::vector<std::wstring>{L"Mr. Smith left.", L"He was tired."}));
}

TEST(TextProcessorTests, ExtractSentencesSplitsKurdishQuestionMarks) {
  EXPECT_EQ(sentenceTexts(L"سڵاو، چۆنی؟ باشم، سوپاس.\nتۆ چۆنی؟",
                          utils::Language::Kurdish),
            (std::vector<std::wstring>{L"سڵاو، چۆنی؟", L"باشم، سوپاس.",
                                       L"تۆ چۆنی؟"}));
}

TEST(TextProcessorTests, ExtractSentencesEndAtLineBreaks) {
  EXPECT_EQ(sentenceTexts(L"A title\r\nThe first sentence. Second\nthird",
                          utils::Language::English),
            (std::vector<std::wstring>{L"A title", L"The first sentence.",
                                       L"Second", L"third"}));
}

TEST(TextProcessorTests, ComputeFrequenciesSumsSentences) {
  processor::TextProcessor tp;
  const std::wstring text = L"The cat sat. The cat ran.\nA dog barked.";
  auto f = tp.computeFrequencies(text, utils::Language::English);
  EXPECT_EQ(f.at(L"the"), 2);
  EXPECT_EQ(f.at(L"cat"), 2);
  EXPECT_EQ(f.at(L"dog"), 1);
  EXPECT_EQ(f.size(), 7u);
}

TEST(TextProcessorTests, WordsKeepsOrderAndRepeats) {
  processor::TextProcessor tp;
  EXPECT_EQ(tp.words(L"The well-known cat, the CAT!", utils::Language::English),
            (std::vector<std::wstring>{L"the", L"wellknown", L"cat", L"the",
                                       L"cat"}));
  // Letter variants are normalised as for stored words (ي → ی).
  EXPECT_EQ(tp.words(L"پێم وايە", utils::Language::Kurdish),
            (std::vector<std::wstring>{L"پێم", L"وایە"}));
}
