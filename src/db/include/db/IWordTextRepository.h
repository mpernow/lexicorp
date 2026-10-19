#pragma once
#include "db/WordText.h"

#include <optional>
#include <utils/Language.h>
#include <vector>

namespace db {
class IWordTextRepository {
public:
  virtual ~IWordTextRepository() = default;

  virtual void add(const models::WordText &wordText) = 0;
  virtual std::optional<models::WordText>
  getByHash(const int hash, const utils::Language language) = 0;
  virtual std::vector<models::WordText>
  getByWord(const std::wstring &word, const utils::Language language) = 0;
  virtual std::vector<models::WordText> getAllByHash(const int textHash) = 0;
  virtual void removeByWord(const std::wstring &word,
                            const utils::Language language) = 0;
  virtual void removeByHash(const int textHash) = 0;
};
} // namespace db
