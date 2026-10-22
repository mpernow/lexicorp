#include "db/DatabaseHandler.h"
#include "db/TextRepository.h"
#include "db/Utils.h"
#include "db/WordRepository.h"
#include "db/WordTextRepository.h"
#include "pages/AppContext.h"
#include "pages/Background.h"
#include "pages/DisplayWordsPage.h"
#include "pages/Explanations.h"
#include "pages/GrammarSections.h"
#include "pages/InsertTextPage.h"
#include "pages/HomePage.h"
#include "pages/LexiCorpMain.h"
#include "pages/Migration.h"
#include "pages/Sentences.h"
#include "pages/ViewSourcePage.h"

#include <Wt/Test/WTestEnvironment.h>
#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WDialog.h>
#include <Wt/WEvent.h>
#include <Wt/WLineEdit.h>
#include <Wt/WMenu.h>
#include <Wt/WMessageBox.h>
#include <Wt/WMenuItem.h>
#include <Wt/WPushButton.h>
#include <Wt/WTable.h>
#include <Wt/WTemplate.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

#include <algorithm>
#include <filesystem>
#include <map>
#include <optional>
#include <gtest/gtest.h>
#include <sqlite/execute.hpp>
#include <string>
#include <vector>

namespace {

// Collects every widget of type T in the subtree rooted at `widget`.
template <typename T>
void collect(Wt::WWidget *widget, std::vector<T *> &found) {
  if (auto *match = dynamic_cast<T *>(widget))
    found.push_back(match);
  for (Wt::WWidget *child : widget->children())
    collect(child, found);
}

template <typename T> std::vector<T *> findAll(Wt::WWidget *root) {
  std::vector<T *> found;
  collect(root, found);
  return found;
}

template <typename T> T *findOne(Wt::WWidget *root) {
  auto found = findAll<T>(root);
  return found.empty() ? nullptr : found.front();
}

// True if any WText under `root` contains `fragment` (including raw XHTML).
bool anyTextContains(Wt::WWidget *root, const std::string &fragment) {
  auto texts = findAll<Wt::WText>(root);
  return std::any_of(texts.begin(), texts.end(), [&](Wt::WText *t) {
    return t->text().toUTF8().find(fragment) != std::string::npos;
  });
}

bool containsText(Wt::WWidget *root, const std::string &text) {
  auto texts = findAll<Wt::WText>(root);
  return std::any_of(texts.begin(), texts.end(), [&](Wt::WText *t) {
    return t->text().toUTF8() == text;
  });
}

Wt::WPushButton *findButton(Wt::WWidget *root, const std::string &label) {
  for (auto *button : findAll<Wt::WPushButton>(root))
    if (button->text().toUTF8() == label)
      return button;
  return nullptr;
}

// The sidebar holds the only language combo box.
Wt::WComboBox *findSidebarCombo(Wt::WApplication &app) {
  return findOne<Wt::WComboBox>(app.root());
}

// The Home page's stat tiles, in order: words, known, texts.
std::vector<std::string> statValues(Wt::WWidget *root) {
  std::vector<std::string> values;
  for (auto *text : findAll<Wt::WText>(root))
    if (text->hasStyleClass("stat-value"))
      values.push_back(text->text().toUTF8());
  return values;
}

void selectLanguage(Wt::WApplication &app, const std::string &language) {
  auto *combo = findSidebarCombo(app);
  ASSERT_NE(combo, nullptr);
  combo->setCurrentIndex(combo->findText(language));
  combo->changed().emit();
}

} // namespace

// AppContext opens "lexiCorp.db" in the working directory, so every test runs
// in a fresh temporary directory to get an empty database.
class PagesTest : public ::testing::Test {
protected:
  void SetUp() override {
    mOriginalDir = std::filesystem::current_path();
    mTestDir = std::filesystem::temp_directory_path() /
               ("lexicorp_pages_test_" +
                std::string(::testing::UnitTest::GetInstance()
                                 ->current_test_info()
                                 ->name()));
    std::filesystem::remove_all(mTestDir);
    std::filesystem::create_directories(mTestDir);
    std::filesystem::current_path(mTestDir);
  }

  void TearDown() override {
    AppContext::sharedClaudeClient.reset();
    runBackgroundWorkInForeground = false;
    std::filesystem::current_path(mOriginalDir);
    std::filesystem::remove_all(mTestDir);
  }

  void addWord(const std::wstring &word, utils::Language language) {
    db::DatabaseHandler dbHandler;
    db::WordRepository words{dbHandler.getConnection()};
    words.add(db::models::Word{word, 1, false, language});
  }

  // Stores a text and its sentences. Only `linkedWord` (default: the first
  // word) is linked to it; enough for the "View source" tests.
  void addText(const std::wstring &text, std::int64_t hash,
               const std::wstring &source, utils::Language language,
               std::wstring linkedWord = L"") {
    if (linkedWord.empty())
      linkedWord = text.substr(0, text.find(L' '));
    AppContext appContext;
    appContext.textRepository->add(
        db::models::Text{text, hash, language, source});
    appContext.wordTextRepository->add(
        db::models::WordText{linkedWord, hash, 1, language});
    saveSentences(appContext, hash, language,
                  processor::TextProcessor{}.extractSentences(text, language),
                  {linkedWord});
  }

  ViewSourcePage *viewSourcePage(Wt::WApplication &app) {
    return findOne<ViewSourcePage>(app.root());
  }

  std::filesystem::path mOriginalDir;
  std::filesystem::path mTestDir;
};

TEST_F(PagesTest, AppContextDefaultsToEnglish) {
  AppContext context;
  EXPECT_EQ(context.selectedLanguage, utils::Language::English);
}

TEST_F(PagesTest, NoCookieSelectsEnglish) {
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};

  for (auto *combo : findAll<Wt::WComboBox>(app.root()))
    EXPECT_EQ(combo->currentText().toUTF8(), "English");
}

TEST_F(PagesTest, LanguageCookieIsRestored) {
  Wt::Test::WTestEnvironment env;
  env.setCookies({{"language", "Kurdish"}});
  LexiCorpMain app{env};

  auto *combo = findSidebarCombo(app);
  ASSERT_NE(combo, nullptr);
  EXPECT_EQ(combo->currentText().toUTF8(), "Kurdish");
}

TEST_F(PagesTest, InvalidLanguageCookieFallsBackToEnglish) {
  Wt::Test::WTestEnvironment env;
  env.setCookies({{"language", "Klingon"}});
  LexiCorpMain app{env};

  for (auto *combo : findAll<Wt::WComboBox>(app.root()))
    EXPECT_EQ(combo->currentText().toUTF8(), "English");
}

TEST_F(PagesTest, RestoredLanguageIsUsedForWordList) {
  addWord(L"kurdishword", utils::Language::Kurdish);
  addWord(L"englishword", utils::Language::English);

  Wt::Test::WTestEnvironment env;
  env.setCookies({{"language", "Kurdish"}});
  LexiCorpMain app{env};
  app.setInternalPath("/list", true);

  auto *page = findOne<DisplayWordsPage>(app.root());
  ASSERT_NE(page, nullptr);
  EXPECT_NE(findButton(page, "kurdishword"), nullptr);
  EXPECT_EQ(findButton(page, "englishword"), nullptr);
}

Wt::WCheckBox *findDarkModeSwitch(Wt::WApplication &app) {
  for (auto *checkBox : findAll<Wt::WCheckBox>(app.root()))
    if (checkBox->text().toUTF8() == "Dark mode")
      return checkBox;
  return nullptr;
}

TEST_F(PagesTest, NoThemeCookieStartsLight) {
  // The browser then reports prefers-color-scheme, which is JavaScript only.
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};

  EXPECT_EQ(app.htmlAttribute("data-bs-theme").toUTF8(), "light");
  auto *darkSwitch = findDarkModeSwitch(app);
  ASSERT_NE(darkSwitch, nullptr);
  EXPECT_FALSE(darkSwitch->isChecked());
}

TEST_F(PagesTest, DarkThemeCookieIsRestored) {
  Wt::Test::WTestEnvironment env;
  env.setCookies({{"theme", "dark"}});
  LexiCorpMain app{env};

  EXPECT_EQ(app.htmlAttribute("data-bs-theme").toUTF8(), "dark");
  auto *darkSwitch = findDarkModeSwitch(app);
  ASSERT_NE(darkSwitch, nullptr);
  EXPECT_TRUE(darkSwitch->isChecked());
}

TEST_F(PagesTest, InvalidThemeCookieStartsLight) {
  Wt::Test::WTestEnvironment env;
  env.setCookies({{"theme", "purple"}});
  LexiCorpMain app{env};

  EXPECT_EQ(app.htmlAttribute("data-bs-theme").toUTF8(), "light");
}

