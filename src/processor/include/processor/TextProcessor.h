#pragma once

#include <memory>
#include <openssl/sha.h>
#include <string>
#include <unordered_map>

#include <unicode/brkiter.h>
#include <unicode/normalizer2.h>
#include <unicode/translit.h>
#include <unicode/uchar.h>
#include <unicode/uniset.h>
#include <unicode/unistr.h>

#include "utils/Language.h"

namespace processor {
class TextProcessor {
public:
  TextProcessor();

  std::unordered_map<std::wstring, int>
  computeFrequencies(const std::wstring &input, utils::Language language);

  // Maps a language's variant spellings of letters to one form, so a word
  // typed on different keyboards is counted once. Applied by
  // computeFrequencies(); also needed to match stored words against raw text.
  void normalizeLetters(icu::UnicodeString &text,
                        utils::Language language) const;

  uint64_t hashWstring(const std::wstring &input);

private:
  icu::UnicodeString toUnicode(const std::wstring &w);
  std::wstring toWString(const icu::UnicodeString &u);

  icu::UnicodeString normalize(const icu::UnicodeString &input,
                               utils::Language language);
  icu::UnicodeString foldCase(const icu::UnicodeString &input);

  bool isWord(const icu::UnicodeString &token);

private:
  const icu::Normalizer2 *mNormalizer;
  std::unique_ptr<icu::BreakIterator> mWordIterator;
  std::unique_ptr<icu::Transliterator> mKurdishLetters;
  icu::UnicodeSet mJoinSet;
  icu::UnicodeSet mSeparatorSet;
};
} // namespace processor
