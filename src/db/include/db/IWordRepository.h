#pragma once
#include "db/Word.h"
#include "utils/Language.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace db {
class IWordRepository {
public:
  virtual ~IWordRepository() = default;

  virtual void add(const models::Word &word) = 0;
  virtual std::optional<models::Word>
  getByText(const std::wstring &text, const utils::Language &language) = 0;
  virtual std::vector<models::Word> getAll(const utils::Language &language) = 0;
  virtual void update(const models::Word &word) = 0;
  virtual void
  updateFrequencies(const std::unordered_map<std::wstring, int> &frequencies,
                    const utils::Language &language) = 0;
  virtual void updateFrequencies(const std::vector<models::Word> &words) = 0;
  // Lowers each word's count by its numOccurrences, removing unknown words
  // whose count reaches zero. Known words stay, with a count of zero.
  virtual void subtractFrequencies(const std::vector<models::Word> &words) = 0;
  virtual void remove(const std::wstring &word,
                      const utils::Language &language) = 0;
};
} // namespace db
