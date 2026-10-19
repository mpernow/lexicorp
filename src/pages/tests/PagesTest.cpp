#include "db/DatabaseHandler.h"
#include "db/TextRepository.h"
#include "db/Utils.h"
#include "db/WordRepository.h"
#include "db/WordTextRepository.h"
#include "pages/AppContext.h"
#include "pages/DisplayWordsPage.h"
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
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

#include <algorithm>
#include <filesystem>
#include <map>
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
  EXPECT_TRUE(findButton(page, "Show Unknown Only")->isHidden());
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
  EXPECT_TRUE(containsText(page, "Source: first-source"));
  EXPECT_TRUE(containsText(page, "Source: (no source)"));
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
  EXPECT_TRUE(containsText(page, "Sentences containing \"cat\" (1)"));
  EXPECT_TRUE(containsText(page, "Source: first-source"));
  EXPECT_FALSE(containsText(page, "Source: second-source"));
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
  EXPECT_FALSE(containsText(page, "Source: doomed"));
  EXPECT_TRUE(containsText(page, "Source: kept"));

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
  EXPECT_TRUE(containsText(page, "Sentences containing \"cat\" (3)"));
  EXPECT_FALSE(containsText(page, "Source: c-source"));

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
            (std::vector<std::string>{"Source: a-source", "Source: b-source"}));
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
