#pragma once
#include "db/Sentence.h"
#include "utils/Language.h"

#include <cstdint>
#include <string>
#include <vector>

namespace db {
class ISentenceRepository {
public:
  virtual ~ISentenceRepository() = default;

  virtual void add(const models::Sentence &sentence) = 0;
  // A text's sentences, in order.
  virtual std::vector<models::Sentence>
  getByHash(const std::int64_t textHash) = 0;
  // The sentences containing a word, ordered by text and position.
  virtual std::vector<models::Sentence>
  getByWord(const std::wstring &word, const utils::Language language) = 0;
  virtual void removeByHash(const std::int64_t textHash) = 0;
};
} // namespace db
