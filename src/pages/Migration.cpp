#include "pages/Migration.h"
#include "db/Savepoint.h"
#include "db/Utils.h"
#include "pages/Sentences.h"
#include "processor/TextProcessor.h"
#include "sqlite/command.hpp"
#include "sqlite/execute.hpp"
#include "sqlite/query.hpp"

#include <string>
#include <unordered_set>

namespace {
int userVersion(sqlite::connection &connection) {
  sqlite::query q{connection, "PRAGMA user_version"};
  for (auto &row : q.each())
    return row.get<int>(0);
  return 0;
}

// Replaces the text's sentences, linking them to the words it was saved with
// (its word_text rows).
void regenerateSentences(AppContext &appContext,
                         processor::TextProcessor &processor,
                         const db::models::Text &text, std::int64_t hash) {
  std::unordered_set<std::wstring> savedWords;
  for (const auto &wordText :
       appContext.wordTextRepository->getAllByHash(hash))
    savedWords.insert(wordText.word);

  appContext.sentenceRepository->removeByHash(hash);
  appContext.wordSentenceRepository->removeByHash(hash);
  saveSentences(appContext, hash, text.language,
                processor.extractSentences(text.text, text.language),
                savedWords);
}

// Version 1:
// - Text hashes are the full 64 bits of hashWstring(); they used to be cut
//   to 32 bits. The stored texts are rehashed, and their word links moved.
// - Every text gets its sentences, linked to the words it was saved with.
void migrateToVersion1(AppContext &appContext) {
  auto &connection = appContext.dbHandler->getConnection();
  processor::TextProcessor processor;

  for (const auto &text : appContext.textRepository->getAll()) {
    auto hash = static_cast<std::int64_t>(processor.hashWstring(text.text));
    if (hash != text.hash) {
      for (const char *sql : {"UPDATE text SET hash = ? WHERE hash = ?",
                              "UPDATE word_text SET text_hash = ? "
                              "WHERE text_hash = ?"}) {
        sqlite::command update{connection, sql};
        update % hash % text.hash;
        update();
      }
    }

    regenerateSentences(appContext, processor, text, hash);
  }
}

// Version 2: Kurdish words stored before letter normalisation (see
// TextProcessor::normalizeLetters) get the normalised spelling, their counts
// merged into it where both exist. Version 1 linked sentences by the old
// spellings, so every Kurdish text's sentences are regenerated too.
void migrateToVersion2(AppContext &appContext) {
  auto &connection = appContext.dbHandler->getConnection();
  processor::TextProcessor processor;
  const auto kurdish = utils::Language::Kurdish;

  for (const auto &word : appContext.wordRepository->getAll(kurdish)) {
    std::string utf8Word = db::Utils::toUtf8(word.word);
    icu::UnicodeString letters = icu::UnicodeString::fromUTF8(utf8Word);
    processor.normalizeLetters(letters, kurdish);
    std::string utf8Normalized;
    letters.toUTF8String(utf8Normalized);
    if (utf8Normalized == utf8Word)
      continue;

    appContext.wordRepository->remove(word.word, kurdish);
    appContext.wordRepository->updateFrequencies(
        {db::models::Word{db::Utils::fromUtf8(utf8Normalized),
                          word.numOccurrences, word.known, kurdish}});

    sqlite::command merge{
        connection,
        "INSERT INTO word_text(word, text_hash, numOccurrences, language) "
        "SELECT ?, text_hash, numOccurrences, language FROM word_text "
        "WHERE word = ? AND language = ? "
        "ON CONFLICT(word, text_hash) DO UPDATE SET "
        "numOccurrences = word_text.numOccurrences + excluded.numOccurrences"};
    merge % utf8Normalized % utf8Word % utils::to_string(kurdish);
    merge();
    appContext.wordTextRepository->removeByWord(word.word, kurdish);
  }

  for (const auto &text : appContext.textRepository->getAll(kurdish))
    regenerateSentences(appContext, processor, text, text.hash);
}
} // namespace

void migrateDatabase(AppContext &appContext) {
  auto &connection = appContext.dbHandler->getConnection();
  int version = userVersion(connection);
  if (version >= kDatabaseVersion)
    return;

  // One transaction: a failed migration leaves the database as it was.
  db::Savepoint tx{connection, "migrate_database"};
  if (version < 1)
    migrateToVersion1(appContext);
  if (version < 2)
    migrateToVersion2(appContext);
  sqlite::execute(connection,
                  "PRAGMA user_version = " + std::to_string(kDatabaseVersion),
                  true);
  tx.commit();
}