TEST_F(PagesTest, DarkModeSwitchTogglesTheme) {
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  auto *darkSwitch = findDarkModeSwitch(app);
  ASSERT_NE(darkSwitch, nullptr);

  darkSwitch->setChecked(true);
  darkSwitch->changed().emit();
  EXPECT_EQ(app.htmlAttribute("data-bs-theme").toUTF8(), "dark");

  darkSwitch->setChecked(false);
  darkSwitch->changed().emit();
  EXPECT_EQ(app.htmlAttribute("data-bs-theme").toUTF8(), "light");
}

TEST_F(PagesTest, OnlyCurrentMenuItemIsMarkedActive) {
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  auto *menu = findOne<Wt::WMenu>(app.root());
  ASSERT_NE(menu, nullptr);

  auto activeItems = [menu] {
    std::vector<std::string> active;
    for (int i = 0; i < menu->count(); ++i)
      if (menu->itemAt(i)->anchor()->hasStyleClass("active"))
        active.push_back(menu->itemAt(i)->text().toUTF8());
    return active;
  };

  EXPECT_EQ(activeItems(), std::vector<std::string>{"Home"});
  app.setInternalPath("/list", true);
  EXPECT_EQ(activeItems(), std::vector<std::string>{"Word List"});
  app.setInternalPath("/", true);
  EXPECT_EQ(activeItems(), std::vector<std::string>{"Home"});
}

TEST_F(PagesTest, HomeShowsCorpusStatsForSelectedLanguage) {
  addWord(L"cat", utils::Language::English);
  addWord(L"dog", utils::Language::English);
  addText(L"cat and dog", 1, L"src", utils::Language::English);
  addWord(L"chat", utils::Language::French);

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};

  auto *home = findOne<HomePage>(app.root());
  ASSERT_NE(home, nullptr);
  EXPECT_EQ(statValues(home), (std::vector<std::string>{"2", "0", "1"}));

  selectLanguage(app, "French");
  EXPECT_EQ(statValues(home), (std::vector<std::string>{"1", "0", "0"}));
}

TEST_F(PagesTest, HomeRefreshesWhenNavigatedTo) {
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  auto *home = findOne<HomePage>(app.root());
  ASSERT_NE(home, nullptr);
  EXPECT_EQ(statValues(home), (std::vector<std::string>{"0", "0", "0"}));

  app.setInternalPath("/list", true);
  addWord(L"cat", utils::Language::English);
  app.setInternalPath("/", true);
  EXPECT_EQ(statValues(home), (std::vector<std::string>{"1", "0", "0"}));
}

TEST_F(PagesTest, WordListShowsEmptyStateWithoutWords) {
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/list", true);

  auto *page = findOne<DisplayWordsPage>(app.root());
  ASSERT_NE(page, nullptr);
  EXPECT_TRUE(anyTextContains(page, "No English words yet."));
  EXPECT_TRUE(findOne<Wt::WTable>(page)->isHidden());
  EXPECT_FALSE(findButton(page, "Show Unknown Only")->isVisible());
  EXPECT_FALSE(findOne<Wt::WLineEdit>(page)->isVisible());
}

TEST_F(PagesTest, WordListShowsEmptyStateWhenAllWordsKnown) {
  {
    db::DatabaseHandler dbHandler;
    db::WordRepository words{dbHandler.getConnection()};
    words.add(db::models::Word{L"cat", 1, true, utils::Language::English});
  }

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/list", true);

  auto *page = findOne<DisplayWordsPage>(app.root());
  ASSERT_NE(page, nullptr);
  EXPECT_NE(findButton(page, "cat"), nullptr);

  findButton(page, "Show Unknown Only")->clicked().emit(Wt::WMouseEvent());
  EXPECT_TRUE(anyTextContains(page, "you know them all"));
  EXPECT_TRUE(findOne<Wt::WTable>(page)->isHidden());
}

TEST_F(PagesTest, WordListSearchFiltersWordsAndGlosses) {
  for (const auto *word : {L"cat", L"catch", L"dog"})
    addWord(word, utils::Language::English);
  {
    AppContext appContext;
    appContext.dictionaryRepository->add(db::models::DictionaryEntry{
        .headword = L"dog",
        .language = utils::Language::English,
        .gloss = L"Canine",
        .partOfSpeech = L"noun",
        .source = db::models::DictionarySource::Manual});
  }

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/list", true);
  auto *page = findOne<DisplayWordsPage>(app.root());
  auto *search = findOne<Wt::WLineEdit>(page);
  ASSERT_NE(search, nullptr);
  auto searchFor = [&](const std::string &query) {
    search->setText(Wt::WString::fromUTF8(query));
    search->textInput().emit();
  };

  searchFor("CAT");
  EXPECT_NE(findButton(page, "cat"), nullptr);
  EXPECT_NE(findButton(page, "catch"), nullptr);
  EXPECT_EQ(findButton(page, "dog"), nullptr);

  searchFor("canine");
  EXPECT_EQ(findButton(page, "cat"), nullptr);
  EXPECT_NE(findButton(page, "dog"), nullptr);

  searchFor("zebra");
  EXPECT_TRUE(findOne<Wt::WTable>(page)->isHidden());
  EXPECT_TRUE(anyTextContains(page, "No words match"));

  searchFor("");
  EXPECT_NE(findButton(page, "cat"), nullptr);
  EXPECT_NE(findButton(page, "dog"), nullptr);
}

TEST_F(PagesTest, ViewSourceShowsEmptyStateWithoutTexts) {
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/source", true);

  auto *page = viewSourcePage(app);
  ASSERT_NE(page, nullptr);
  EXPECT_TRUE(anyTextContains(page, "No English texts yet."));
}

TEST_F(PagesTest, ProcessingTextWithoutWordsHidesSubmit) {
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/insert", true);

  auto *page = findOne<InsertTextPage>(app.root());
  ASSERT_NE(page, nullptr);
  findAll<Wt::WTextArea>(page)[0]->setText("!!! ... ???");
  findButton(page, "Process")->clicked().emit(Wt::WMouseEvent());

  EXPECT_TRUE(containsText(page, "No words found in this text."));
  EXPECT_TRUE(findButton(page, "Submit")->isHidden());
}

TEST_F(PagesTest, WordListReloadsWhenNavigatedTo) {
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};

  // The word list page is built (and first loads) before the word exists.
  addWord(L"lateword", utils::Language::English);
  app.setInternalPath("/list", true);

  auto *page = findOne<DisplayWordsPage>(app.root());
  ASSERT_NE(page, nullptr);
  EXPECT_NE(findButton(page, "lateword"), nullptr);
}

TEST_F(PagesTest, SubmittingSameTextTwiceDoesNotThrowOrDoubleCount) {
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/insert", true);

  auto *page = findOne<InsertTextPage>(app.root());
  ASSERT_NE(page, nullptr);
  auto textAreas = findAll<Wt::WTextArea>(page);
  ASSERT_EQ(textAreas.size(), 2u);
  textAreas[0]->setText("the cat and the dog");
  textAreas[1]->setText("test source");

  auto *processButton = findButton(page, "Process");
  auto *submitButton = findButton(page, "Submit");
  ASSERT_NE(processButton, nullptr);
  ASSERT_NE(submitButton, nullptr);

  processButton->clicked().emit(Wt::WMouseEvent());
  submitButton->clicked().emit(Wt::WMouseEvent());

  // Submitting navigates to the word list, which must show the new words.
  auto *list = findOne<DisplayWordsPage>(app.root());
  ASSERT_NE(list, nullptr);
  EXPECT_NE(findButton(list, "the"), nullptr);

  // Adding the same text again is refused.
  app.setInternalPath("/insert", true);
  textAreas[0]->setText("the cat and the dog");
  processButton->clicked().emit(Wt::WMouseEvent());
  EXPECT_NO_THROW(submitButton->clicked().emit(Wt::WMouseEvent()));
  EXPECT_TRUE(containsText(page, "This text has already been added."));

  db::DatabaseHandler dbHandler;
  db::WordRepository words{dbHandler.getConnection()};
  auto the = words.getByText(L"the", utils::Language::English);
  ASSERT_TRUE(the.has_value());
  EXPECT_EQ(the->numOccurrences, 2);
}

// The buttons are disabled in the browser on click (see InsertTextPage);
// the handlers must enable them again on every path.
TEST_F(PagesTest, ProcessAndSubmitReenableTheirButtons) {
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/insert", true);

  auto *page = findOne<InsertTextPage>(app.root());
  ASSERT_NE(page, nullptr);
  findAll<Wt::WTextArea>(page)[0]->setText("the cat");
  auto *processButton = findButton(page, "Process");
  auto *submitButton = findButton(page, "Submit");

  processButton->disable();
  processButton->clicked().emit(Wt::WMouseEvent());
  EXPECT_TRUE(processButton->isEnabled());

  submitButton->disable();
  submitButton->clicked().emit(Wt::WMouseEvent());
  EXPECT_TRUE(submitButton->isEnabled());

  // Duplicate submit returns early; the button must still come back.
  app.setInternalPath("/insert", true);
  findAll<Wt::WTextArea>(page)[0]->setText("the cat");
  processButton->clicked().emit(Wt::WMouseEvent());
  submitButton->disable();
  submitButton->clicked().emit(Wt::WMouseEvent());
  EXPECT_TRUE(containsText(page, "This text has already been added."));
  EXPECT_TRUE(submitButton->isEnabled());
}

