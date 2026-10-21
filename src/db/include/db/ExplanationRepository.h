#pragma once
#include "sqlite/connection.hpp"

#include "db/Explanation.h"
#include "db/IExplanationRepository.h"

namespace db {
class ExplanationRepository : public IExplanationRepository {
public:
  ExplanationRepository(sqlite::connection &dbConn);
  ~ExplanationRepository();

  void save(const models::Explanation &explanation) override;
  std::optional<models::Explanation>
  get(const std::int64_t sentenceHash) override;

private:
  sqlite::connection &mDbConn;
};

} // namespace db
