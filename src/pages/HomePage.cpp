#include "pages/HomePage.h"
#include <Wt/WApplication.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
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

// 12345 -> "12.3k"
std::string tokens(std::int64_t count) {
  if (count < 1000)
    return std::to_string(count);
  char buffer[32];
  std::snprintf(buffer, sizeof buffer, "%.1fk", count / 1000.0);
  return buffer;
}

// Midnight UTC on the first of this month, in Unix time.
std::int64_t startOfMonth() {
  using namespace std::chrono;
  year_month_day today{floor<days>(system_clock::now())};
  sys_days first{today.year() / today.month() / 1};
  return duration_cast<seconds>(first.time_since_epoch()).count();
}

// "Claude this month: 3 lookups · 4.2k tokens in, 310 out · $0.01", or empty
// without requests.
std::string usageSummary(const std::vector<db::models::ClaudeUsage> &usage) {
  auto requests = static_cast<std::int64_t>(usage.size());
  std::int64_t input = 0, output = 0;
  double cost = 0;
  bool costKnown = true;
  // Priced one by one: Haiku's price depends on the request's size.
  for (const auto &request : usage) {
    input += request.inputTokens + request.cacheReadTokens +
             request.cacheCreationTokens;
    output += request.outputTokens;
    auto requestCost = claude::costOf(claude::Usage{
        .model = request.model,
        .inputTokens = request.inputTokens,
        .outputTokens = request.outputTokens,
        .cacheReadTokens = request.cacheReadTokens,
        .cacheCreationTokens = request.cacheCreationTokens,
    });
    costKnown = costKnown && requestCost;
    cost += requestCost.value_or(0);
  }
  if (requests == 0)
    return {};

  std::string summary = "Claude this month: " + std::to_string(requests) +
                        (requests == 1 ? " lookup" : " lookups") + " · " +
                        tokens(input) + " tokens in, " + tokens(output) +
                        " out";
  if (costKnown && cost > 0 && cost < 0.005) {
    summary += " · <$0.01";
  } else if (costKnown) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, " · $%.2f", cost);
    summary += buffer;
  }
  return summary;
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

  mClaudeUsage = container->addNew<Wt::WText>();
  mClaudeUsage->setTextFormat(Wt::TextFormat::Plain);
  mClaudeUsage->setStyleClass("claude-usage small text-body-secondary mb-4");
  mClaudeUsage->setInline(false);

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

  // For all languages: it is one bill.
  auto usage = usageSummary(
      mAppContext->claudeUsageRepository->requestsSince(startOfMonth()));
  mClaudeUsage->setText(Wt::WString::fromUTF8(usage));
  mClaudeUsage->setHidden(usage.empty());
}