TEST_F(PagesTest, SubmittingClearsInsertPage) {
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/insert", true);

  auto *page = findOne<InsertTextPage>(app.root());
  ASSERT_NE(page, nullptr);
  auto textAreas = findAll<Wt::WTextArea>(page);
  textAreas[0]->setText("the cat");
  textAreas[1]->setText("a source");
  findButton(page, "Process")->clicked().emit(Wt::WMouseEvent());
  auto *submitButton = findButton(page, "Submit");
  submitButton->clicked().emit(Wt::WMouseEvent());
  ASSERT_EQ(app.internalPath(), "/list");

  app.setInternalPath("/insert", true);
  EXPECT_TRUE(textAreas[0]->text().empty());
  EXPECT_TRUE(textAreas[1]->text().empty());
  EXPECT_TRUE(findOne<Wt::WTable>(page)->isHidden());
  EXPECT_TRUE(submitButton->isHidden());

  // A stray Submit on the cleared page saves nothing.
  submitButton->clicked().emit(Wt::WMouseEvent());
  db::DatabaseHandler dbHandler;
  db::TextRepository texts{dbHandler.getConnection()};
  EXPECT_EQ(texts.getAll(utils::Language::English).size(), 1u);
}

TEST_F(PagesTest, ProcessPrefillsAndLocksAlreadyKnownWords) {
  {
    db::DatabaseHandler dbHandler;
    db::WordRepository words{dbHandler.getConnection()};
    words.add(db::models::Word{L"cat", 1, true, utils::Language::English});
    words.add(db::models::Word{L"dog", 1, false, utils::Language::English});
    // Known in another language only: must not count.
    words.add(db::models::Word{L"the", 1, true, utils::Language::French});
  }

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/insert", true);
  auto *page = findOne<InsertTextPage>(app.root());
  ASSERT_NE(page, nullptr);
  findAll<Wt::WTextArea>(page)[0]->setText("the cat dog");
  findButton(page, "Process")->clicked().emit(Wt::WMouseEvent());

  // Row cells: word, count, include, known.
  auto *table = findOne<Wt::WTable>(page);
  auto knownBox = [table](const std::string &word) -> Wt::WCheckBox * {
    for (int row = 1; row < table->rowCount(); ++row)
      if (containsText(table->elementAt(row, 0), word))
        return findOne<Wt::WCheckBox>(table->elementAt(row, 3));
    return nullptr;
  };
  for (const char *word : {"the", "cat", "dog"})
    ASSERT_NE(knownBox(word), nullptr) << word;

  EXPECT_TRUE(knownBox("cat")->isChecked());
  EXPECT_FALSE(knownBox("cat")->isEnabled());
  EXPECT_FALSE(knownBox("dog")->isChecked());
  EXPECT_TRUE(knownBox("dog")->isEnabled());
  EXPECT_FALSE(knownBox("the")->isChecked());
  EXPECT_TRUE(knownBox("the")->isEnabled());
}

TEST_F(PagesTest, SidebarViewSourceListsAllSourcesWithoutHighlight) {
  addText(L"cat sat here", 1, L"first-source", utils::Language::English);
  addText(L"dog ran there", 2, L"", utils::Language::English);
  addText(L"chat assis", 3, L"french-source", utils::Language::French);

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/source", true);

  auto *page = viewSourcePage(app);
  ASSERT_NE(page, nullptr);
  EXPECT_TRUE(containsText(page, "All sources (2)"));
  EXPECT_TRUE(containsText(page, "Source: \u2068first-source\u2069"));
  EXPECT_TRUE(containsText(page, "Source: \u2068(no source)\u2069"));
  EXPECT_TRUE(anyTextContains(page, "cat sat here"));
  EXPECT_TRUE(anyTextContains(page, "dog ran there"));
  EXPECT_FALSE(anyTextContains(page, "chat assis"));
  EXPECT_FALSE(anyTextContains(page, "highlight-word"));
}

TEST_F(PagesTest, TappingWordHighlightsItInSources) {
  addText(L"cat sat here", 1, L"first-source", utils::Language::English);
  addText(L"dog ran there", 2, L"second-source", utils::Language::English);
  addWord(L"cat", utils::Language::English);

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/list", true);

  auto *list = findOne<DisplayWordsPage>(app.root());
  ASSERT_NE(list, nullptr);
  auto *button = findButton(list, "cat");
  ASSERT_NE(button, nullptr);
  button->clicked().emit(Wt::WMouseEvent());

  EXPECT_EQ(app.internalPath(), "/source/word");
  auto *page = viewSourcePage(app);
  ASSERT_NE(page, nullptr);
  EXPECT_TRUE(containsText(
      page, "Sentences containing \"\u2068cat\u2069\" (1)"));
  EXPECT_TRUE(containsText(page, "Source: \u2068first-source\u2069"));
  EXPECT_FALSE(containsText(page, "Source: \u2068second-source\u2069"));
  EXPECT_TRUE(anyTextContains(page, "<span class=\"highlight-word\">cat"));
}

TEST_F(PagesTest, SidebarViewSourceAfterWordSelectionShowsAllSources) {
  addText(L"cat sat here", 1, L"first-source", utils::Language::English);
  addText(L"dog ran there", 2, L"second-source", utils::Language::English);
  addWord(L"cat", utils::Language::English);

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};

  // A word is chosen from the word list earlier in the session...
  app.setInternalPath("/list", true);
  auto *list = findOne<DisplayWordsPage>(app.root());
  ASSERT_NE(list, nullptr);
  findButton(list, "cat")->clicked().emit(Wt::WMouseEvent());
  ASSERT_EQ(app.internalPath(), "/source/word");

  // ...then the sidebar link is followed.
  app.setInternalPath("/source", true);
  auto *page = viewSourcePage(app);
  ASSERT_NE(page, nullptr);
  EXPECT_TRUE(containsText(page, "All sources (2)"));
  EXPECT_FALSE(anyTextContains(page, "highlight-word"));
}

TEST_F(PagesTest, KnownSwitchUpdatesWord) {
  addWord(L"cat", utils::Language::English);

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/list", true);
  auto *list = findOne<DisplayWordsPage>(app.root());
  ASSERT_NE(list, nullptr);

  auto isKnown = [] {
    db::DatabaseHandler dbHandler;
    db::WordRepository words{dbHandler.getConnection()};
    return words.getByText(L"cat", utils::Language::English)->known;
  };
  auto toggle = [list] {
    auto *knownSwitch = findOne<Wt::WCheckBox>(list);
    ASSERT_NE(knownSwitch, nullptr);
    knownSwitch->setChecked(!knownSwitch->isChecked());
    knownSwitch->changed().emit();
  };

  ASSERT_FALSE(isKnown());
  toggle();
  EXPECT_TRUE(isKnown());
  EXPECT_TRUE(findOne<Wt::WCheckBox>(list)->isChecked());
  toggle();
  EXPECT_FALSE(isKnown());
}

// Reloading the browser on a page starts the session on that path; routing
// must still work from there (the menu's base path once came from it).
TEST_F(PagesTest, TappingWordWorksWhenSessionStartsOnWordList) {
  addText(L"cat sat here", 1, L"first-source", utils::Language::English);
  addWord(L"cat", utils::Language::English);

  Wt::Test::WTestEnvironment env;
  env.setInternalPath("/list");
  LexiCorpMain app{env};

  auto *menu = findOne<Wt::WMenu>(app.root());
  ASSERT_NE(menu, nullptr);
  EXPECT_EQ(menu->currentItem()->text(), "Word List");

  auto *list = findOne<DisplayWordsPage>(app.root());
  ASSERT_NE(list, nullptr);
  findButton(list, "cat")->clicked().emit(Wt::WMouseEvent());

  EXPECT_EQ(app.internalPath(), "/source/word");
  EXPECT_EQ(menu->currentItem()->text(), "View source");
}

TEST_F(PagesTest, ViewSourceDoesNotDoubleEscape) {
  addText(L"l'homme & <la> femme", 1, L"src", utils::Language::French);

  Wt::Test::WTestEnvironment env;
  env.setCookies({{"language", "French"}});
  LexiCorpMain app{env};
  app.setInternalPath("/source", true);

  auto *page = viewSourcePage(app);
  ASSERT_NE(page, nullptr);
  // Wt normalises the XHTML, turning &#39; back into a plain apostrophe.
  EXPECT_TRUE(containsText(page, "l'homme &amp; &lt;la&gt; femme"));
  EXPECT_FALSE(anyTextContains(page, "&amp;#39;"));
  EXPECT_FALSE(anyTextContains(page, "&amp;amp;"));
}

