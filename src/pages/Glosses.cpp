#include "pages/Glosses.h"
#include "db/Utils.h"

#include <algorithm>

namespace {
// The leading entries that share the first entry's source, which is the
// preferred one: entries come ordered by precedence.
std::vector<db::models::DictionaryEntry>
preferredSource(std::vector<db::models::DictionaryEntry> entries) {
  if (!entries.empty())
    std::erase_if(entries, [source = entries.front().source](const auto &e) {
      return e.source != source;
    });
  return entries;
}
} // namespace

std::unordered_map<std::wstring, Gloss>
glossesOfStoredWords(AppContext &appContext) {
  auto language = appContext.selectedLanguage;

  std::unordered_map<std::wstring, std::vector<db::models::DictionaryEntry>>
      byHeadword;
  for (auto &entry :
       appContext.dictionaryRepository->getForStoredWords(language))
    byHeadword[entry.headword].push_back(std::move(entry));

  std::unordered_map<std::wstring, Gloss> glosses;
  for (auto &[headword, entries] : byHeadword)
    glosses[headword] = Gloss{preferredSource(std::move(entries)), L""};

  // Forms are ordered by lemma, so the first lemma with entries wins.
  std::unordered_map<std::wstring, std::vector<db::models::DictionaryEntry>>
      lemmaEntries;
  for (const auto &form :
       appContext.wordFormRepository->getForStoredWords(language)) {
    if (glosses.contains(form.form))
      continue;
    auto [it, added] = lemmaEntries.try_emplace(form.lemma);
    if (added)
      it->second = appContext.dictionaryRepository->getByHeadword(form.lemma,
                                                                  language);
    if (!it->second.empty())
      glosses[form.form] = Gloss{preferredSource(it->second), form.lemma};
  }
  return glosses;
}

std::optional<Gloss> glossOf(AppContext &appContext,
                             const std::wstring &word) {
  auto language = appContext.selectedLanguage;

  auto entries =
      appContext.dictionaryRepository->getByHeadword(word, language);
  if (!entries.empty())
    return Gloss{preferredSource(std::move(entries)), L""};

  for (const auto &form :
       appContext.wordFormRepository->getByForm(word, language)) {
    entries =
        appContext.dictionaryRepository->getByHeadword(form.lemma, language);
    if (!entries.empty())
      return Gloss{preferredSource(std::move(entries)), form.lemma};
  }
  return std::nullopt;
}

std::string glossSummary(const Gloss &gloss) {
  std::vector<std::wstring> seen;
  std::string summary;
  for (const auto &entry : gloss.entries) {
    if (std::ranges::find(seen, entry.gloss) != seen.end())
      continue;
    seen.push_back(entry.gloss);
    if (!summary.empty())
      summary += "; ";
    summary += db::Utils::toUtf8(entry.gloss);
  }
  if (!gloss.lemma.empty())
    summary = "form of " + db::Utils::toUtf8(gloss.lemma) + ": " + summary;
  return summary;
}
