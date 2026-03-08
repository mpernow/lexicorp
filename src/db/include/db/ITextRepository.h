#pragma once
#include "db/Text.h"

#include <optional>
#include <vector>

namespace db {
class ITextRepository {
public:
  virtual ~ITextRepository() = default;

  virtual void add(const models::Text &text) = 0;
  virtual std::optional<models::Text> getByHash(const int hash) = 0;
  virtual std::vector<models::Text> getAll() = 0;
};
} // namespace db
