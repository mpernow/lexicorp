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

icu::Locale TextProcessor::locale(utils::Language language) {
  // ss=standard: don't end a sentence after common abbreviations ("Mr.").
  switch (language) {
  case utils::Language::English:
    return icu::Locale("en@ss=standard");
  case utils::Language::French:
    return icu::Locale("fr@ss=standard");
  case utils::Language::Kurdish:
    return icu::Locale("ckb");
  default:
    return icu::Locale::getRoot();
  }
}

std::vector<Sentence>
TextProcessor::extractSentences(const std::wstring &input,
                                utils::Language language) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale loc = locale(language);
  std::unique_ptr<icu::BreakIterator> sentenceIterator(
      icu::BreakIterator::createSentenceInstance(loc, status));
  std::unique_ptr<icu::BreakIterator> wordIterator(
      icu::BreakIterator::createWordInstance(loc, status));
  if (U_FAILURE(status) || !sentenceIterator || !wordIterator)
    throw std::runtime_error("Failed to create ICU BreakIterator");

  icu::UnicodeString text = toUnicode(input);
  sentenceIterator->setText(text);

  std::vector<Sentence> sentences;
  int32_t start = sentenceIterator->first();
  for (int32_t end = sentenceIterator->next(); end != icu::BreakIterator::DONE;
       start = end, end = sentenceIterator->next()) {
    int32_t first = start, last = end;
    while (first < last && u_isUWhiteSpace(text.char32At(first)))
      first = text.moveIndex32(first, 1);
    while (last > first && u_isUWhiteSpace(text.char32At(last - 1)))
      last = text.moveIndex32(last, -1);

    icu::UnicodeString sentence = text.tempSubStringBetween(first, last);
    auto words = countWords(sentence, language, *wordIterator);
    if (words.empty())
      continue;

    sentences.push_back(Sentence{
        .text = toWString(sentence),
        .start = text.countChar32(0, first),
        .length = sentence.countChar32(),
        .words = std::move(words),
    });
  }
  return sentences;
}

std::unordered_map<std::wstring, int>
TextProcessor::computeFrequencies(const std::wstring &input,
                                  utils::Language language) {
  return sumFrequencies(extractSentences(input, language));
}

std::unordered_map<std::wstring, int>
TextProcessor::sumFrequencies(const std::vector<Sentence> &sentences) {
  std::unordered_map<std::wstring, int> results;
  for (const auto &sentence : sentences)
    for (const auto &[word, count] : sentence.words)
      results[word] += count;
  return results;
}

std::unordered_map<std::wstring, int>
TextProcessor::countWords(const icu::UnicodeString &sentence,
                          utils::Language language,
                          icu::BreakIterator &wordIterator) {
  icu::UnicodeString text = foldCase(normalize(sentence, language));
  wordIterator.setText(text);

  std::unordered_map<std::wstring, int> results;
  int32_t start = wordIterator.first();
  for (int32_t end = wordIterator.next(); end != icu::BreakIterator::DONE;
       start = end, end = wordIterator.next()) {
    icu::UnicodeString token = text.tempSubStringBetween(start, end);

    if (isWord(token))
      results[toWString(token)]++;
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
