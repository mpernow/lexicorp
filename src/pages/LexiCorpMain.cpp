#include "pages/LexiCorpMain.h"
#include "pages/DisplayWordsPage.h"
#include "pages/InsertTextPage.h"
#include "pages/LanguageSelectionPage.h"

#include <Wt/WApplication.h>
#include <Wt/WBootstrapTheme.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WMenu.h>
#include <Wt/WStackedWidget.h>
#include <memory>

LexiCorpMain::LexiCorpMain(const Wt::WEnvironment &env)
    : Wt::WApplication(env) {

  setTitle("LexiCorp");

  auto theme = std::make_shared<Wt::WBootstrapTheme>();
  theme->setVersion(Wt::BootstrapVersion::v2);
  setTheme(theme);

  useStyleSheet("resources/style.css");

  mAppContext = std::make_shared<AppContext>();

  auto mainLayout = root()->setLayout(std::make_unique<Wt::WHBoxLayout>());

  // --- Sidebar container ---
  auto sidebar = std::make_unique<Wt::WContainerWidget>();
  sidebar->setStyleClass("sidebar bg-light p-3");
  sidebar->setWidth(250);

  sidebar->addWidget(std::make_unique<Wt::WText>("Language"));

  auto languageCombo = sidebar->addWidget(std::make_unique<Wt::WComboBox>());
  languageCombo->setStyleClass("form-select mb-3");
  mLanguageCombo = languageCombo;

  for (utils::Language language : languages) {
    languageCombo->addItem(utils::to_string(language));
  }
  languageCombo->changed().connect([this] {
    std::string selected = mLanguageCombo->currentText().toUTF8();

    mAppContext->selectedLanguage = utils::language_from_string(selected);
    mAppContext->languageChanged.emit(selected);
  });
  mAppContext->languageChanged.connect([this](const std::string &lang) {
    for (int i = 0; i < mLanguageCombo->count(); ++i) {
      if (mLanguageCombo->itemText(i).toUTF8() == lang) {
        mLanguageCombo->setCurrentIndex(i);
        break;
      }
    }
  });

  auto stack = std::make_unique<Wt::WStackedWidget>();

  // Keep raw pointer before moving ownership
  Wt::WStackedWidget *stackPtr = stack.get();

  // --- Menu (linked to stacked widget) ---
  auto menu = std::make_unique<Wt::WMenu>(stackPtr);
  menu->setStyleClass("nav flex-column nav-stacked");

  // Enable URL routing
  menu->setInternalPathEnabled();

  // --- Pages ---
  auto langPage = std::make_unique<LanguageSelectionPage>(mAppContext);
  auto insertPage = std::make_unique<InsertTextPage>(mAppContext);
  auto listPage = std::make_unique<DisplayWordsPage>(mAppContext);

  // --- Menu items with URL paths ---
  menu->addItem("Language", std::move(langPage))->setPathComponent("");
  menu->addItem("Insert Text", std::move(insertPage))
      ->setPathComponent("insert");
  menu->addItem("Word List", std::move(listPage))->setPathComponent("list");

  // --- Add menu to sidebar ---
  sidebar->addWidget(std::move(menu));

  // --- Add to layout ---
  mainLayout->addWidget(std::move(sidebar));

  auto content =
      mainLayout->addWidget(std::make_unique<Wt::WContainerWidget>(), 1);
  content->setOverflow(Wt::Overflow::Auto);

  content->addWidget(std::move(stack));

  // Default route
  if (internalPath().empty() || internalPath() == "/") {
    setInternalPath("/", true);
  }
}
