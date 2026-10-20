#include "pages/ClaudeLookup.h"
#include "db/Savepoint.h"
#include "processor/TextProcessor.h"

#include <algorithm>

claude::WordQuery wordQuery(AppContext &appContext, const std::wstring &word,
                            utils::Language language) {
  claude::WordQuery query{word, {}};
  for (const auto &sentence :
       appContext.sentenceRepository->getByWord(word, language)) {
    if (query.sentences.size() == claude::kMaxSentences)
      break;
    if (std::ranges::find(query.sentences, sentence.sentence) ==
        query.sentences.end())
      query.sentences.push_back(sentence.sentence);
  }
  return query;
}

namespace {
std::wstring joined(const std::vector<std::wstring> &words) {
  std::wstring result;
  for (const auto &word : words)
    result += (result.empty() ? L"" : L" ") + word;
  return result;
}

// True if `words` occur in `sentence` in this order, with or without other
// words between them.
bool occursInOrder(const std::vector<std::wstring> &sentence,
                   const std::vector<std::wstring> &words) {
  auto next = sentence.begin();
  for (const auto &word : words) {
    next = std::find(next, sentence.end(), word);
    if (next == sentence.end())
      return false;
    ++next;
  }
  return true;
}
} // namespace

void saveLookup(AppContext &appContext, utils::Language language,
                const claude::WordQuery &query,
                const claude::WordLookup &lookup) {
  db::Savepoint tx{appContext.dbHandler->getConnection(), "save_lookup"};

  if (lookup.usage)
    appContext.claudeUsageRepository->add(db::models::ClaudeUsage{
        .model = lookup.usage->model,
        .purpose = "define_word",
        .inputTokens = lookup.usage->inputTokens,
        .outputTokens = lookup.usage->outputTokens,
        .cacheReadTokens = lookup.usage->cacheReadTokens,
        .cacheCreationTokens = lookup.usage->cacheCreationTokens,
    });

  if (lookup.error.empty() && !lookup.senses.empty()) {
    std::vector<db::models::DictionaryEntry> entries;
    for (const auto &sense : lookup.senses)
      entries.push_back(db::models::DictionaryEntry{
          .gloss = sense.gloss,
          .partOfSpeech = sense.partOfSpeech,
          .aiGenerated = true,
      });
    appContext.dictionaryRepository->replaceEntries(
        query.word, language, db::models::DictionarySource::Claude, entries);
  }

  processor::TextProcessor textProcessor;
  std::vector<std::vector<std::wstring>> sentenceWords;
  for (const auto &sentence : query.sentences)
    sentenceWords.push_back(textProcessor.words(sentence, language));

  for (const auto &idiom : lookup.idioms) {
    auto words = textProcessor.words(idiom.expression, language);
    if (words.size() < 2 || words.size() > kMaxExpressionWords ||
        std::ranges::find(words, query.word) == words.end())
      continue;
    if (std::ranges::none_of(sentenceWords, [&](const auto &sentence) {
          return occursInOrder(sentence, words);
        }))
      continue;
    auto dictionaryWords = textProcessor.words(idiom.dictionaryForm, language);
    if (dictionaryWords.empty() ||
        dictionaryWords.size() > kMaxExpressionWords)
      dictionaryWords = words;

    auto expression = joined(dictionaryWords);
    appContext.dictionaryRepository->replaceEntries(
        expression, language, db::models::DictionarySource::Claude,
        {db::models::DictionaryEntry{.gloss = idiom.gloss,
                                     .partOfSpeech = L"idiom",
                                     .aiGenerated = true}});
    for (const auto *forms : {&words, &dictionaryWords})
      for (const auto &word : *forms)
        appContext.expressionWordRepository->add(
            db::models::ExpressionWord{word, language, expression});
  }

  tx.commit();
}
