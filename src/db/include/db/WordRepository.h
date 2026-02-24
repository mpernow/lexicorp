#pragma once
#include "sqlite/connection.hpp"

#include "db/IWordRepository.h"
#include "db/Word.h"

namespace db {
class WordRepository : public IWordRepository {
public:
  WordRepository(sqlite::connection &dbConn);
  ~WordRepository();

  void add(const models::Word &word) override;
  std::optional<models::Word> getByText(const std::string &text) override;
  std::vector<models::Word> getAll() override;
  void update(const models::Word &word) override;

private:
  sqlite::connection &mDbConn;
};

} // namespace db
