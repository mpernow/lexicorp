#include "pages/Sentences.h"
#include "db/Sentence.h"
#include "db/WordSentence.h"

void saveSentences(AppContext &appContext, std::int64_t textHash,
                   utils::Language language,
                   const std::vector<processor::Sentence> &sentences,
                   const std::unordered_set<std::wstring> &includedWords) {
  int ordinal = 0;
  for (const auto &sentence : sentences) {
    appContext.sentenceRepository->add(db::models::Sentence{
        textHash, ordinal, sentence.text, sentence.start, sentence.length});
    for (const auto &[word, count] : sentence.words) {
      if (includedWords.contains(word))
        appContext.wordSentenceRepository->add(
            db::models::WordSentence{word, language, textHash, ordinal, count});
    }
    ++ordinal;
  }
}
