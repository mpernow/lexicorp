#pragma once
#include "db/Word.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace db {
class IWordRepository {
public:
  virtual ~IWordRepository() = default;

  virtual void add(const models::Word &word) = 0;
  virtual std::optional<models::Word> getByText(const std::wstring &text) = 0;
  virtual std::vector<models::Word> getAll() = 0;
  virtual void update(const models::Word &word) = 0;
  virtual void updateFrequencies(
      const std::unordered_map<std::wstring, int> &frequencies) = 0;
};
} // namespace db
