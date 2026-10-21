#include "claude/GrammarReference.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

namespace claude {
GrammarReference grammarReferenceFrom(std::string text) {
  static const std::regex sectionId{R"re(<section\s+id="([^"]+)")re"};
  GrammarReference reference{std::move(text), {}};
  for (auto it = std::sregex_iterator(reference.text.begin(),
                                      reference.text.end(), sectionId);
       it != std::sregex_iterator(); ++it) {
    auto id = (*it)[1].str();
    if (std::ranges::find(reference.sectionIds, id) ==
        reference.sectionIds.end())
      reference.sectionIds.push_back(std::move(id));
  }
  return reference;
}

std::optional<GrammarReference> loadGrammarReference(const std::string &path) {
  std::ifstream file{path, std::ios::binary};
  if (!file)
    return std::nullopt;
  std::ostringstream text;
  text << file.rdbuf();
  if (text.str().find_first_not_of(" \t\r\n") == std::string::npos)
    return std::nullopt;
  return grammarReferenceFrom(std::move(text).str());
}
} // namespace claude
