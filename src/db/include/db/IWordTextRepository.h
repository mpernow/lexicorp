#pragma once
#include "db/WordText.h"

#include <optional>
#include <utils/Language.h>

namespace db {
class IWordTextRepository {
public:
  virtual ~IWordTextRepository() = default;

  virtual void add(const models::WordText &wordText) = 0;
  virtual std::optional<models::WordText>
  getByHash(const int hash, const utils::Language language) = 0;
  virtual std::optional<models::WordText>
  getByWord(const std::wstring &word, const utils::Language language) = 0;
};
} // namespace db
