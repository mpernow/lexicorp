#pragma once

#include <memory>
#include <openssl/sha.h>
#include <string>
#include <unordered_map>

#include <unicode/brkiter.h>
#include <unicode/normalizer2.h>
#include <unicode/uchar.h>
#include <unicode/uniset.h>
#include <unicode/unistr.h>

namespace processor {
class TextProcessor {
public:
  TextProcessor();

  std::unordered_map<std::wstring, int>
  computeFrequencies(const std::wstring &input);

  uint64_t hashWstring(const std::wstring &input);

private:
  icu::UnicodeString toUnicode(const std::wstring &w);
  std::wstring toWString(const icu::UnicodeString &u);

  icu::UnicodeString normalize(const icu::UnicodeString &input);
  icu::UnicodeString foldCase(const icu::UnicodeString &input);

  bool isWord(const icu::UnicodeString &token);

private:
  const icu::Normalizer2 *mNormalizer;
  std::unique_ptr<icu::BreakIterator> mWordIterator;
  icu::UnicodeSet mRemoveSet;
};
} // namespace processor
