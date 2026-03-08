#pragma once
#include "sqlite/connection.hpp"

#include "db/ITextRepository.h"
#include "db/Text.h"

namespace db {
class TextRepository : public ITextRepository {
public:
  TextRepository(sqlite::connection &dbConn);
  ~TextRepository();

  void add(const models::Text &text) override;
  std::optional<models::Text> getByHash(const int hash) override;
  std::vector<models::Text> getAll() override;

private:
  sqlite::connection &mDbConn;
};

} // namespace db
