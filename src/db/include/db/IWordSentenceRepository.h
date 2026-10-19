#pragma once
#include "db/WordSentence.h"
#include "utils/Language.h"

#include <cstdint>
#include <string>
#include <vector>

namespace db {
class IWordSentenceRepository {
public:
  virtual ~IWordSentenceRepository() = default;

  virtual void add(const models::WordSentence &wordSentence) = 0;
  virtual std::vector<models::WordSentence>
  getByHash(const std::int64_t textHash) = 0;
  virtual void removeByHash(const std::int64_t textHash) = 0;
  virtual void removeByWord(const std::wstring &word,
                            const utils::Language language) = 0;
};
} // namespace db
