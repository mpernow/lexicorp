#pragma once
#include "sqlite/connection.hpp"

#include "db/IWordSentenceRepository.h"
#include "db/WordSentence.h"

namespace db {
class WordSentenceRepository : public IWordSentenceRepository {
public:
  WordSentenceRepository(sqlite::connection &dbConn);
  ~WordSentenceRepository();

  void add(const models::WordSentence &wordSentence) override;
  std::vector<models::WordSentence>
  getByHash(const std::int64_t textHash) override;
  void removeByHash(const std::int64_t textHash) override;
  void removeByWord(const std::wstring &word,
                    const utils::Language language) override;

private:
  sqlite::connection &mDbConn;
};

} // namespace db
