#pragma once

#include "claude/ClaudeClient.h"
#include "pages/AppContext.h"
#include "utils/Language.h"

#include <string>

// Claude only looks up words of this language: the prompt is for Sorani.
inline constexpr utils::Language kClaudeLanguage = utils::Language::Kurdish;
// Longer "idioms" are clauses Claude quoted whole, not expressions.
inline constexpr std::size_t kMaxExpressionWords = 5;

// The word with the first stored sentences it occurs in.
claude::WordQuery wordQuery(AppContext &appContext, const std::wstring &word,
                            utils::Language language);

// Stores what Claude answered, in one transaction: its glosses as the word's
// `claude` entries, its idioms as entries of their own (under their
// dictionary form) linked to each of their words in both forms, and the
// request's usage. An idiom is dropped unless it has 2 to
// kMaxExpressionWords words, including `query.word`, that occur in that order
// (other words may come between them) in one of the sentences.
void saveLookup(AppContext &appContext, utils::Language language,
                const claude::WordQuery &query,
                const claude::WordLookup &lookup);
