#include "pages/HomePage.h"
#include <Wt/WApplication.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>
#include <algorithm>
#include <memory>

namespace {
// Adds a stat tile (big number over a small caption) and returns the number.
Wt::WText *addStat(Wt::WContainerWidget *row, const std::string &caption) {
  auto col = row->addNew<Wt::WContainerWidget>();
  col->setStyleClass("col");
  auto card = col->addNew<Wt::WContainerWidget>();
  card->setStyleClass("card h-100 text-center");
  auto body = card->addNew<Wt::WContainerWidget>();
  body->setStyleClass("card-body");

  auto value = body->addNew<Wt::WText>("0");
  value->setStyleClass("stat-value");
  value->setInline(false);
  auto label = body->addNew<Wt::WText>(caption);
  label->setStyleClass("small text-body-secondary");
  label->setInline(false);
  return value;
}
} // namespace

HomePage::HomePage(std::shared_ptr<AppContext> appContext)
    : mAppContext(appContext) {

  auto container = addWidget(std::make_unique<Wt::WContainerWidget>());
  container->setStyleClass("main-container");

  auto title = container->addNew<Wt::WText>("Welcome to LexiCorp");
  title->setStyleClass("h3 mb-1");
  title->setInline(false);

  mSubtitle = container->addNew<Wt::WText>();
  mSubtitle->setStyleClass("text-body-secondary mb-4");
  mSubtitle->setInline(false);

  auto stats = container->addNew<Wt::WContainerWidget>();
  stats->setStyleClass("row row-cols-3 g-2 g-sm-3 mb-4");
  mWordCount = addStat(stats, "Words");
  mKnownCount = addStat(stats, "Known");
  mTextCount = addStat(stats, "Texts");

  auto actions = container->addNew<Wt::WContainerWidget>();
  actions->setStyleClass("d-flex flex-wrap gap-2");
  auto insert = actions->addNew<Wt::WPushButton>("Insert a text");
  insert->setStyleClass("btn-primary");
  insert->clicked().connect([] {
    Wt::WApplication::instance()->setInternalPath("/insert", true);
  });
  auto list = actions->addNew<Wt::WPushButton>("Browse words");
  list->setStyleClass("btn-outline-secondary");
  list->clicked().connect([] {
    Wt::WApplication::instance()->setInternalPath("/list", true);
  });

  refresh();

  mAppContext->languageChanged.connect([this] { refresh(); });
  Wt::WApplication::instance()->internalPathChanged().connect(
      [this](const std::string &path) {
        if (path.empty() || path == "/")
          refresh();
      });
}

void HomePage::refresh() {
  utils::Language language = mAppContext->selectedLanguage;
  auto words = mAppContext->wordRepository->getAll(language);
  auto known = std::count_if(words.begin(), words.end(),
                             [](const auto &word) { return word.known; });
  auto texts = mAppContext->textRepository->getAll(language);

  mSubtitle->setText("Your " + utils::to_string(language) +
                     " corpus at a glance.");
  mWordCount->setText(std::to_string(words.size()));
  mKnownCount->setText(std::to_string(known));
  mTextCount->setText(std::to_string(texts.size()));
}
