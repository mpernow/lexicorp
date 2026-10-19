#pragma once

#include "pages/AppContext.h"
#include "processor/TextProcessor.h"
#include "utils/Language.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

// Stores a text's sentences (numbered in order) and links each to the words
// it contains. Only words in `includedWords` are linked, matching the words
// the text was saved with. Call inside the caller's transaction.
void saveSentences(AppContext &appContext, std::int64_t textHash,
                   utils::Language language,
                   const std::vector<processor::Sentence> &sentences,
                   const std::unordered_set<std::wstring> &includedWords);
