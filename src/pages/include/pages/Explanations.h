#pragma once

#include "claude/ClaudeClient.h"
#include "db/Sentence.h"
#include "pages/AppContext.h"
#include "processor/TextProcessor.h"

#include <Wt/WContainerWidget.h>

#include <cstdint>
#include <memory>
#include <string>

// The sentence with the ones before and after it in its text, for context.
claude::SentenceQuery sentenceQuery(AppContext &appContext,
                                    const db::models::Sentence &sentence);

// The key of a sentence's explanation: the same sentence in another text
// shares it.
std::int64_t sentenceHash(processor::TextProcessor &processor,
                          const std::wstring &sentence);

// Stores the request's usage and, if Claude answered, the explanation under
// `hash` (the sentence's sentenceHash()), in one transaction.
void saveExplanation(AppContext &appContext, std::int64_t hash,
                     const claude::SentenceQuery &query,
                     const claude::SentenceExplanation &explanation);

// The explanation as shown under its sentence: the translation, the words
// interlinear (word, Leipzig gloss, base form and gloss), the parts of each
// word that has several, the constructions and the notes, with the grammar
// sections they cite. `model` is the one that answered. With `grammar`
// (which must outlive the widget), a "Sources" list ends it: each cited
// section, opening its text in a dialog.
std::unique_ptr<Wt::WContainerWidget>
explanationWidget(const claude::SentenceExplanation &explanation,
                  const std::string &model,
                  const claude::GrammarReference *grammar = nullptr);
