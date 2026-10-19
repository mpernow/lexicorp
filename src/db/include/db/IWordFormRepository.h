#pragma once
#include "db/WordForm.h"
#include "utils/Language.h"

#include <string>
#include <vector>

namespace db {
class IWordFormRepository {
public:
  virtual ~IWordFormRepository() = default;

  virtual void add(const models::WordForm &wordForm) = 0;
  // The lemmas `form` is a form of, ordered by lemma.
  virtual std::vector<models::WordForm>
  getByForm(const std::wstring &form, const utils::Language language) = 0;
  // The forms of `language` that are in `words`, ordered by form, then lemma.
  virtual std::vector<models::WordForm>
  getForStoredWords(const utils::Language language) = 0;
};
} // namespace db
