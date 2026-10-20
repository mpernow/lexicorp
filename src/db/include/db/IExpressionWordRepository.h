#pragma once
#include "db/ExpressionWord.h"
#include "utils/Language.h"

#include <string>
#include <vector>

namespace db {
class IExpressionWordRepository {
public:
  virtual ~IExpressionWordRepository() = default;

  // Does nothing if the link exists.
  virtual void add(const models::ExpressionWord &expressionWord) = 0;
  // The expressions `word` is part of, in alphabetical order.
  virtual std::vector<std::wstring>
  getByWord(const std::wstring &word, const utils::Language language) = 0;
  // The words linked to `expression`, in alphabetical order.
  virtual std::vector<std::wstring>
  getWords(const std::wstring &expression, const utils::Language language) = 0;
};
} // namespace db
