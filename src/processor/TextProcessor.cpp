#include "processor/TextProcessor.h"
#include <ranges>

namespace processor {
TextProcessor::TextProcessor() = default;

std::unordered_map<std::wstring, int>
TextProcessor::computeFrequencies(const std::wstring &text) {
  std::wstring normalizedText = normalize(text);

  std::unordered_map<std::wstring, int> wordCount;

  auto words = normalizedText | std::views::split(' ') |
               std::views::transform([](auto &&wordRange) {
                 std::wstring word;

                 for (wchar_t c : wordRange) {
                   if (!std::ispunct(static_cast<wchar_t>(c))) {
                     word += std::tolower(static_cast<wchar_t>(c));
                   }
                 }
                 return word;
               }) |
               std::views::filter(
                   [](const std::wstring &word) { return !word.empty(); });

  for (const auto &word : words) {
    wordCount[word]++;
  }

  return wordCount;
}

std::wstring TextProcessor::normalize(const std::wstring &text) {
  std::wstring normalized;
  normalized.reserve(text.size());

  bool in_space = false;
  for (wchar_t c : text) {
    if (std::isspace(c)) {
      if (!in_space) {
        normalized += ' ';
        in_space = true;
      }
    } else {
      normalized += c;
      in_space = false;
    }
  }
  return normalized;
}

} // namespace processor
