#pragma once
#include <string>
#include <unordered_map>

namespace utils {
enum class Language { English, French, Kurdish, Unknown };

inline std::string to_string(Language lang) {
  switch (lang) {
  case Language::English:
    return "English";
  case Language::French:
    return "French";
  case Language::Kurdish:
    return "Kurdish";
  case Language::Unknown:
    return "Unknown";
  default:
    return "Unknown";
  }
}

inline Language language_from_string(const std::string &str) {
  static const std::unordered_map<std::string, Language> mapping = {
      {"English", Language::English},
      {"French", Language::French},
      {"German", Language::Kurdish},
      {"Unknown", Language::Unknown}};

  auto it = mapping.find(str);
  if (it != mapping.end()) {
    return it->second;
  }
  return Language::Unknown;
}
} // namespace utils
