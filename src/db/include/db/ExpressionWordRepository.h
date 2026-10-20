#pragma once
#include "sqlite/connection.hpp"

#include "db/ExpressionWord.h"
#include "db/IExpressionWordRepository.h"

namespace db {
class ExpressionWordRepository : public IExpressionWordRepository {
public:
  ExpressionWordRepository(sqlite::connection &dbConn);
  ~ExpressionWordRepository();

  void add(const models::ExpressionWord &expressionWord) override;
  std::vector<std::wstring>
  getByWord(const std::wstring &word, const utils::Language language) override;
  std::vector<std::wstring>
  getWords(const std::wstring &expression,
           const utils::Language language) override;

private:
  sqlite::connection &mDbConn;
};

} // namespace db
