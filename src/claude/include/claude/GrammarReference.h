#pragma once

#include <optional>
#include <string>
#include <vector>

namespace claude {
// One `<section id="…" title="…">` block of the grammar reference.
struct GrammarSection {
  std::string id;
  std::string title; // may be empty
  std::string text;  // its Markdown, without the tags
};

// The grammar reference sent with every sentence explanation: Markdown split
// into `<section id="…" title="…">` blocks, whose ids the answers cite (see
// prompts/grammar_outline.md). Built from copyrighted books, so it is read
// from a file at startup rather than compiled in.
struct GrammarReference {
  std::string text; // UTF-8
  // The sections' ids, in order of appearance, without duplicates.
  std::vector<std::string> sectionIds;
  // The sections, in order; an id that appears twice keeps its first.
  std::vector<GrammarSection> sections;

  // Null if there is no section `id`.
  const GrammarSection *section(const std::string &id) const;
};

GrammarReference grammarReferenceFrom(std::string text);
// Empty if the file can't be read or has no text.
std::optional<GrammarReference> loadGrammarReference(const std::string &path);
} // namespace claude
