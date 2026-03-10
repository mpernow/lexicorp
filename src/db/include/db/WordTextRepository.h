#pragma once
#include "sqlite/connection.hpp"

#include "db/IWordTextRepository.h"
#include "db/WordText.h"

namespace db {
class WordTextRepository : public IWordTextRepository {
public:
  WordTextRepository(sqlite::connection &dbConn);
  ~WordTextRepository();

  void add(const models::WordText &wordText) override;
  std::optional<models::WordText>
  getByHash(const int textHash, const utils::Language language) override;
  std::optional<models::WordText>
  getByWord(const std::wstring &word, const utils::Language language) override;

private:
  sqlite::connection &mDbConn;
};

} // namespace db
