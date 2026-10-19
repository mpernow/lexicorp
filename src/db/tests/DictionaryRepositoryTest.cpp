#include "db/DatabaseHandler.h"
#include "db/DictionaryRepository.h"
#include "db/WordFormRepository.h"
#include "db/WordRepository.h"
#include "utils/Language.h"
#include <gtest/gtest.h>
#include <sqlite/command.hpp>
#include <string>
#include <vector>

namespace {
const auto en = utils::Language::English;
const auto ckb = utils::Language::Kurdish;
using db::models::DictionaryEntry;
using db::models::DictionarySource;

class DictionaryRepositoryTests : public ::testing::Test {
protected:
  db::DatabaseHandler mDbHandler{":memory:"};
  db::WordRepository mWords{mDbHandler.getConnection()};
  db::DictionaryRepository mDictionary{mDbHandler.getConnection()};
  db::WordFormRepository mForms{mDbHandler.getConnection()};

  void addEntry(const std::wstring &headword, const std::wstring &gloss,
                DictionarySource source, utils::Language language = en) {
    mDictionary.add(DictionaryEntry{.headword = headword,
                                    .language = language,
                                    .gloss = gloss,
                                    .partOfSpeech = L"noun",
                                    .source = source});
  }

  std::vector<std::wstring> glosses(const std::wstring &headword,
                                    utils::Language language = en) {
    std::vector<std::wstring> result;
    for (const auto &entry : mDictionary.getByHeadword(headword, language))
      result.push_back(entry.gloss);
    return result;
  }
};
} // namespace

TEST_F(DictionaryRepositoryTests, AddAndGetByHeadword) {
  mDictionary.add(DictionaryEntry{.headword = L"ئاو",
                                  .language = ckb,
                                  .gloss = L"water",
                                  .partOfSpeech = L"noun",
                                  .source = DictionarySource::Claude,
                                  .aiGenerated = true});

  auto entries = mDictionary.getByHeadword(L"ئاو", ckb);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_GT(entries[0].id, 0);
  EXPECT_EQ(entries[0].headword, L"ئاو");
  EXPECT_EQ(entries[0].language, ckb);
  EXPECT_EQ(entries[0].gloss, L"water");
  EXPECT_EQ(entries[0].partOfSpeech, L"noun");
  EXPECT_EQ(entries[0].source, DictionarySource::Claude);
  EXPECT_TRUE(entries[0].aiGenerated);
  EXPECT_GT(entries[0].updatedAt, 0);

  EXPECT_TRUE(mDictionary.getByHeadword(L"ئاو", en).empty());
}

TEST_F(DictionaryRepositoryTests, EntriesAreOrderedBySourcePrecedence) {
  addEntry(L"cat", L"google gloss", DictionarySource::Google);
  addEntry(L"cat", L"claude gloss", DictionarySource::Claude);
  addEntry(L"cat", L"first sense", DictionarySource::Wiktionary);
  addEntry(L"cat", L"my gloss", DictionarySource::Manual);
  addEntry(L"cat", L"second sense", DictionarySource::Wiktionary);

  EXPECT_EQ(glosses(L"cat"),
            (std::vector<std::wstring>{L"my gloss", L"first sense",
                                       L"second sense", L"claude gloss",
                                       L"google gloss"}));
}

TEST_F(DictionaryRepositoryTests, UnknownSourcesAreSkipped) {
  addEntry(L"cat", L"feline", DictionarySource::Wiktionary);
  sqlite::command insert{mDbHandler.getConnection(),
                         "INSERT INTO dictionary(headword, language, gloss, "
                         "source, updated_at) "
                         "VALUES('cat', 'English', 'x', 'future', 0)"};
  insert();

  EXPECT_EQ(glosses(L"cat"), (std::vector<std::wstring>{L"feline"}));
}

TEST_F(DictionaryRepositoryTests, SetManualGlossReplacesOnlyTheManualEntry) {
  addEntry(L"cat", L"feline", DictionarySource::Wiktionary);

  mDictionary.setManualGloss(L"cat", en, L"pet");
  mDictionary.setManualGloss(L"cat", en, L"house cat");
  EXPECT_EQ(glosses(L"cat"),
            (std::vector<std::wstring>{L"house cat", L"feline"}));

  // An empty gloss removes the manual entry; the others stay.
  mDictionary.setManualGloss(L"cat", en, L"");
  EXPECT_EQ(glosses(L"cat"), (std::vector<std::wstring>{L"feline"}));
}

TEST_F(DictionaryRepositoryTests, GetForStoredWordsOnlyReturnsCorpusWords) {
  mWords.add(db::models::Word{L"cat", 1, false, en});
  mWords.add(db::models::Word{L"dog", 1, false, en});
  mWords.add(db::models::Word{L"bird", 1, false, ckb});
  addEntry(L"dog", L"canine", DictionarySource::Wiktionary);
  addEntry(L"cat", L"feline", DictionarySource::Wiktionary);
  addEntry(L"cat", L"pet", DictionarySource::Manual);
  addEntry(L"cow", L"bovine", DictionarySource::Wiktionary);
  addEntry(L"bird", L"avian", DictionarySource::Wiktionary);

  std::vector<std::wstring> found;
  for (const auto &entry : mDictionary.getForStoredWords(en))
    found.push_back(entry.headword + L":" + entry.gloss);
  EXPECT_EQ(found, (std::vector<std::wstring>{L"cat:pet", L"cat:feline",
                                              L"dog:canine"}));
}

TEST_F(DictionaryRepositoryTests, WordFormsLookUpTheirLemmas) {
  mForms.add(db::models::WordForm{L"ran", en, L"run", L"past"});
  mForms.add(db::models::WordForm{L"left", en, L"leave", L"past"});
  mForms.add(db::models::WordForm{L"left", en, L"left", L"alternative"});
  mForms.add(db::models::WordForm{L"went", en, L"go", L"past"});
  mWords.add(db::models::Word{L"left", 1, false, en});
  mWords.add(db::models::Word{L"ran", 1, false, en});

  auto lemmas = mForms.getByForm(L"left", en);
  ASSERT_EQ(lemmas.size(), 2u);
  EXPECT_EQ(lemmas[0].lemma, L"leave");
  EXPECT_EQ(lemmas[0].tags, L"past");
  EXPECT_EQ(lemmas[1].lemma, L"left");
  EXPECT_TRUE(mForms.getByForm(L"left", ckb).empty());

  std::vector<std::wstring> stored;
  for (const auto &form : mForms.getForStoredWords(en))
    stored.push_back(form.form + L">" + form.lemma);
  EXPECT_EQ(stored, (std::vector<std::wstring>{L"left>leave", L"left>left",
                                               L"ran>run"}));
}
