#include "pages/Migration.h"
#include "db/Savepoint.h"
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
  sqlite::execute(connection,
                  "PRAGMA user_version = " + std::to_string(kDatabaseVersion),
                  true);
  tx.commit();
}