// The open confirmation dialog, if any (dialogs live outside app.root()).
Wt::WMessageBox *openDialog(Wt::WApplication &app) {
  return findOne<Wt::WMessageBox>(app.domRoot());
}

void answerDialog(Wt::WApplication &app, Wt::StandardButton answer) {
  auto *box = openDialog(app);
  ASSERT_NE(box, nullptr);
  box->button(answer)->clicked().emit(Wt::WMouseEvent());
  EXPECT_EQ(openDialog(app), nullptr);
}

TEST_F(PagesTest, DeletingWordAsksFirst) {
  addText(L"cat sat here", 1, L"src", utils::Language::English);
  addWord(L"cat", utils::Language::English);
  addWord(L"dog", utils::Language::English);

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/list", true);
  auto *list = findOne<DisplayWordsPage>(app.root());
  ASSERT_NE(list, nullptr);

  auto deleteCat = [list] {
    auto *table = findOne<Wt::WTable>(list);
    for (int row = 1; row < table->rowCount(); ++row)
      if (findButton(table->elementAt(row, 0), "cat"))
        return findOne<Wt::WPushButton>(table->elementAt(row, 3));
    return static_cast<Wt::WPushButton *>(nullptr);
  };
  auto catInDb = [] {
    db::DatabaseHandler dbHandler;
    db::WordRepository words{dbHandler.getConnection()};
    return words.getByText(L"cat", utils::Language::English).has_value();
  };

  ASSERT_NE(deleteCat(), nullptr);
  deleteCat()->clicked().emit(Wt::WMouseEvent());
  answerDialog(app, Wt::StandardButton::Cancel);
  EXPECT_TRUE(catInDb());
  EXPECT_NE(findButton(list, "cat"), nullptr);

  deleteCat()->clicked().emit(Wt::WMouseEvent());
  answerDialog(app, Wt::StandardButton::Yes);
  EXPECT_FALSE(catInDb());
  EXPECT_EQ(findButton(list, "cat"), nullptr);
  EXPECT_NE(findButton(list, "dog"), nullptr);

  db::DatabaseHandler dbHandler;
  db::WordTextRepository wordTexts{dbHandler.getConnection()};
  db::TextRepository texts{dbHandler.getConnection()};
  EXPECT_TRUE(wordTexts.getByWord(L"cat", utils::Language::English).empty());
  EXPECT_TRUE(texts.getByHash(1).has_value()); // the text itself stays
}

TEST_F(PagesTest, DeletingTextAsksFirstAndLowersWordCounts) {
  const auto en = utils::Language::English;
  {
    db::DatabaseHandler dbHandler;
    db::WordRepository words{dbHandler.getConnection()};
    db::TextRepository texts{dbHandler.getConnection()};
    db::WordTextRepository wordTexts{dbHandler.getConnection()};
    texts.add(db::models::Text{L"cat cat bad", 1, en, L"doomed"});
    texts.add(db::models::Text{L"cat sat", 2, en, L"kept"});
    words.add(db::models::Word{L"cat", 3, false, en});
    words.add(db::models::Word{L"bad", 1, false, en});
    words.add(db::models::Word{L"sat", 1, false, en});
    wordTexts.add(db::models::WordText{L"cat", 1, 2, en});
    wordTexts.add(db::models::WordText{L"bad", 1, 1, en});
    wordTexts.add(db::models::WordText{L"cat", 2, 1, en});
    wordTexts.add(db::models::WordText{L"sat", 2, 1, en});
  }

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/source", true);
  auto *page = viewSourcePage(app);
  ASSERT_NE(page, nullptr);
  ASSERT_TRUE(containsText(page, "All sources (2)"));

  // "kept" sorts after "doomed", so the first Delete button is doomed's.
  findButton(page, "Delete")->clicked().emit(Wt::WMouseEvent());
  answerDialog(app, Wt::StandardButton::Cancel);
  EXPECT_TRUE(containsText(page, "All sources (2)"));

  findButton(page, "Delete")->clicked().emit(Wt::WMouseEvent());
  answerDialog(app, Wt::StandardButton::Yes);
  EXPECT_TRUE(containsText(page, "All sources (1)"));
  EXPECT_FALSE(containsText(page, "Source: \u2068doomed\u2069"));
  EXPECT_TRUE(containsText(page, "Source: \u2068kept\u2069"));

  db::DatabaseHandler dbHandler;
  db::WordRepository words{dbHandler.getConnection()};
  db::TextRepository texts{dbHandler.getConnection()};
  db::WordTextRepository wordTexts{dbHandler.getConnection()};
  EXPECT_FALSE(texts.getByHash(1).has_value());
  EXPECT_TRUE(wordTexts.getAllByHash(1).empty());
  EXPECT_EQ(wordTexts.getAllByHash(2).size(), 2u);
  EXPECT_EQ(words.getByText(L"cat", en)->numOccurrences, 1);
  EXPECT_EQ(words.getByText(L"sat", en)->numOccurrences, 1);
  EXPECT_FALSE(words.getByText(L"bad", en).has_value());
}

// Stored words are letter-normalised; the raw text keeps the variant typed.
TEST_F(PagesTest, HighlightMatchesKurdishLetterVariants) {
  const std::wstring arabicKaf = L"\u0643\u062A\u06CE\u0628";  // كتێب
  const std::wstring kurdishKaf = L"\u06A9\u062A\u06CE\u0628"; // کتێب
  addText(arabicKaf + L" \u0628\u0627\u0634\u06D5", 1, L"src",
          utils::Language::Kurdish, kurdishKaf);
  addWord(kurdishKaf, utils::Language::Kurdish);

  Wt::Test::WTestEnvironment env;
  env.setCookies({{"language", "Kurdish"}});
  LexiCorpMain app{env};
  app.setInternalPath("/list", true);
  auto *list = findOne<DisplayWordsPage>(app.root());
  ASSERT_NE(list, nullptr);
  findButton(list, db::Utils::toUtf8(kurdishKaf))
      ->clicked()
      .emit(Wt::WMouseEvent());

  auto *page = viewSourcePage(app);
  ASSERT_NE(page, nullptr);
  EXPECT_TRUE(anyTextContains(page, "<span class=\"highlight-word\">" +
                                        db::Utils::toUtf8(arabicKaf) +
                                        "</span>"));
}

TEST_F(PagesTest, SubmittingTextStoresSentencesForIncludedWords) {
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/insert", true);
  auto *page = findOne<InsertTextPage>(app.root());
  ASSERT_NE(page, nullptr);

  findAll<Wt::WTextArea>(page)[0]->setText("The cat sat. A dog ran.");
  findButton(page, "Process")->clicked().emit(Wt::WMouseEvent());

  // Untick "dog" (row cells: word, count, include, known).
  auto *table = findOne<Wt::WTable>(page);
  for (int row = 1; row < table->rowCount(); ++row) {
    if (containsText(table->elementAt(row, 0), "dog")) {
      auto *include = findOne<Wt::WCheckBox>(table->elementAt(row, 2));
      include->setChecked(false);
      include->changed().emit();
    }
  }
  findButton(page, "Submit")->clicked().emit(Wt::WMouseEvent());

  AppContext appContext;
  auto texts = appContext.textRepository->getAll(utils::Language::English);
  ASSERT_EQ(texts.size(), 1u);
  auto sentences = appContext.sentenceRepository->getByHash(texts[0].hash);
  ASSERT_EQ(sentences.size(), 2u);
  EXPECT_EQ(sentences[0].sentence, L"The cat sat.");
  EXPECT_EQ(sentences[1].sentence, L"A dog ran.");
  EXPECT_EQ(sentences[1].start, 13);

  auto en = utils::Language::English;
  EXPECT_EQ(appContext.sentenceRepository->getByWord(L"cat", en).size(), 1u);
  EXPECT_EQ(appContext.sentenceRepository->getByWord(L"ran", en).size(), 1u);
  EXPECT_TRUE(appContext.sentenceRepository->getByWord(L"dog", en).empty());
}

