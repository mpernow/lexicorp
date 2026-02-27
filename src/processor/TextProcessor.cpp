#include "processor/TextProcessor.h"
#include <ranges>

namespace processor {
TextProcessor::TextProcessor() = default;

std::unordered_map<std::string, int>
TextProcessor::computeFrequencies(const std::string &text) {
  std::string normalizedText = normalize(text);

  std::unordered_map<std::string, int> wordCount;

  auto words =
      normalizedText | std::views::split(' ') |
      std::views::transform([](auto &&wordRange) {
        std::string word;

        for (char c : wordRange) {
          if (!std::ispunct(static_cast<unsigned char>(c))) {
            word += std::tolower(static_cast<unsigned char>(c));
          }
        }

        return word;
      }) |
      std::views::filter([](const std::string &word) { return !word.empty(); });

  for (const auto &word : words) {
    wordCount[word]++;
  }

  return wordCount;
}

std::string TextProcessor::normalize(const std::string &text) {
  std::string normalized;
  normalized.reserve(text.size());

  bool in_space = false;
  for (unsigned char c : text) {
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
