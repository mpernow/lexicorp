#pragma once
#include "sqlite/connection.hpp"

#include "db/IWordFormRepository.h"
#include "db/WordForm.h"

namespace db {
class WordFormRepository : public IWordFormRepository {
public:
  WordFormRepository(sqlite::connection &dbConn);
  ~WordFormRepository();

  void add(const models::WordForm &wordForm) override;
  std::vector<models::WordForm>
  getByForm(const std::wstring &form, const utils::Language language) override;
  std::vector<models::WordForm>
  getForStoredWords(const utils::Language language) override;

private:
  sqlite::connection &mDbConn;
};

} // namespace db
