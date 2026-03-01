#pragma once
#include <string>
#include <unordered_map>

namespace processor {
class TextProcessor {
public:
  TextProcessor();

  std::unordered_map<std::wstring, int>
  computeFrequencies(const std::wstring &text);

private:
  std::wstring normalize(const std::wstring &text);
};
} // namespace processor
