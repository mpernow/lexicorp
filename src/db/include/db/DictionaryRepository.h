#pragma once
#include "sqlite/connection.hpp"

#include "db/DictionaryEntry.h"
#include "db/IDictionaryRepository.h"

namespace db {
class DictionaryRepository : public IDictionaryRepository {
public:
  DictionaryRepository(sqlite::connection &dbConn);
  ~DictionaryRepository();

  void add(const models::DictionaryEntry &entry) override;
  std::vector<models::DictionaryEntry>
  getByHeadword(const std::wstring &headword,
                const utils::Language language) override;
  std::vector<models::DictionaryEntry>
  getForStoredWords(const utils::Language language) override;
  void
  replaceEntries(const std::wstring &headword, const utils::Language language,
                 const models::DictionarySource source,
                 const std::vector<models::DictionaryEntry> &entries) override;
  void setManualGloss(const std::wstring &headword,
                      const utils::Language language,
                      const std::wstring &gloss) override;

private:
  sqlite::connection &mDbConn;
};

} // namespace db
