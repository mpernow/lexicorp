#include "pages/LexiCorpMain.h"
#include "pages/DisplayWordsPage.h"
#include "pages/InsertTextPage.h"
#include "pages/HomePage.h"
#include "pages/ViewSourcePage.h"

#include <Wt/WApplication.h>
#include <Wt/WBootstrap5Theme.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WLabel.h>
#include <Wt/WMenu.h>
#include <Wt/WMenuItem.h>
#include <Wt/WStackedWidget.h>
#include <Wt/Http/Cookie.h>
#include <Wt/WButtonGroup.h>
#include <Wt/WRadioButton.h>
#include <chrono>
#include <memory>

LexiCorpMain::LexiCorpMain(const Wt::WEnvironment &env)
    : Wt::WApplication(env) {

  setTitle("LexiCorp");

  setTheme(std::make_shared<Wt::WBootstrap5Theme>());

  useStyleSheet("resources/style.css");

  addHeadTags();

  mAppContext = std::make_shared<AppContext>();

  // Restore the language chosen in a previous session, if any.
  if (const std::string *cookie = env.getCookie(languageCookieName)) {
    utils::Language language = utils::language_from_string(*cookie);
    if (language != utils::Language::Unknown)
      mAppContext->selectedLanguage = language;
  }

  // Plain flexbox rather than a Wt layout manager, so CSS media queries can
  // turn the sidebar into a top bar on narrow screens (see style.css).
  root()->addStyleClass("app-shell");

  // --- Sidebar container ---
  auto sidebar = root()->addNew<Wt::WContainerWidget>();
  sidebar->setStyleClass("sidebar bg-body-tertiary");

  auto brand = sidebar->addNew<Wt::WText>("LexiCorp");
  brand->setStyleClass("sidebar-brand fs-4 fw-semibold mb-4");
  brand->setInline(false);

  auto languageGroup = sidebar->addNew<Wt::WContainerWidget>();
  languageGroup->setStyleClass("sidebar-language");
  auto languageLabel = languageGroup->addNew<Wt::WLabel>("Language");
  languageLabel->setStyleClass("form-label small text-body-secondary");

  auto languageCombo =
      languageGroup->addWidget(std::make_unique<Wt::WComboBox>());
  languageCombo->setStyleClass("form-select");
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

  auto content = root()->addNew<Wt::WContainerWidget>();
  content->setStyleClass("app-content");

  auto stack = content->addWidget(std::make_unique<Wt::WStackedWidget>());
  // WStackedWidget clips its contents by default, which would stop sticky
  // table headers and the sticky Submit button from sticking.
  stack->setOverflow(Wt::Overflow::Visible);

  auto menu = std::make_unique<Wt::WMenu>(stack);
  menu->setStyleClass("nav nav-pills sidebar-nav gap-1");

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

  addModelChoice(sidebar, env);

  // --- Dark mode switch, pinned to the bottom of the sidebar ---
  mDarkModeSwitch = sidebar->addNew<Wt::WCheckBox>("Dark mode");
  mDarkModeSwitch->addStyleClass("form-switch theme-switch");
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

  // Default route
  if (internalPath().empty() || internalPath() == "/") {
    setInternalPath("/", true);
  }
  markActive();
}

void LexiCorpMain::addModelChoice(Wt::WContainerWidget *sidebar,
                                  const Wt::WEnvironment &env) {
  const auto &client = mAppContext->claudeClient;
  if (!client || !client->canExplain())
    return;

  if (const std::string *cookie = env.getCookie(explainModelCookieName))
    for (const auto &choice : claude::kExplainModels)
      if (*cookie == choice.id)
        mAppContext->explainModel = choice.id;

  auto group = sidebar->addNew<Wt::WContainerWidget>();
  group->setStyleClass("sidebar-model small");
  group->addNew<Wt::WText>("Explain with")
      ->setStyleClass("sidebar-model-label text-body-secondary");
  // The group only makes the radio buttons exclusive.
  mExplainModelButtons = std::make_shared<Wt::WButtonGroup>();
  auto current = mAppContext->currentExplainModel();
  for (const auto &choice : claude::kExplainModels) {
    auto radio = group->addNew<Wt::WRadioButton>(choice.label);
    radio->addStyleClass("form-check-inline");
    mExplainModelButtons->addButton(radio);
    radio->setChecked(current == choice.id);
    radio->checked().connect([this, id = std::string(choice.id)] {
      mAppContext->explainModel = id;
      Wt::Http::Cookie cookie{explainModelCookieName, id,
                              std::chrono::days{365}};
      cookie.setPath("/");
      setCookie(cookie);
      mAppContext->explainModelChanged.emit();
    });
  }
}

void LexiCorpMain::addHeadTags() {
  // Wt's JavaScript bootstrap page leaves out headers set with
  // addMetaHeader()/addMetaLink(), so add the tags from the browser instead.
  auto url = [this](const std::string &path) {
    return Wt::WString(resolveRelativeUrl(path)).jsStringLiteral();
  };
  doJavaScript(
      "(function() {"
      "  function add(tag, attrs) {"
      "    var e = document.createElement(tag);"
      "    for (var k in attrs) e.setAttribute(k, attrs[k]);"
      "    document.head.appendChild(e);"
      "  }"
      // Phones otherwise lay the page out at desktop width and zoom out.
      "  add('meta', {name: 'viewport',"
      "               content: 'width=device-width, initial-scale=1'});"
      // Installable as a home-screen app (see resources/manifest.json).
      "  add('meta', {name: 'theme-color', content: '#0d6efd'});"
      "  add('meta', {name: 'mobile-web-app-capable', content: 'yes'});"
      "  add('meta', {name: 'apple-mobile-web-app-capable', content: 'yes'});"
      "  add('meta', {name: 'apple-mobile-web-app-title',"
      "               content: 'LexiCorp'});"
      "  add('link', {rel: 'manifest', href: " +
      url("resources/manifest.json") +
      "});"
      "  add('link', {rel: 'apple-touch-icon', sizes: '180x180', href: " +
      url("resources/icons/apple-touch-icon.png") +
      "});"
      "  add('link', {rel: 'icon', type: 'image/png', sizes: '32x32', href: " +
      url("resources/icons/favicon-32.png") +
      "});"
      "})();");
}

void LexiCorpMain::applyTheme(bool dark) {
  setHtmlAttribute("data-bs-theme", dark ? "dark" : "light");
  mDarkModeSwitch->setChecked(dark);
}
