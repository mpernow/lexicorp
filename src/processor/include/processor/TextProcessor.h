#pragma once

#include <memory>
#include <openssl/sha.h>
#include <string>
#include <unordered_map>
#include <vector>

#include <unicode/brkiter.h>
#include <unicode/normalizer2.h>
#include <unicode/translit.h>
#include <unicode/uchar.h>
#include <unicode/uniset.h>
#include <unicode/unistr.h>

#include "utils/Language.h"

namespace processor {
struct Sentence {
  std::wstring text; // trimmed
  // Position of `text` in the input, in code points.
  int start = 0;
  int length = 0;
  std::unordered_map<std::wstring, int> words;
};

class TextProcessor {
public:
  TextProcessor();

  // The ICU locale for a language's word and sentence boundaries.
  static icu::Locale locale(utils::Language language);

  // The input's sentences that contain words, in order, each with its word
  // counts. Every line break ends a sentence, so a title on its own line
  // stays separate (but hard-wrapped text is split at each line end).
  std::vector<Sentence> extractSentences(const std::wstring &input,
                                         utils::Language language);

  // Word counts of the whole input: the sum over extractSentences().
  std::unordered_map<std::wstring, int>
  computeFrequencies(const std::wstring &input, utils::Language language);
  static std::unordered_map<std::wstring, int>
  sumFrequencies(const std::vector<Sentence> &sentences);

  // Maps a language's variant spellings of letters to one form, so a word
  // typed on different keyboards is counted once. Applied by
  // computeFrequencies(); also needed to match stored words against raw text.
  void normalizeLetters(icu::UnicodeString &text,
                        utils::Language language) const;

  uint64_t hashWstring(const std::wstring &input);

private:
  std::unordered_map<std::wstring, int>
  countWords(const icu::UnicodeString &sentence, utils::Language language,
             icu::BreakIterator &wordIterator);

  icu::UnicodeString toUnicode(const std::wstring &w);
  std::wstring toWString(const icu::UnicodeString &u);

  icu::UnicodeString normalize(const icu::UnicodeString &input,
                               utils::Language language);
  icu::UnicodeString foldCase(const icu::UnicodeString &input);

  bool isWord(const icu::UnicodeString &token);

private:
  const icu::Normalizer2 *mNormalizer;
  std::unique_ptr<icu::Transliterator> mKurdishLetters;
  icu::UnicodeSet mJoinSet;
  icu::UnicodeSet mSeparatorSet;
};
} // namespace processor