TEST_F(PagesTest, WordSourcesShowSentencesAndToggleFullText) {
  addText(L"cat one. Nothing here. Cat two.", 1, L"b-source",
          utils::Language::English, L"cat");
  addText(L"A cat. More text.", 2, L"a-source", utils::Language::English,
          L"cat");
  addText(L"No match here.", 3, L"c-source", utils::Language::English, L"no");
  addWord(L"cat", utils::Language::English);

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/list", true);
  findButton(findOne<DisplayWordsPage>(app.root()), "cat")
      ->clicked()
      .emit(Wt::WMouseEvent());

  auto *page = viewSourcePage(app);
  ASSERT_NE(page, nullptr);
  EXPECT_TRUE(containsText(
      page, "Sentences containing \"\u2068cat\u2069\" (3)"));
  EXPECT_FALSE(containsText(page, "Source: \u2068c-source\u2069"));

  // Cards are sorted by source; each lists only its matching sentences.
  auto cards = findAll<Wt::WText>(page);
  std::vector<std::string> sentences, sources;
  for (auto *text : cards) {
    if (text->hasStyleClass("source-sentence"))
      sentences.push_back(text->text().toUTF8());
    if (text->text().toUTF8().starts_with("Source: "))
      sources.push_back(text->text().toUTF8());
  }
  EXPECT_EQ(sources,
            (std::vector<std::string>{"Source: \u2068a-source\u2069",
                                      "Source: \u2068b-source\u2069"}));
  ASSERT_EQ(sentences.size(), 3u);
  EXPECT_EQ(sentences[0], "A <span class=\"highlight-word\">cat</span>.");
  EXPECT_EQ(sentences[1], "<span class=\"highlight-word\">cat</span> one.");
  EXPECT_FALSE(anyTextContains(page, "Nothing here"));

  // Showing b-source's full text marks both matching sentences.
  auto toggles = findAll<Wt::WPushButton>(page);
  std::erase_if(toggles, [](auto *b) {
    return b->text().toUTF8() != "Show full text";
  });
  ASSERT_EQ(toggles.size(), 2u);
  toggles[1]->clicked().emit(Wt::WMouseEvent());
  EXPECT_EQ(toggles[1]->text().toUTF8(), "Show sentences");
  EXPECT_TRUE(anyTextContains(
      page, "<span class=\"highlight-sentence\"><span "
            "class=\"highlight-word\">cat</span> one.</span> Nothing here. "
            "<span class=\"highlight-sentence\"><span "
            "class=\"highlight-word\">Cat</span> two.</span>"));

  toggles[1]->clicked().emit(Wt::WMouseEvent());
  EXPECT_EQ(toggles[1]->text().toUTF8(), "Show full text");
}

TEST_F(PagesTest, DeletingRemovesSentences) {
  addText(L"cat sat. Dog ran.", 1, L"first", utils::Language::English, L"cat");
  addText(L"cat again.", 2, L"second", utils::Language::English, L"cat");
  {
    AppContext appContext;
    appContext.wordSentenceRepository->add(
        db::models::WordSentence{L"dog", utils::Language::English, 1, 1, 1});
  }
  addWord(L"cat", utils::Language::English);
  auto en = utils::Language::English;

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};

  // Deleting a word unlinks it from every sentence, keeping the sentences.
  app.setInternalPath("/list", true);
  auto *table = findOne<Wt::WTable>(findOne<DisplayWordsPage>(app.root()));
  ASSERT_NE(findButton(table->elementAt(1, 0), "cat"), nullptr);
  findOne<Wt::WPushButton>(table->elementAt(1, 3))
      ->clicked()
      .emit(Wt::WMouseEvent());
  answerDialog(app, Wt::StandardButton::Yes);
  {
    AppContext appContext;
    EXPECT_TRUE(appContext.sentenceRepository->getByWord(L"cat", en).empty());
    EXPECT_EQ(appContext.sentenceRepository->getByHash(1).size(), 2u);
    EXPECT_EQ(appContext.sentenceRepository->getByWord(L"dog", en).size(),
              1u);
  }

  // Deleting a text removes its sentences and their links.
  app.setInternalPath("/source", true);
  findButton(viewSourcePage(app), "Delete")->clicked().emit(Wt::WMouseEvent());
  answerDialog(app, Wt::StandardButton::Yes);

  AppContext appContext;
  EXPECT_TRUE(appContext.sentenceRepository->getByHash(1).empty());
  EXPECT_TRUE(appContext.wordSentenceRepository->getByHash(1).empty());
  EXPECT_EQ(appContext.sentenceRepository->getByHash(2).size(), 1u);
}

TEST_F(PagesTest, MigrationRehashesTextsAndAddsSentences) {
  const std::wstring text = L"The cat sat. A dog ran.";
  const auto fullHash =
      static_cast<std::int64_t>(processor::TextProcessor{}.hashWstring(text));
  const std::int64_t oldHash = static_cast<std::int32_t>(fullHash);
  const auto en = utils::Language::English;
  {
    // A database from before version 1: 32-bit hashes, no sentences.
    AppContext appContext;
    appContext.textRepository->add(db::models::Text{text, oldHash, en, L"s"});
    for (const auto *word : {L"the", L"cat", L"sat", L"a", L"ran"})
      appContext.wordTextRepository->add(
          db::models::WordText{word, oldHash, 1, en});
  }

  for (int run = 0; run < 2; ++run) { // the second run must change nothing
    AppContext appContext;
    migrateDatabase(appContext);

    EXPECT_FALSE(appContext.textRepository->getByHash(oldHash).has_value());
    EXPECT_TRUE(appContext.textRepository->getByHash(fullHash).has_value());
    EXPECT_EQ(appContext.wordTextRepository->getAllByHash(fullHash).size(),
              5u);
    auto sentences = appContext.sentenceRepository->getByHash(fullHash);
    ASSERT_EQ(sentences.size(), 2u);
    EXPECT_EQ(sentences[1].sentence, L"A dog ran.");
    // "dog" wasn't saved with the text, so it isn't linked.
    EXPECT_EQ(appContext.sentenceRepository->getByWord(L"ran", en).size(), 1u);
    EXPECT_TRUE(appContext.sentenceRepository->getByWord(L"dog", en).empty());
  }
}

TEST_F(PagesTest, MigrationNormalizesKurdishWords) {
  const std::wstring arabicKaf = L"كتێب";  // كتێب
  const std::wstring kurdishKaf = L"کتێب"; // کتێب
  const std::wstring text = arabicKaf + L" باشە. " +
                            kurdishKaf;
  const auto hash =
      static_cast<std::int64_t>(processor::TextProcessor{}.hashWstring(text));
  const auto ckb = utils::Language::Kurdish;
  {
    // A version 1 database with words stored before letter normalisation.
    AppContext appContext;
    sqlite::execute(appContext.dbHandler->getConnection(),
                    "PRAGMA user_version = 1", true);
    appContext.textRepository->add(db::models::Text{text, hash, ckb, L"s"});
    appContext.wordRepository->add(db::models::Word{arabicKaf, 1, false, ckb});
    appContext.wordRepository->add(db::models::Word{kurdishKaf, 2, true, ckb});
    appContext.wordTextRepository->add(
        db::models::WordText{arabicKaf, hash, 1, ckb});
    appContext.wordTextRepository->add(
        db::models::WordText{kurdishKaf, hash, 2, ckb});
  }

  for (int run = 0; run < 2; ++run) { // the second run must change nothing
    AppContext appContext;
    migrateDatabase(appContext);

    EXPECT_FALSE(
        appContext.wordRepository->getByText(arabicKaf, ckb).has_value());
    auto word = appContext.wordRepository->getByText(kurdishKaf, ckb);
    ASSERT_TRUE(word.has_value());
    EXPECT_EQ(word->numOccurrences, 3);
    EXPECT_TRUE(word->known);

    auto wordTexts = appContext.wordTextRepository->getAllByHash(hash);
    ASSERT_EQ(wordTexts.size(), 1u);
    EXPECT_EQ(wordTexts[0].word, kurdishKaf);
    EXPECT_EQ(wordTexts[0].numOccurrences, 3);

    // Both sentences contain the word, whichever kaf they were typed with.
    EXPECT_EQ(appContext.sentenceRepository->getByWord(kurdishKaf, ckb).size(),
              2u);
  }
}

namespace {
void addDictionaryEntry(const std::wstring &headword, const std::wstring &gloss,
                        db::models::DictionarySource source) {
  AppContext appContext;
  appContext.dictionaryRepository->add(
      db::models::DictionaryEntry{.headword = headword,
                                  .language = utils::Language::English,
                                  .gloss = gloss,
                                  .partOfSpeech = L"noun",
                                  .source = source});
}

// The gloss shown under each word in the word list, by word.
std::map<std::string, std::string> wordListGlosses(Wt::WWidget *list) {
  auto *table = findOne<Wt::WTable>(list);
  std::map<std::string, std::string> glosses;
  for (int row = 1; row < table->rowCount(); ++row) {
    auto *cell = table->elementAt(row, 0);
    std::string word = findOne<Wt::WPushButton>(cell)->text().toUTF8();
    for (auto *text : findAll<Wt::WText>(cell))
      if (text->hasStyleClass("word-gloss"))
        glosses[word] = text->text().toUTF8();
  }
  return glosses;
}

Wt::WDialog *openGlossDialog(Wt::WApplication &app) {
  return findOne<Wt::WDialog>(app.domRoot());
}
} // namespace

