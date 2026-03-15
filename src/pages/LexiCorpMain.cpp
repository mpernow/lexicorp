#include "pages/LexiCorpMain.h"
#include "db/DatabaseHandler.h"
#include "pages/InsertTextPage.h"
#include "pages/LanguageSelectionPage.h"
#include <Wt/WApplication.h>
#include <Wt/WBootstrapTheme.h>
#include <Wt/WComboBox.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WMenu.h>
#include <Wt/WNavigationBar.h>
#include <Wt/WPushButton.h>
#include <Wt/WStackedWidget.h>
#include <Wt/WText.h>
#include <memory.h>

LexiCorpMain::LexiCorpMain(const Wt::WEnvironment &env)
    : Wt::WApplication(env) {
  setTitle("LexiCorp");

  useStyleSheet("resources/style.css");

  mDatabaseHandler = std::make_shared<db::DatabaseHandler>();
  mWordRepository =
      std::make_shared<db::WordRepository>(mDatabaseHandler->getConnection());

  internalPathChanged().connect(this, &LexiCorpMain::handlePath);
  handlePath(internalPath());
}

void LexiCorpMain::handlePath(const std::string &path) {
  root()->clear();

  if (path == "/") {
    root()->addWidget(std::make_unique<LanguageSelectionPage>());
  } else if (path == "/insert") {
    root()->addWidget(std::make_unique<InsertTextPage>(mWordRepository));
  } else if (path == "/page2") {
    root()->addWidget(std::make_unique<Wt::WText>("This is Page 2"));
  } else {
    auto btn =
        root()->addWidget(std::make_unique<Wt::WPushButton>("Go to Page 2"));
    btn->clicked().connect([this] { setInternalPath("/page2", true); });
  }
}
