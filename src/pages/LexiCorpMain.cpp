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

  // --- Content area ---
  auto languageText = sidebar->addWidget(std::make_unique<Wt::WText>());
  languageText->setStyleClass("mb-3 fw-bold");
  mLanguageText = languageText;
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
  mainLayout->addWidget(std::move(stack), 1); // stretch content

  // Default route
  if (internalPath().empty() || internalPath() == "/") {
    setInternalPath("/", true);
  }

  updateLanguageDisplay();
  mAppContext->languageChanged.connect(this,
                                       &LexiCorpMain::updateLanguageDisplay);
}

void LexiCorpMain::updateLanguageDisplay() {
  if (utils::to_string(mAppContext->selectedLanguage).empty()) {
    mLanguageText->setText("No language selected");
  } else {
    mLanguageText->setTextFormat(Wt::TextFormat::XHTML);
    mLanguageText->setText("<strong>Language:</strong><br/>" +
                           utils::to_string(mAppContext->selectedLanguage));
  }
}
