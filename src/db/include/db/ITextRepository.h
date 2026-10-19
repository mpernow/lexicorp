#pragma once
#include "db/Text.h"
#include "utils/Language.h"

#include <optional>
#include <vector>

namespace db {
class ITextRepository {
public:
  virtual ~ITextRepository() = default;

  virtual void add(const models::Text &text) = 0;
  virtual std::optional<models::Text> getByHash(const std::int64_t hash) = 0;
  virtual std::vector<models::Text> getAll() = 0;
  virtual std::vector<models::Text>
  getAll(const utils::Language &language) = 0;
  virtual void remove(const std::int64_t hash) = 0;
};
} // namespace db
