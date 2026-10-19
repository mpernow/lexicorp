#include "processor/TextProcessor.h"
#include <cstring>
#include <stdexcept>
#include <vector>

namespace processor {
namespace {
// Sorani Kurdish (Central Kurdish) letter variants, as ICU transliterator
// rules. Arabic kaf and yeh (Sorani only uses the Persian-style ک and ی):
//   ك → ک, ي → ی, ى → ی
// Heh doachashmee is another way of writing /h/:
//   ھ → ه
// Before U+06D5 (ە) was widely available, the vowel /e/ was typed as heh plus
// a zero-width non-joiner, which looks the same; teh marbuta was also used:
//   ه + ZWNJ → ە, ة → ە
// A bare word-final ه is left alone: it is usually /e/ in older texts but a
// real /h/ in some words, so mapping it would corrupt those.
const char16_t *const kKurdishLetterRules =
    u"[\\u0647\\u06BE] \\u200C > \\u06D5;"
    u"\\u06BE > \\u0647;"
    u"\\u0629 > \\u06D5;"
    u"\\u0643 > \\u06A9;"
    u"[\\u064A \\u0649] > \\u06CC;";
} // namespace

TextProcessor::TextProcessor() {
  UErrorCode status = U_ZERO_ERROR;

  mNormalizer = icu::Normalizer2::getNFKCInstance(status);
  if (U_FAILURE(status))
    throw std::runtime_error("Failed to initialize ICU Normalizer2");

  mWordIterator.reset(icu::BreakIterator::createWordInstance(
      icu::Locale::getDefault(), status));

  if (U_FAILURE(status) || !mWordIterator)
    throw std::runtime_error("Failed to create ICU BreakIterator");

  // Hyphens are dropped, joining compounds ("well-known" -> "wellknown").
  // Other punctuation, symbols and control characters (incl. line breaks)
  // become spaces, so they still separate the words around them.
  mJoinSet = icu::UnicodeSet(
      UNICODE_STRING_SIMPLE("[\\-\\u2010\\u2011]"), status);
  mSeparatorSet = icu::UnicodeSet(
      UNICODE_STRING_SIMPLE("[[:P:][:S:][:Cc:]-['’]]"), status);
  if (U_FAILURE(status))
    throw std::runtime_error("Failed to init UnicodeSet");
  mSeparatorSet.removeAll(mJoinSet);
  mJoinSet.freeze();
  mSeparatorSet.freeze();

  UParseError parseError;
  mKurdishLetters.reset(icu::Transliterator::createFromRules(
      "ckb-letters", icu::UnicodeString(kKurdishLetterRules), UTRANS_FORWARD,
      parseError, status));
  if (U_FAILURE(status) || !mKurdishLetters)
    throw std::runtime_error("Failed to create Kurdish letter transliterator");
}

std::unordered_map<std::wstring, int>
TextProcessor::computeFrequencies(const std::wstring &input,
                                  utils::Language language) {
  std::vector<std::wstring> words;

  icu::UnicodeString text = toUnicode(input);

  text = normalize(text, language);
  text = foldCase(text);

  mWordIterator->setText(text);

  int32_t start = mWordIterator->first();

  for (int32_t end = mWordIterator->next(); end != icu::BreakIterator::DONE;
       start = end, end = mWordIterator->next()) {
    icu::UnicodeString token = text.tempSubStringBetween(start, end);

    if (isWord(token))
      words.push_back(toWString(token));
  }
  std::unordered_map<std::wstring, int> results;

  for (const auto &t : words) {
    results[t]++;
  }

  return results;
}

uint64_t TextProcessor::hashWstring(const std::wstring &input) {
  std::string utf8;
  icu::UnicodeString icuString = toUnicode(input);
  icuString.toUTF8String(utf8);

  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char *>(utf8.data()), utf8.size(),
         hash);

  uint64_t result = 0;
  std::memcpy(&result, hash, sizeof(result));
  return result;
}

icu::UnicodeString TextProcessor::toUnicode(const std::wstring &w) {
  return icu::UnicodeString::fromUTF32(
      reinterpret_cast<const UChar32 *>(w.c_str()),
      static_cast<int32_t>(w.length()));
}

std::wstring TextProcessor::toWString(const icu::UnicodeString &u) {
  std::wstring ws;
  ws.reserve(u.countChar32());

  for (int32_t i = 0; i < u.length();) {
    ws.push_back(static_cast<wchar_t>(u.char32At(i)));
    i += U16_LENGTH(u.char32At(i));
  }

  return ws;
}

void TextProcessor::normalizeLetters(icu::UnicodeString &text,
                                     utils::Language language) const {
  if (language == utils::Language::Kurdish)
    mKurdishLetters->transliterate(text);
}

icu::UnicodeString TextProcessor::normalize(const icu::UnicodeString &input,
                                            utils::Language language) {
  UErrorCode status = U_ZERO_ERROR;

  icu::UnicodeString normalized;
  mNormalizer->normalize(input, normalized, status);

  if (U_FAILURE(status))
    throw std::runtime_error("ICU normalization failed");

  // After NFKC, which turns Arabic presentation forms into plain letters; and
  // before stripping, which would remove the ZWNJ in heh + ZWNJ.
  normalizeLetters(normalized, language);

  icu::UnicodeString cleaned;
  for (int32_t i = 0; i < normalized.length();) {
    UChar32 c = normalized.char32At(i);

    if (mSeparatorSet.contains(c))
      cleaned.append(u' ');
    else if (!mJoinSet.contains(c))
      cleaned.append(c);

    i = normalized.moveIndex32(i, 1);
  }

  return cleaned;
}

icu::UnicodeString TextProcessor::foldCase(const icu::UnicodeString &input) {
  icu::UnicodeString result = input;
  result.foldCase();
  return result;
}

bool TextProcessor::isWord(const icu::UnicodeString &token) {
  for (int32_t i = 0; i < token.length();) {
    UChar32 c = token.char32At(i);

    if (u_isalnum(c))
      return true;

    i += U16_LENGTH(c);
  }

  return false;
}
} // namespace processor
