#include "db/WordFormRepository.h"
#include "db/Utils.h"
#include "sqlite/command.hpp"
#include "sqlite/execute.hpp"
#include "sqlite/query.hpp"
#include "utils/Language.h"

namespace db {

namespace {
// Columns in the order wordFormsOf() reads them.
constexpr const char *kColumns = "f.form, f.language, f.lemma, f.tags";

std::vector<models::WordForm> wordFormsOf(sqlite::query &q) {
  std::vector<models::WordForm> results;
  for (auto &row : q.each()) {
    results.push_back(models::WordForm{
        .form = Utils::fromUtf8(row.get<std::string>(0)),
        .language = utils::language_from_string(row.get<std::string>(1)),
        .lemma = Utils::fromUtf8(row.get<std::string>(2)),
        .tags = Utils::fromUtf8(row.get<std::string>(3)),
    });
  }
  return results;
}
} // namespace

WordFormRepository::WordFormRepository(sqlite::connection &dbConn)
    : mDbConn(dbConn) {
  sqlite::execute(mDbConn, Utils::createTableSql<models::WordForm>(), true);
}

WordFormRepository::~WordFormRepository() = default;

void WordFormRepository::add(const models::WordForm &wordForm) {
  sqlite::command insert{mDbConn, "INSERT INTO word_form(form, language, "
                                  "lemma, tags) VALUES(?, ?, ?, ?)"};

  insert % Utils::toUtf8(wordForm.form) % utils::to_string(wordForm.language) %
      Utils::toUtf8(wordForm.lemma) % Utils::toUtf8(wordForm.tags);
  insert();
}

std::vector<models::WordForm>
WordFormRepository::getByForm(const std::wstring &form,
                              const utils::Language language) {
  sqlite::query q{mDbConn, std::string("SELECT ") + kColumns +
                               " FROM word_form f "
                               "WHERE f.form = ? AND f.language = ? "
                               "ORDER BY f.lemma"};
  q.bind(1, Utils::toUtf8(form));
  q.bind(2, utils::to_string(language));
  return wordFormsOf(q);
}

std::vector<models::WordForm>
WordFormRepository::getForStoredWords(const utils::Language language) {
  sqlite::query q{mDbConn, std::string("SELECT ") + kColumns +
                               " FROM word_form f JOIN words w "
                               "ON w.word = f.form "
                               "AND w.language = f.language "
                               "WHERE f.language = ? "
                               "ORDER BY f.form, f.lemma"};
  q.bind(1, utils::to_string(language));
  return wordFormsOf(q);
}

} // namespace db
