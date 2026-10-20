#include "db/DictionaryRepository.h"
#include "db/Savepoint.h"
#include "db/Utils.h"
#include "sqlite/command.hpp"
#include "sqlite/execute.hpp"
#include "sqlite/query.hpp"
#include "utils/Language.h"

#include <optional>

namespace db {

namespace {
using models::DictionarySource;

// Columns in the order entryFromRow() reads them.
constexpr const char *kColumns = "d.id, d.headword, d.language, d.gloss, "
                                 "d.part_of_speech, d.source, "
                                 "d.ai_generated, d.updated_at";

// Sorts by DictionarySource's order; unknown sources last.
std::string precedenceSql() {
  std::string sql = "CASE d.source";
  for (auto source : {DictionarySource::Manual, DictionarySource::Wiktionary,
                      DictionarySource::Claude, DictionarySource::Google})
    sql += " WHEN '" + models::to_string(source) + "' THEN " +
           std::to_string(static_cast<int>(source));
  return sql + " ELSE 99 END";
}

// Empty for a source this version doesn't know (e.g. from a newer import).
template <typename Row>
std::optional<models::DictionaryEntry> entryFromRow(Row &row) {
  auto source = models::dictionary_source_from_string(
      row.template get<std::string>(5));
  if (!source)
    return std::nullopt;
  return models::DictionaryEntry{
      .id = row.template get<std::int64_t>(0),
      .headword = Utils::fromUtf8(row.template get<std::string>(1)),
      .language = utils::language_from_string(row.template get<std::string>(2)),
      .gloss = Utils::fromUtf8(row.template get<std::string>(3)),
      .partOfSpeech = Utils::fromUtf8(row.template get<std::string>(4)),
      .source = *source,
      .aiGenerated = row.template get<int>(6) != 0,
      .updatedAt = row.template get<std::int64_t>(7),
  };
}

std::vector<models::DictionaryEntry> entriesOf(sqlite::query &q) {
  std::vector<models::DictionaryEntry> results;
  for (auto &row : q.each())
    if (auto entry = entryFromRow(row))
      results.push_back(std::move(*entry));
  return results;
}
} // namespace

DictionaryRepository::DictionaryRepository(sqlite::connection &dbConn)
    : mDbConn(dbConn) {
  sqlite::execute(mDbConn, Utils::createTableSql<models::DictionaryEntry>(),
                  true);
  sqlite::execute(mDbConn,
                  "CREATE INDEX IF NOT EXISTS dictionary_headword "
                  "ON dictionary(headword, language)",
                  true);
}

DictionaryRepository::~DictionaryRepository() = default;

void DictionaryRepository::add(const models::DictionaryEntry &entry) {
  sqlite::command insert{
      mDbConn, "INSERT INTO dictionary(headword, language, gloss, "
               "part_of_speech, source, ai_generated, updated_at) "
               "VALUES(?, ?, ?, ?, ?, ?, CAST(strftime('%s', 'now') AS "
               "INTEGER))"};

  insert % Utils::toUtf8(entry.headword) % utils::to_string(entry.language) %
      Utils::toUtf8(entry.gloss) % Utils::toUtf8(entry.partOfSpeech) %
      models::to_string(entry.source) % (entry.aiGenerated ? 1 : 0);
  insert();
}

std::vector<models::DictionaryEntry>
DictionaryRepository::getByHeadword(const std::wstring &headword,
                                    const utils::Language language) {
  sqlite::query q{mDbConn, std::string("SELECT ") + kColumns +
                               " FROM dictionary d "
                               "WHERE d.headword = ? AND d.language = ? "
                               "ORDER BY " +
                               precedenceSql() + ", d.id"};
  q.bind(1, Utils::toUtf8(headword));
  q.bind(2, utils::to_string(language));
  return entriesOf(q);
}

std::vector<models::DictionaryEntry>
DictionaryRepository::getForStoredWords(const utils::Language language) {
  sqlite::query q{mDbConn, std::string("SELECT ") + kColumns +
                               " FROM dictionary d JOIN words w "
                               "ON w.word = d.headword "
                               "AND w.language = d.language "
                               "WHERE d.language = ? "
                               "ORDER BY d.headword, " +
                               precedenceSql() + ", d.id"};
  q.bind(1, utils::to_string(language));
  return entriesOf(q);
}

void DictionaryRepository::replaceEntries(
    const std::wstring &headword, const utils::Language language,
    const DictionarySource source,
    const std::vector<models::DictionaryEntry> &entries) {
  Savepoint tx{mDbConn, "replace_entries"};

  sqlite::command remove{mDbConn, "DELETE FROM dictionary WHERE headword = ? "
                                  "AND language = ? AND source = ?"};
  remove % Utils::toUtf8(headword) % utils::to_string(language) %
      models::to_string(source);
  remove();

  for (auto entry : entries) {
    entry.headword = headword;
    entry.language = language;
    entry.source = source;
    add(entry);
  }

  tx.commit();
}

void DictionaryRepository::setManualGloss(const std::wstring &headword,
                                          const utils::Language language,
                                          const std::wstring &gloss) {
  std::vector<models::DictionaryEntry> entries;
  if (!gloss.empty())
    entries.push_back(models::DictionaryEntry{.gloss = gloss});
  replaceEntries(headword, language, DictionarySource::Manual, entries);
}

} // namespace db
