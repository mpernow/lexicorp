#pragma once

#include "db/DictionaryEntry.h"
#include "pages/AppContext.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// What to show as a word's gloss: its entries from the preferred source (see
// DictionarySource), or, for a word without entries of its own, those of a
// lemma it is a form of.
struct Gloss {
  std::vector<db::models::DictionaryEntry> entries; // one source, in order
  std::wstring lemma; // set when the entries are the lemma's
};

// The gloss of every word of the selected language that has one.
std::unordered_map<std::wstring, Gloss>
glossesOfStoredWords(AppContext &appContext);

// The gloss of one word of the selected language.
std::optional<Gloss> glossOf(AppContext &appContext, const std::wstring &word);

// The gloss on one line (UTF-8): its distinct glosses joined by "; ",
// introduced by "form of <lemma>: " for a lemma's.
std::string glossSummary(const Gloss &gloss);
