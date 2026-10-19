#pragma once
#include "sqlite/connection.hpp"

#include "db/ISentenceRepository.h"
#include "db/Sentence.h"

namespace db {
class SentenceRepository : public ISentenceRepository {
public:
  SentenceRepository(sqlite::connection &dbConn);
  ~SentenceRepository();

  void add(const models::Sentence &sentence) override;
  std::vector<models::Sentence>
  getByHash(const std::int64_t textHash) override;
  std::vector<models::Sentence>
  getByWord(const std::wstring &word, const utils::Language language) override;
  void removeByHash(const std::int64_t textHash) override;

private:
  sqlite::connection &mDbConn;
};

} // namespace db
