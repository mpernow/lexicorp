#include "claude/GrammarReference.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

namespace claude {
const GrammarSection *GrammarReference::section(const std::string &id) const {
  auto it = std::ranges::find(sections, id, &GrammarSection::id);
  return it == sections.end() ? nullptr : &*it;
}

GrammarReference grammarReferenceFrom(std::string text) {
  // Only the opening tag goes through the regex: matching up to the closing
  // tag too would recurse once per character in libstdc++'s regex.
  static const std::regex sectionTag{
      R"re(<section\s+id="([^"]+)"(?:\s+title="([^"]*)")?[^>]*>)re"};
  GrammarReference reference{std::move(text), {}, {}};
  const auto &all = reference.text;
  for (auto it = std::sregex_iterator(all.begin(), all.end(), sectionTag);
       it != std::sregex_iterator(); ++it) {
    auto id = (*it)[1].str();
    if (std::ranges::find(reference.sectionIds, id) !=
        reference.sectionIds.end())
      continue;
    reference.sectionIds.push_back(id);

    auto start = static_cast<std::size_t>(it->position() + it->length());
    auto end = all.find("</section>", start);
    auto body = all.substr(start, end == std::string::npos ? std::string::npos
                                                           : end - start);
    auto first = body.find_first_not_of("\r\n");
    auto last = body.find_last_not_of(" \t\r\n");
    body = first == std::string::npos ? ""
                                      : body.substr(first, last + 1 - first);
    reference.sections.push_back({std::move(id), (*it)[2].str(),
                                  std::move(body)});
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