TEST_F(PagesTest, WordListShowsPreferredGlosses) {
  for (const auto *word : {L"cat", L"dog", L"ran"})
    addWord(word, utils::Language::English);
  using db::models::DictionarySource;
  addDictionaryEntry(L"cat", L"feline", DictionarySource::Wiktionary);
  addDictionaryEntry(L"cat", L"pet", DictionarySource::Manual);
  addDictionaryEntry(L"run", L"to move fast", DictionarySource::Wiktionary);
  addDictionaryEntry(L"run", L"to manage", DictionarySource::Wiktionary);
  {
    AppContext appContext;
    appContext.wordFormRepository->add(db::models::WordForm{
        L"ran", utils::Language::English, L"run", L"past"});
  }

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/list", true);

  EXPECT_EQ(wordListGlosses(findOne<DisplayWordsPage>(app.root())),
            (std::map<std::string, std::string>{
                {"cat", "pet"},
                {"ran", "form of run: to move fast; to manage"}}));
}

TEST_F(PagesTest, EditingGlossSavesManualEntry) {
  addText(L"The cat sat.", 1, L"src", utils::Language::English, L"cat");
  addWord(L"cat", utils::Language::English);
  addDictionaryEntry(L"cat", L"feline", db::models::DictionarySource::Wiktionary);

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  app.setInternalPath("/list", true);
  findButton(findOne<DisplayWordsPage>(app.root()), "cat")
      ->clicked()
      .emit(Wt::WMouseEvent());
  auto *page = viewSourcePage(app);
  ASSERT_NE(page, nullptr);
  EXPECT_TRUE(containsText(page, "feline"));
  EXPECT_TRUE(containsText(page, "Wiktionary"));

  // Without a gloss of its own the editor starts empty; spaces are trimmed.
  findButton(page, "Edit gloss")->clicked().emit(Wt::WMouseEvent());
  auto *dialog = openGlossDialog(app);
  ASSERT_NE(dialog, nullptr);
  auto *edit = findOne<Wt::WLineEdit>(dialog);
  EXPECT_EQ(edit->text().toUTF8(), "");
  edit->setText("  house cat ");
  findButton(dialog, "Save")->clicked().emit(Wt::WMouseEvent());
  EXPECT_EQ(openGlossDialog(app), nullptr);

  {
    AppContext appContext;
    auto entries = appContext.dictionaryRepository->getByHeadword(
        L"cat", utils::Language::English);
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].gloss, L"house cat");
    EXPECT_EQ(entries[0].source, db::models::DictionarySource::Manual);
  }
  EXPECT_TRUE(containsText(page, "house cat"));
  EXPECT_TRUE(containsText(page, "Your gloss"));
  EXPECT_FALSE(containsText(page, "feline"));

  // Cancel keeps it; saving it empty falls back to the dictionary.
  findButton(page, "Edit gloss")->clicked().emit(Wt::WMouseEvent());
  edit = findOne<Wt::WLineEdit>(openGlossDialog(app));
  EXPECT_EQ(edit->text().toUTF8(), "house cat");
  edit->setText("");
  findButton(openGlossDialog(app), "Cancel")->clicked().emit(Wt::WMouseEvent());
  EXPECT_TRUE(containsText(page, "house cat"));

  findButton(page, "Edit gloss")->clicked().emit(Wt::WMouseEvent());
  findOne<Wt::WLineEdit>(openGlossDialog(app))->setText("");
  findButton(openGlossDialog(app), "Save")->clicked().emit(Wt::WMouseEvent());
  EXPECT_TRUE(containsText(page, "feline"));
  EXPECT_FALSE(containsText(page, "house cat"));
}

namespace {
// Answers every lookup with `answer`, and remembers the queries.
class FakeClaudeClient : public claude::IClaudeClient {
public:
  explicit FakeClaudeClient(claude::WordLookup answer)
      : mAnswer(std::move(answer)) {}

  claude::WordLookup defineWord(const claude::WordQuery &query) override {
    mQueries.push_back(query);
    return mAnswer;
  }

  claude::SentenceExplanation
  explainSentence(const claude::SentenceQuery &query) override {
    mSentenceQueries.push_back(query);
    return mExplanation;
  }

  bool canExplain() const override { return mCanExplain; }
  const claude::GrammarReference *grammar() const override {
    return mGrammar ? &*mGrammar : nullptr;
  }

  claude::WordLookup mAnswer;
  std::vector<claude::WordQuery> mQueries;
  claude::SentenceExplanation mExplanation;
  std::vector<claude::SentenceQuery> mSentenceQueries;
  bool mCanExplain = false;
  std::optional<claude::GrammarReference> mGrammar;
};

std::shared_ptr<FakeClaudeClient> useFakeClaude(claude::WordLookup answer) {
  auto client = std::make_shared<FakeClaudeClient>(std::move(answer));
  AppContext::sharedClaudeClient = client;
  runBackgroundWorkInForeground = true;
  return client;
}

// Opens /source/word for `word` from the word list.
ViewSourcePage *openWord(Wt::WApplication &app, const std::string &word) {
  app.setInternalPath("/list", true);
  findButton(findOne<DisplayWordsPage>(app.root()), word)
      ->clicked()
      .emit(Wt::WMouseEvent());
  return findOne<ViewSourcePage>(app.root());
}

const auto ckb = utils::Language::Kurdish;
} // namespace

TEST_F(PagesTest, AskClaudeOnlyForKurdishWordsWithoutGloss) {
  addText(L"The cat sat.", 1, L"src", utils::Language::English, L"cat");
  addWord(L"cat", utils::Language::English);
  addText(L"پێم وایە درەنگە.", 2, L"src", ckb, L"پێم");
  addWord(L"پێم", ckb);
  addText(L"ئاو خۆشە.", 3, L"src", ckb, L"ئاو");
  addWord(L"ئاو", ckb);
  {
    AppContext appContext;
    appContext.dictionaryRepository->setManualGloss(L"ئاو", ckb, L"water");
  }

  {
    // Without an API key there is no button.
    Wt::Test::WTestEnvironment env;
    LexiCorpMain app{env};
    selectLanguage(app, "Kurdish");
    EXPECT_EQ(findButton(openWord(app, "پێم"), "Ask Claude"), nullptr);
  }

  useFakeClaude({});
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  EXPECT_EQ(findButton(openWord(app, "cat"), "Ask Claude"), nullptr);
  selectLanguage(app, "Kurdish");
  EXPECT_NE(findButton(openWord(app, "پێم"), "Ask Claude"), nullptr);
  EXPECT_EQ(findButton(openWord(app, "ئاو"), "Ask Claude"), nullptr);
}

TEST_F(PagesTest, AskClaudeStoresGlossIdiomsAndUsage) {
  addText(L"پێم وایە درەنگە.", 1, L"src", ckb, L"پێم");
  addWord(L"پێم", ckb);
  addWord(L"وایە", ckb);
  auto client = useFakeClaude(claude::WordLookup{
      .senses = {{L"to me", L"preposition"}},
      .idioms = {{L"پێم وايە", L"", L"i think"}, // a letter variant
                 {L"پێم دەڵێن", L"", L"they tell me"}, // not in the sentence
                 {L"وایە", L"", L"is so"}},             // a single word
      .usage = claude::Usage{.model = "claude-sonnet-5-5",
                             .inputTokens = 900,
                             .outputTokens = 40},
  });

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  selectLanguage(app, "Kurdish");
  auto *page = openWord(app, "پێم");
  findButton(page, "Ask Claude")->clicked().emit(Wt::WMouseEvent());

  ASSERT_EQ(client->mQueries.size(), 1u);
  EXPECT_EQ(client->mQueries[0].word, L"پێم");
  EXPECT_EQ(client->mQueries[0].sentences,
            (std::vector<std::wstring>{L"پێم وایە درەنگە."}));

  {
    AppContext appContext;
    auto entries = appContext.dictionaryRepository->getByHeadword(L"پێم", ckb);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].gloss, L"to me");
    EXPECT_EQ(entries[0].source, db::models::DictionarySource::Claude);
    EXPECT_TRUE(entries[0].aiGenerated);

    // The idiom is stored normalised and linked to both its words.
    auto idiom =
        appContext.dictionaryRepository->getByHeadword(L"پێم وایە", ckb);
    ASSERT_EQ(idiom.size(), 1u);
    EXPECT_EQ(idiom[0].gloss, L"i think");
    EXPECT_EQ(idiom[0].partOfSpeech, L"idiom");
    for (const auto *word : {L"پێم", L"وایە"})
      EXPECT_EQ(appContext.expressionWordRepository->getByWord(word, ckb),
                (std::vector<std::wstring>{L"پێم وایە"}));
    EXPECT_TRUE(appContext.dictionaryRepository
                    ->getByHeadword(L"پێم دەڵێن", ckb)
                    .empty());

    auto usage = appContext.claudeUsageRepository->totalsSince(0);
    ASSERT_EQ(usage.size(), 1u);
    EXPECT_EQ(usage[0].inputTokens, 900);
  }

  EXPECT_TRUE(containsText(page, "to me"));
  EXPECT_TRUE(containsText(page, "Claude (AI-generated)"));
  EXPECT_TRUE(containsText(page, "Part of"));
  EXPECT_TRUE(containsText(page, "i think"));
  EXPECT_EQ(findButton(page, "Ask Claude"), nullptr);
  bool underSentence = false;
  for (auto *widget : findAll<Wt::WContainerWidget>(page))
    underSentence = underSentence ||
                    (widget->hasStyleClass("sentence-expression") &&
                     containsText(widget, "i think"));
  EXPECT_TRUE(underSentence);

  // The other word of the idiom shows it too.
  page = openWord(app, "وایە");
  EXPECT_TRUE(containsText(page, "i think"));
  EXPECT_NE(findButton(page, "Ask Claude"), nullptr);

  app.setInternalPath("/", true);
  auto *home = findOne<HomePage>(app.root());
  EXPECT_TRUE(anyTextContains(home, "Claude this month: 1 lookup · 900 tokens "
                                    "in, 40 out · <$0.01"));
}

