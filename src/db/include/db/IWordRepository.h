#pragma once
#include "db/Word.h"

#include <optional>
#include <string>
#include <vector>

namespace db {
class IWordRepository {
public:
  virtual ~IWordRepository() = default;

  virtual void add(const models::Word &word) = 0;
  virtual std::optional<models::Word> getByText(const std::string &text) = 0;
  virtual std::vector<models::Word> getAll() = 0;
  virtual void update(const models::Word &word) = 0;
};
} // namespace db
