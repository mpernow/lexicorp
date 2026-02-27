#pragma once
#include <string>
#include <unordered_map>

namespace processor {
class TextProcessor {
public:
  TextProcessor();

  std::unordered_map<std::string, int>
  computeFrequencies(const std::string &text);

private:
  std::string normalize(const std::string &text);
};
} // namespace processor