TEST_F(PagesTest, AskClaudeShowsErrors) {
  addText(L"پێم وایە درەنگە.", 1, L"src", ckb, L"پێم");
  addWord(L"پێم", ckb);
  useFakeClaude(
      claude::WordLookup{.error = "Claude is overloaded; try again later"});

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  selectLanguage(app, "Kurdish");
  auto *page = openWord(app, "پێم");
  findButton(page, "Ask Claude")->clicked().emit(Wt::WMouseEvent());

  EXPECT_TRUE(containsText(page, "Claude is overloaded; try again later."));
  auto *button = findButton(page, "Ask Claude");
  ASSERT_NE(button, nullptr);
  EXPECT_TRUE(button->isEnabled());
  {
    AppContext appContext;
    EXPECT_TRUE(appContext.claudeUsageRepository->totalsSince(0).empty());
  }

  // Shown once: gone when the page is redrawn.
  page = openWord(app, "پێم");
  EXPECT_FALSE(containsText(page, "Claude is overloaded; try again later."));
}

TEST_F(PagesTest, AskClaudeStoresSplitCompoundVerbsByDictionaryForm) {
  addText(L"ئەوان دوێنێ تاوتوێی پرسەکەی ئاو و کارەبایان کرد.", 1, L"src", ckb,
          L"تاوتوێی");
  addWord(L"تاوتوێی", ckb);
  addWord(L"کرد", ckb);
  useFakeClaude(claude::WordLookup{
      .senses = {{L"discussion", L"noun"}},
      .idioms = {// Split by its object.
                 {L"تاوتوێی کرد", L"تاوتوێ کردن", L"to discuss"},
                 // A whole clause, not an expression.
                 {L"تاوتوێی پرسەکەی ئاو و کارەبایان کرد", L"",
                  L"discussed water and power"}},
  });

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  selectLanguage(app, "Kurdish");
  auto *page = openWord(app, "تاوتوێی");
  findButton(page, "Ask Claude")->clicked().emit(Wt::WMouseEvent());

  {
    AppContext appContext;
    auto idiom =
        appContext.dictionaryRepository->getByHeadword(L"تاوتوێ کردن", ckb);
    ASSERT_EQ(idiom.size(), 1u);
    EXPECT_EQ(idiom[0].gloss, L"to discuss");
    // Both forms of its words are linked to it.
    EXPECT_EQ(appContext.expressionWordRepository->getWords(L"تاوتوێ کردن",
                                                            ckb),
              (std::vector<std::wstring>{L"تاوتوێ", L"تاوتوێی", L"کرد",
                                         L"کردن"}));
    EXPECT_EQ(appContext.expressionWordRepository->getByWord(L"تاوتوێی", ckb),
              (std::vector<std::wstring>{L"تاوتوێ کردن"}));
  }

  EXPECT_TRUE(containsText(page, "تاوتوێ کردن"));
  EXPECT_TRUE(containsText(page, "to discuss"));
  EXPECT_FALSE(containsText(page, "discussed water and power"));
  bool underSentence = false;
  for (auto *widget : findAll<Wt::WContainerWidget>(page))
    underSentence = underSentence ||
                    (widget->hasStyleClass("sentence-expression") &&
                     containsText(widget, "to discuss"));
  EXPECT_TRUE(underSentence);

  EXPECT_TRUE(containsText(openWord(app, "کرد"), "to discuss"));
}

TEST_F(PagesTest, MigrationRemovesClauseLongIdioms) {
  const auto clause = L"تاوتوێی دەرفەتەکانی هاوکاریی هاوبەش و واژۆکردنی "
                      L"گرێبەستێکی وەبەرهێنانیان کردووە";
  {
    // A version 2 database with an idiom that is a whole clause.
    AppContext appContext;
    sqlite::execute(appContext.dbHandler->getConnection(),
                    "PRAGMA user_version = 2", true);
    for (const auto *headword : {clause, L"پێم وایە"})
      appContext.dictionaryRepository->add(
          db::models::DictionaryEntry{.headword = headword,
                                      .language = ckb,
                                      .gloss = L"x",
                                      .partOfSpeech = L"idiom",
                                      .source =
                                          db::models::DictionarySource::Claude,
                                      .aiGenerated = true});
    appContext.expressionWordRepository->add({L"تاوتوێی", ckb, clause});
    appContext.expressionWordRepository->add({L"پێم", ckb, L"پێم وایە"});
  }

  AppContext appContext;
  migrateDatabase(appContext);
  EXPECT_TRUE(appContext.dictionaryRepository->getByHeadword(clause, ckb)
                  .empty());
  EXPECT_TRUE(
      appContext.expressionWordRepository->getByWord(L"تاوتوێی", ckb).empty());
  EXPECT_EQ(
      appContext.dictionaryRepository->getByHeadword(L"پێم وایە", ckb).size(),
      1u);
  EXPECT_EQ(appContext.expressionWordRepository->getByWord(L"پێم", ckb),
            (std::vector<std::wstring>{L"پێم وایە"}));
}

namespace {
// A Claude explanation of "کتێبەکەم خوێندەوە.", as the fake client returns it.
claude::SentenceExplanation bookExplanation() {
  auto explanation = claude::explanationFromJson(R"({
    "translation": "I read the book.",
    "words": [{"surface": "کتێبەکەم", "base_form": "کتێب",
               "base_gloss": "book",
               "morphemes": [
                 {"form": "کتێب", "kind": "stem", "function": "book"},
                 {"form": "ەکە", "kind": "suffix", "function": "definite"},
                 {"form": "م", "kind": "clitic", "function": "1sg agent"}],
               "leipzig": "book-DEF=1SG"},
              {"surface": "خوێندەوە", "base_form": "خوێندنەوە",
               "base_gloss": "to read", "morphemes": [],
               "leipzig": "read.PST"}],
    "constructions": [],
    "notes": [{"text": "The -م is the agent, not 'my'.",
               "sections": ["clitics.agent"]}]
  })");
  explanation.usage = claude::Usage{.model = "claude-sonnet-5-5",
                                    .inputTokens = 30000,
                                    .outputTokens = 800};
  return explanation;
}

const std::wstring kBookText =
    L"ڕۆژی شەممە بوو.\nکتێبەکەم خوێندەوە.\nزۆر خۆش بوو.";

// The explanation panel under a sentence, if one is shown.
Wt::WContainerWidget *shownExplanation(Wt::WWidget *page) {
  for (auto *widget : findAll<Wt::WContainerWidget>(page))
    if (widget->hasStyleClass("explanation") && widget->isVisible())
      return widget;
  return nullptr;
}
} // namespace

TEST_F(PagesTest, ExplainNeedsTheGrammarReferenceAndKurdish) {
  addText(L"The cat sat.", 1, L"src", utils::Language::English, L"cat");
  addWord(L"cat", utils::Language::English);
  addText(kBookText, 2, L"src", ckb, L"کتێبەکەم");
  addWord(L"کتێبەکەم", ckb);

  {
    // Without an API key there is no button.
    Wt::Test::WTestEnvironment env;
    LexiCorpMain app{env};
    selectLanguage(app, "Kurdish");
    EXPECT_EQ(findButton(openWord(app, "کتێبەکەم"), "Explain"), nullptr);
  }
  {
    // Nor without the grammar reference.
    useFakeClaude({});
    Wt::Test::WTestEnvironment env;
    LexiCorpMain app{env};
    selectLanguage(app, "Kurdish");
    EXPECT_EQ(findButton(openWord(app, "کتێبەکەم"), "Explain"), nullptr);
  }

  useFakeClaude({})->mCanExplain = true;
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  EXPECT_EQ(findButton(openWord(app, "cat"), "Explain"), nullptr);
  selectLanguage(app, "Kurdish");
  EXPECT_NE(findButton(openWord(app, "کتێبەکەم"), "Explain"), nullptr);
}

