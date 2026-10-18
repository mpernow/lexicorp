#include "pages/LexiCorpMain.h"
#include "pages/DisplayWordsPage.h"
#include "pages/InsertTextPage.h"
#include "pages/HomePage.h"
#include "pages/ViewSourcePage.h"

#include <Wt/WApplication.h>
#include <Wt/WBootstrap5Theme.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WLabel.h>
#include <Wt/WMenu.h>
#include <Wt/WMenuItem.h>
#include <Wt/WStackedWidget.h>
#include <Wt/Http/Cookie.h>
#include <chrono>
#include <memory>

LexiCorpMain::LexiCorpMain(const Wt::WEnvironment &env)
    : Wt::WApplication(env) {

  setTitle("LexiCorp");

  setTheme(std::make_shared<Wt::WBootstrap5Theme>());

  useStyleSheet("resources/style.css");

  mAppContext = std::make_shared<AppContext>();

  // Restore the language chosen in a previous session, if any.
  if (const std::string *cookie = env.getCookie(languageCookieName)) {
    utils::Language language = utils::language_from_string(*cookie);
    if (language != utils::Language::Unknown)
      mAppContext->selectedLanguage = language;
  }

  auto mainLayout = root()->setLayout(std::make_unique<Wt::WHBoxLayout>());
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // --- Sidebar container ---
  auto sidebar = std::make_unique<Wt::WContainerWidget>();
  sidebar->setStyleClass(
      "sidebar bg-body-tertiary border-end p-3 d-flex flex-column");
  sidebar->setWidth(250);

  auto brand = sidebar->addNew<Wt::WText>("LexiCorp");
  brand->setStyleClass("sidebar-brand fs-4 fw-semibold mb-4");
  brand->setInline(false);

  auto languageLabel = sidebar->addNew<Wt::WLabel>("Language");
  languageLabel->setStyleClass("form-label small text-body-secondary");

  auto languageCombo = sidebar->addWidget(std::make_unique<Wt::WComboBox>());
  languageCombo->setStyleClass("form-select mb-4");
  languageLabel->setBuddy(languageCombo);
  mLanguageCombo = languageCombo;

  for (utils::Language language : languages) {
    languageCombo->addItem(utils::to_string(language));
  }
  languageCombo->setCurrentIndex(
      languageCombo->findText(utils::to_string(mAppContext->selectedLanguage)));
  languageCombo->changed().connect([this] {
    std::string selected = mLanguageCombo->currentText().toUTF8();

    mAppContext->selectedLanguage = utils::language_from_string(selected);
    mAppContext->languageChanged.emit(selected);
  });
  mAppContext->languageChanged.connect([this](const std::string &lang) {
    Wt::Http::Cookie cookie{languageCookieName, lang,
                            std::chrono::days{365}};
    cookie.setPath("/");
    setCookie(cookie);

    for (int i = 0; i < mLanguageCombo->count(); ++i) {
      if (mLanguageCombo->itemText(i).toUTF8() == lang) {
        mLanguageCombo->setCurrentIndex(i);
        break;
      }
    }
  });

  auto content =
      mainLayout->addWidget(std::make_unique<Wt::WContainerWidget>(), 1);
  content->setOverflow(Wt::Overflow::Auto);

  auto stack = content->addWidget(std::make_unique<Wt::WStackedWidget>());

  auto menu = std::make_unique<Wt::WMenu>(stack);
  menu->setStyleClass("nav nav-pills flex-column gap-1");

  // Enable URL routing. The base path must be explicit: the default is the
  // internal path at startup, so opening the app on e.g. /list would make
  // the menu ignore every path outside /list/.
  menu->setInternalPathEnabled("/");

  // --- Menu items with URL paths ---
  menu->addItem("Home", std::make_unique<HomePage>(mAppContext))
      ->setPathComponent("");
  menu->addItem("Insert Text", std::make_unique<InsertTextPage>(mAppContext))
      ->setPathComponent("insert");
  menu->addItem("Word List", std::make_unique<DisplayWordsPage>(mAppContext))
      ->setPathComponent("list");
  menu->addItem("View source", std::make_unique<ViewSourcePage>(mAppContext))
      ->setPathComponent("source");

  // Wt doesn't mark the item that is selected when the page is first rendered
  // as active, so keep the Bootstrap "active" class in sync ourselves.
  Wt::WMenu *menuPtr = menu.get();
  auto markActive = [menuPtr] {
    for (int i = 0; i < menuPtr->count(); ++i)
      menuPtr->itemAt(i)->anchor()->toggleStyleClass(
          "active", i == menuPtr->currentIndex());
  };
  menu->itemSelected().connect(markActive);

  // --- Add menu to sidebar ---
  sidebar->addWidget(std::move(menu));

  // --- Dark mode switch, pinned to the bottom of the sidebar ---
  mDarkModeSwitch = sidebar->addNew<Wt::WCheckBox>("Dark mode");
  mDarkModeSwitch->addStyleClass("form-switch mt-auto pt-3");
  mDarkModeSwitch->changed().connect([this] {
    bool dark = mDarkModeSwitch->isChecked();
    applyTheme(dark);

    Wt::Http::Cookie cookie{themeCookieName, dark ? "dark" : "light",
                            std::chrono::days{365}};
    cookie.setPath("/");
    setCookie(cookie);
  });

  const std::string *themeCookie = env.getCookie(themeCookieName);
  if (themeCookie && (*themeCookie == "dark" || *themeCookie == "light")) {
    applyTheme(*themeCookie == "dark");
  } else {
    applyTheme(false);
    mSystemPrefersDark.connect([this](bool dark) { applyTheme(dark); });
    doJavaScript(mSystemPrefersDark.createCall(
        {"window.matchMedia('(prefers-color-scheme: dark)').matches"}));
  }

  // --- Add to layout ---
  mainLayout->insertWidget(0, std::move(sidebar));

  // Default route
  if (internalPath().empty() || internalPath() == "/") {
    setInternalPath("/", true);
  }
  markActive();
}

void LexiCorpMain::applyTheme(bool dark) {
  setHtmlAttribute("data-bs-theme", dark ? "dark" : "light");
  mDarkModeSwitch->setChecked(dark);
}
