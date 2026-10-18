#include "db/DatabaseHandler.h"
#include "db/TextRepository.h"
#include "db/WordRepository.h"
#include "db/WordTextRepository.h"
#include "pages/AppContext.h"
#include "pages/DisplayWordsPage.h"
#include "pages/InsertTextPage.h"
#include "pages/HomePage.h"
#include "pages/LexiCorpMain.h"
#include "pages/ViewSourcePage.h"

#include <Wt/Test/WTestEnvironment.h>
#include <Wt/WCheckBox.h>
#include <Wt/WComboBox.h>
#include <Wt/WEvent.h>
#include <Wt/WMenu.h>
#include <Wt/WMenuItem.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
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

  void addText(const std::wstring &text, int hash, const std::wstring &source,
               utils::Language language) {
    db::DatabaseHandler dbHandler;
    db::TextRepository texts{dbHandler.getConnection()};
    db::WordTextRepository wordTexts{dbHandler.getConnection()};
    texts.add(db::models::Text{text, hash, language, source});
    // Only the first word is linked; enough for the "View source" tests.
    std::wstring firstWord = text.substr(0, text.find(L' '));
    wordTexts.add(db::models::WordText{firstWord, hash, 1, language});
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

  app.setInternalPath("/insert", true);
  EXPECT_NO_THROW(submitButton->clicked().emit(Wt::WMouseEvent()));
  EXPECT_TRUE(containsText(page, "This text has already been added."));

  db::DatabaseHandler dbHandler;
  db::WordRepository words{dbHandler.getConnection()};
  auto the = words.getByText(L"the", utils::Language::English);
  ASSERT_TRUE(the.has_value());
  EXPECT_EQ(the->numOccurrences, 2);
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
  EXPECT_TRUE(containsText(page, "Sources containing \"cat\""));
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