TEST_F(PagesTest, ExplainShowsStoresAndReusesTheExplanation) {
  addText(kBookText, 1, L"src", ckb, L"کتێبەکەم");
  addWord(L"کتێبەکەم", ckb);
  auto client = useFakeClaude({});
  client->mCanExplain = true;
  client->mExplanation = bookExplanation();

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  selectLanguage(app, "Kurdish");
  auto *page = openWord(app, "کتێبەکەم");
  EXPECT_EQ(shownExplanation(page), nullptr);
  findButton(page, "Explain")->clicked().emit(Wt::WMouseEvent());

  // The sentence goes with its neighbours.
  ASSERT_EQ(client->mSentenceQueries.size(), 1u);
  EXPECT_EQ(client->mSentenceQueries[0].sentence, L"کتێبەکەم خوێندەوە.");
  EXPECT_EQ(client->mSentenceQueries[0].before, L"ڕۆژی شەممە بوو.");
  EXPECT_EQ(client->mSentenceQueries[0].after, L"زۆر خۆش بوو.");

  auto *explanation = shownExplanation(page);
  ASSERT_NE(explanation, nullptr);
  EXPECT_TRUE(containsText(explanation, "I read the book."));
  EXPECT_TRUE(containsText(explanation, "book-DEF=1SG"));
  EXPECT_TRUE(containsText(explanation, "Parts"));
  // Affixes shown with a tatweel on their joining side.
  EXPECT_TRUE(anyTextContains(explanation, "ـەکە⁩ (definite)"));
  EXPECT_TRUE(anyTextContains(explanation, "ـم⁩ (1sg agent)"));
  EXPECT_TRUE(containsText(explanation, "The -م is the agent, not 'my'."));
  EXPECT_TRUE(containsText(explanation, " (clitics.agent)"));
  EXPECT_TRUE(
      containsText(explanation, "Claude (AI-generated) · claude-sonnet-5-5"));
  auto *button = findButton(page, "Hide explanation");
  ASSERT_NE(button, nullptr);
  EXPECT_TRUE(button->isEnabled());

  {
    AppContext appContext;
    processor::TextProcessor processor;
    auto stored = appContext.explanationRepository->get(
        sentenceHash(processor, L"کتێبەکەم خوێندەوە."));
    ASSERT_TRUE(stored);
    EXPECT_EQ(stored->answerJson, client->mExplanation.answerJson);
    EXPECT_EQ(stored->model, "claude-sonnet-5-5");
    auto usage = appContext.claudeUsageRepository->totalsSince(0);
    ASSERT_EQ(usage.size(), 1u);
    EXPECT_EQ(usage[0].inputTokens, 30000);
  }

  // The button hides and shows it again.
  button->clicked().emit(Wt::WMouseEvent());
  EXPECT_EQ(shownExplanation(page), nullptr);
  button = findButton(page, "Show explanation");
  ASSERT_NE(button, nullptr);
  button->clicked().emit(Wt::WMouseEvent());
  EXPECT_NE(shownExplanation(page), nullptr);

  // Redrawn, it comes from the database, without asking Claude again.
  page = openWord(app, "کتێبەکەم");
  EXPECT_EQ(shownExplanation(page), nullptr);
  findButton(page, "Show explanation")->clicked().emit(Wt::WMouseEvent());
  explanation = shownExplanation(page);
  ASSERT_NE(explanation, nullptr);
  EXPECT_TRUE(containsText(explanation, "I read the book."));
  EXPECT_EQ(client->mSentenceQueries.size(), 1u);
}

TEST_F(PagesTest, ExplainShowsErrors) {
  addText(kBookText, 1, L"src", ckb, L"کتێبەکەم");
  addWord(L"کتێبەکەم", ckb);
  auto client = useFakeClaude({});
  client->mCanExplain = true;
  // A refusal is billed, so its usage is kept.
  client->mExplanation = claude::SentenceExplanation{
      .usage = claude::Usage{.model = "claude-sonnet-5-5", .inputTokens = 100},
      .error = "Claude declined to answer"};

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  selectLanguage(app, "Kurdish");
  auto *page = openWord(app, "کتێبەکەم");
  findButton(page, "Explain")->clicked().emit(Wt::WMouseEvent());

  EXPECT_TRUE(containsText(page, "Claude declined to answer."));
  EXPECT_EQ(shownExplanation(page), nullptr);
  auto *button = findButton(page, "Explain");
  ASSERT_NE(button, nullptr);
  EXPECT_TRUE(button->isEnabled());
  {
    AppContext appContext;
    processor::TextProcessor processor;
    EXPECT_FALSE(appContext.explanationRepository->get(
        sentenceHash(processor, L"کتێبەکەم خوێندەوە.")));
    EXPECT_EQ(appContext.claudeUsageRepository->totalsSince(0).size(), 1u);
  }

  // Trying again asks again.
  client->mExplanation = bookExplanation();
  button->clicked().emit(Wt::WMouseEvent());
  EXPECT_EQ(client->mSentenceQueries.size(), 2u);
  EXPECT_NE(shownExplanation(page), nullptr);
  bool errorShown = false;
  for (auto *text : findAll<Wt::WText>(page))
    errorShown = errorShown || (text->text() == "Claude declined to answer." &&
                                text->isVisible());
  EXPECT_FALSE(errorShown);
}

TEST(GrammarSectionsTest, MarkdownToXhtml) {
  EXPECT_EQ(markdownToXhtml("#### The affixes\n\nOne line\nand the next."),
            "<h6>The affixes</h6><p>One line and the next.</p>");
  EXPECT_EQ(markdownToXhtml("A **bold** and *italic* `x` <b>&"),
            "<p>A <strong>bold</strong> and <em>italic</em> <code>x</code> "
            "&lt;b&gt;&amp;</p>");
  // Nested lists, and a line continuing the item above.
  EXPECT_EQ(markdownToXhtml("- one\n  1. a\n  2. b\n- two\n  more"),
            "<ul><li>one<ol><li>a</li><li>b</li></ol></li>"
            "<li>two more</li></ul>");
  EXPECT_EQ(markdownToXhtml("| | Affix |\n|---|---|\n| 1sg | ـم |"),
            "<table class=\"table table-sm table-bordered\"><thead><tr>"
            "<th></th><th>Affix</th></tr></thead><tbody><tr>"
            "<td dir=\"auto\">1sg</td><td dir=\"auto\">ـم</td></tr>"
            "</tbody></table>");
}

TEST_F(PagesTest, ExplanationSourcesOpenTheirGrammarSections) {
  addText(kBookText, 1, L"src", ckb, L"کتێبەکەم");
  addWord(L"کتێبەکەم", ckb);
  auto client = useFakeClaude({});
  client->mCanExplain = true;
  client->mExplanation = bookExplanation();
  client->mGrammar = claude::grammarReferenceFrom(
      "<section id=\"clitics.agent\" title=\"Agent clitics\">\n"
      "The **agent** affix (see clitics.sets).\n</section>\n"
      "<section id=\"clitics.sets\" title=\"The clitic sets\">\n"
      "Set I and $et II.\n</section>");

  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};
  selectLanguage(app, "Kurdish");
  auto *page = openWord(app, "کتێبەکەم");
  findButton(page, "Explain")->clicked().emit(Wt::WMouseEvent());
  auto *explanation = shownExplanation(page);
  ASSERT_NE(explanation, nullptr);
  EXPECT_TRUE(containsText(explanation, "Sources"));
  auto *source = findButton(explanation, "Agent clitics");
  ASSERT_NE(source, nullptr);
  // Only the cited section is listed.
  EXPECT_EQ(findButton(explanation, "The clitic sets"), nullptr);

  source->clicked().emit(Wt::WMouseEvent());
  auto *dialog = findOne<Wt::WDialog>(app.domRoot());
  ASSERT_NE(dialog, nullptr);
  EXPECT_EQ(dialog->windowTitle(), "Agent clitics");
  auto *body = findOne<Wt::WTemplate>(dialog->contents());
  ASSERT_NE(body, nullptr);
  EXPECT_EQ(body->templateText(),
            "<p>The <strong>agent</strong> affix (see ${ref0}).</p>");
  auto *back = findButton(dialog, "Back");
  ASSERT_NE(back, nullptr);
  EXPECT_TRUE(back->isHidden());

  // A section it mentions opens in the same dialog, and Back returns.
  findButton(dialog->contents(), "clitics.sets")
      ->clicked()
      .emit(Wt::WMouseEvent());
  EXPECT_EQ(dialog->windowTitle(), "The clitic sets");
  body = findOne<Wt::WTemplate>(dialog->contents());
  EXPECT_EQ(body->templateText(), "<p>Set I and $$et II.</p>");
  EXPECT_FALSE(back->isHidden());
  back->clicked().emit(Wt::WMouseEvent());
  EXPECT_EQ(dialog->windowTitle(), "Agent clitics");
  EXPECT_TRUE(back->isHidden());

  findButton(dialog, "Close")->clicked().emit(Wt::WMouseEvent());
  EXPECT_EQ(findOne<Wt::WDialog>(app.domRoot()), nullptr);
}
