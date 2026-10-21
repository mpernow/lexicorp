#pragma once
#include "db/Explanation.h"

#include <cstdint>
#include <optional>

namespace db {
class IExplanationRepository {
public:
  virtual ~IExplanationRepository() = default;

  // Stores `explanation`, replacing any earlier one of the same sentence.
  virtual void save(const models::Explanation &explanation) = 0;
  // Empty if the sentence hasn't been explained.
  virtual std::optional<models::Explanation>
  get(const std::int64_t sentenceHash) = 0;
};
} // namespace db
