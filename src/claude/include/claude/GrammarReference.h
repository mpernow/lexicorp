#pragma once

#include <optional>
#include <string>
#include <vector>

namespace claude {
// The grammar reference sent with every sentence explanation: Markdown split
// into `<section id="…" title="…">` blocks, whose ids the answers cite (see
// prompts/grammar_outline.md). Built from copyrighted books, so it is read
// from a file at startup rather than compiled in.
struct GrammarReference {
  std::string text; // UTF-8
  // The sections' ids, in order of appearance, without duplicates.
  std::vector<std::string> sectionIds;
};

GrammarReference grammarReferenceFrom(std::string text);
// Empty if the file can't be read or has no text.
std::optional<GrammarReference> loadGrammarReference(const std::string &path);
} // namespace claude
