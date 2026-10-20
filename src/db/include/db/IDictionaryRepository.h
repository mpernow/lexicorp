#pragma once
#include "db/DictionaryEntry.h"
#include "utils/Language.h"

#include <string>
#include <vector>

namespace db {
class IDictionaryRepository {
public:
  virtual ~IDictionaryRepository() = default;

  virtual void add(const models::DictionaryEntry &entry) = 0;
  // A headword's entries, the preferred source first (see DictionarySource),
  // then in the order they were added.
  virtual std::vector<models::DictionaryEntry>
  getByHeadword(const std::wstring &headword,
                const utils::Language language) = 0;
  // The entries of every word of `language` in `words`, by headword, then
  // ordered as in getByHeadword().
  virtual std::vector<models::DictionaryEntry>
  getForStoredWords(const utils::Language language) = 0;
  // Replaces the headword's entries from `source` with `entries` (whose own
  // headword, language and source are ignored).
  virtual void
  replaceEntries(const std::wstring &headword, const utils::Language language,
                 const models::DictionarySource source,
                 const std::vector<models::DictionaryEntry> &entries) = 0;
  // Replaces the headword's manual entry (removes it if `gloss` is empty).
  virtual void setManualGloss(const std::wstring &headword,
                              const utils::Language language,
                              const std::wstring &gloss) = 0;
};
} // namespace db
