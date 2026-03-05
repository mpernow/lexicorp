#include "processor/TextProcessor.h"
#include <stdexcept>
#include <vector>

namespace processor {
TextProcessor::TextProcessor() {
  UErrorCode status = U_ZERO_ERROR;

  mNormalizer = icu::Normalizer2::getNFKCInstance(status);
  if (U_FAILURE(status))
    throw std::runtime_error("Failed to initialize ICU Normalizer2");

  mWordIterator.reset(icu::BreakIterator::createWordInstance(
      icu::Locale::getDefault(), status));

  if (U_FAILURE(status) || !mWordIterator)
    throw std::runtime_error("Failed to create ICU BreakIterator");

  icu::UnicodeString pattern = UNICODE_STRING_SIMPLE("[[:P:][:S:][:Cc:]-['’]]");
  mRemoveSet = icu::UnicodeSet(pattern, status);
  if (U_FAILURE(status))
    throw std::runtime_error("Failed to init UnicodeSet");
  mRemoveSet.freeze();
}

std::unordered_map<std::wstring, int>
TextProcessor::computeFrequencies(const std::wstring &input) {
  std::vector<std::wstring> words;

  icu::UnicodeString text = toUnicode(input);

  text = normalize(text);
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

icu::UnicodeString TextProcessor::normalize(const icu::UnicodeString &input) {
  UErrorCode status = U_ZERO_ERROR;

  icu::UnicodeString normalized;
  mNormalizer->normalize(input, normalized, status);

  if (U_FAILURE(status))
    throw std::runtime_error("ICU normalization failed");

  icu::UnicodeString cleaned;
  for (int32_t i = 0; i < normalized.length();) {
    UChar32 c = normalized.char32At(i);

    if (!mRemoveSet.contains(c)) {
      cleaned.append(c);
    }

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
