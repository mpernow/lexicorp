#include "db/DatabaseHandler.h"
#include "db/WordRepository.h"
#include "pages/AppContext.h"
#include "pages/DisplayWordsPage.h"
#include "pages/InsertTextPage.h"
#include "pages/LanguageSelectionPage.h"
#include "pages/LexiCorpMain.h"

#include <Wt/Test/WTestEnvironment.h>
#include <Wt/WComboBox.h>
#include <Wt/WEvent.h>
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

// The sidebar combo box is the one that is not part of LanguageSelectionPage.
Wt::WComboBox *findSidebarCombo(Wt::WApplication &app) {
  for (auto *combo : findAll<Wt::WComboBox>(app.root())) {
    Wt::WWidget *ancestor = combo->parent();
    while (ancestor && !dynamic_cast<LanguageSelectionPage *>(ancestor))
      ancestor = ancestor->parent();
    if (!ancestor)
      return combo;
  }
  return nullptr;
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

  auto combos = findAll<Wt::WComboBox>(app.root());
  ASSERT_EQ(combos.size(), 2u);
  for (auto *combo : combos)
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
  EXPECT_TRUE(containsText(page, "kurdishword"));
  EXPECT_FALSE(containsText(page, "englishword"));
}

TEST_F(PagesTest, SidebarChangeUpdatesLanguagePageCombo) {
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};

  auto *sidebarCombo = findSidebarCombo(app);
  ASSERT_NE(sidebarCombo, nullptr);
  sidebarCombo->setCurrentIndex(sidebarCombo->findText("French"));
  sidebarCombo->changed().emit();

  auto *page = findOne<LanguageSelectionPage>(app.root());
  ASSERT_NE(page, nullptr);
  auto *pageCombo = findOne<Wt::WComboBox>(page);
  ASSERT_NE(pageCombo, nullptr);
  EXPECT_EQ(pageCombo->currentText().toUTF8(), "French");
}

TEST_F(PagesTest, WordListReloadsWhenNavigatedTo) {
  Wt::Test::WTestEnvironment env;
  LexiCorpMain app{env};

  // The word list page is built (and first loads) before the word exists.
  addWord(L"lateword", utils::Language::English);
  app.setInternalPath("/list", true);

  auto *page = findOne<DisplayWordsPage>(app.root());
  ASSERT_NE(page, nullptr);
  EXPECT_TRUE(containsText(page, "lateword"));
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
  EXPECT_TRUE(containsText(list, "the"));

  app.setInternalPath("/insert", true);
  EXPECT_NO_THROW(submitButton->clicked().emit(Wt::WMouseEvent()));
  EXPECT_TRUE(containsText(page, "This text has already been added."));

  db::DatabaseHandler dbHandler;
  db::WordRepository words{dbHandler.getConnection()};
  auto the = words.getByText(L"the", utils::Language::English);
  ASSERT_TRUE(the.has_value());
  EXPECT_EQ(the->numOccurrences, 2);
}
