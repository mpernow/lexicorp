#include "pages/ViewSourcePage.h"
#include "pages/ConfirmDialog.h"
#include "pages/EmptyState.h"
#include "db/Savepoint.h"
#include "db/Text.h"
#include "db/Utils.h"
#include "db/WordText.h"
#include <Wt/WApplication.h>
#include <Wt/WBreak.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>
#include <algorithm>
#include <unicode/brkiter.h>
#include <unicode/unistr.h>
#include <unicode/ustream.h>

ViewSourcePage::ViewSourcePage(std::shared_ptr<AppContext> appContext)
    : mAppContext(appContext) {

  auto container = addWidget(std::make_unique<Wt::WContainerWidget>());
  container->setStyleClass("main-container");

  mHeading = container->addNew<Wt::WText>();
  mHeading->setInline(false);
  mHeading->setStyleClass("h5 mb-3");

  mTextsContainer =
      container->addWidget(std::make_unique<Wt::WContainerWidget>());

  refresh();

  Wt::WApplication::instance()->internalPathChanged().connect(
      [this](const std::string &path) {
        if (path.starts_with("/source"))
          refresh();
      });
  mAppContext->languageChanged.connect([this] { refresh(); });
}

std::string htmlEscape(const std::string &input) {
  std::string out;
  out.reserve(input.size());

  for (char c : input) {
    switch (c) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    case '"':
      out += "&quot;";
      break;
    case '\'':
      out += "&#39;";
      break;
    default:
      out += c;
      break;
    }
  }
  return out;
}

// `utf8Word` is a stored word, so tokens of the raw text get the same letter
// normalisation before comparing.
std::string highlightWithBreakIterator(const std::string &utf8Text,
                                       const std::string &utf8Word,
                                       const processor::TextProcessor &letters,
                                       utils::Language language) {
  UErrorCode status = U_ZERO_ERROR;

  icu::UnicodeString text = icu::UnicodeString::fromUTF8(utf8Text);

  icu::UnicodeString word = icu::UnicodeString::fromUTF8(utf8Word);

  std::unique_ptr<icu::BreakIterator> it(icu::BreakIterator::createWordInstance(
      icu::Locale::getDefault(), status));

  it->setText(text);

  word.foldCase();

  std::string result;

  int32_t start = it->first();
  int32_t end = it->next();

  while (end != icu::BreakIterator::DONE) {

    icu::UnicodeString token = text.tempSubStringBetween(start, end);

    icu::UnicodeString tokenCopy = token;
    letters.normalizeLetters(tokenCopy, language);
    tokenCopy.foldCase();

    std::string utf8Token;
    token.toUTF8String(utf8Token);

    std::string escaped = htmlEscape(utf8Token);

    if ((it->getRuleStatus() != UBRK_WORD_NONE) && tokenCopy == word) {
      result += "<span class=\"highlight-word\">";
      result += escaped;
      result += "</span>";
    } else {
      result += escaped;
    }

    start = end;
    end = it->next();
  }

  return result;
}

void ViewSourcePage::refresh() {
  mTextsContainer->clear();

  if (Wt::WApplication::instance()->internalPath() == "/source/word" &&
      !mAppContext->selectedWord.empty()) {
    showWordSources();
  } else {
    showAllSources();
  }
}

void ViewSourcePage::showWordSources() {
  const std::wstring &selectedWord = mAppContext->selectedWord;
  std::string utf8Word = db::Utils::toUtf8(selectedWord);

  auto wordTextVec = mAppContext->wordTextRepository->getByWord(
      selectedWord, mAppContext->selectedLanguage);
  std::sort(wordTextVec.begin(), wordTextVec.end(),
            [](const db::models::WordText &wt1,
               const db::models::WordText &wt2) {
              return wt1.numOccurrences > wt2.numOccurrences;
            });

  mHeading->setText(Wt::WString::fromUTF8("Sources containing \"" + utf8Word +
                                          "\""));
  mHeading->setTextFormat(Wt::TextFormat::Plain);

  if (wordTextVec.empty()) {
    mTextsContainer->addWidget(makeEmptyState(
        "No stored text contains \"" + utf8Word + "\".", false));
  }

  for (const db::models::WordText &wt : wordTextVec) {
    // TODO: Handle case of std::optional being nullopt - it is an error
    if (auto text = mAppContext->textRepository->getByHash(wt.textHash)) {
      addSourceBlock(*text, highlightWithBreakIterator(
                                db::Utils::toUtf8(text->text), utf8Word,
                                mTextProcessor, mAppContext->selectedLanguage));
    }
  }
}

void ViewSourcePage::showAllSources() {
  auto texts =
      mAppContext->textRepository->getAll(mAppContext->selectedLanguage);

  mHeading->setText("All sources (" + std::to_string(texts.size()) + ")");
  mHeading->setTextFormat(Wt::TextFormat::Plain);

  if (texts.empty()) {
    mTextsContainer->addWidget(makeEmptyState(
        "No " + utils::to_string(mAppContext->selectedLanguage) +
            " texts yet. Inserted texts and their sources will appear here.",
        true));
  }

  for (const db::models::Text &text : texts) {
    addSourceBlock(text, htmlEscape(db::Utils::toUtf8(text.text)));
  }
}

void ViewSourcePage::addSourceBlock(const db::models::Text &text,
                                    const std::string &textHtml) {
  auto textBlock = mTextsContainer->addNew<Wt::WContainerWidget>();
  textBlock->setStyleClass("card mb-3");

  auto header = textBlock->addNew<Wt::WContainerWidget>();
  header->setStyleClass("card-header d-flex align-items-center gap-2");

  std::string source =
      text.source.empty() ? "(no source)" : db::Utils::toUtf8(text.source);
  auto sourceWidget = header->addNew<Wt::WText>(
      Wt::WString::fromUTF8("Source: " + source), Wt::TextFormat::Plain);
  sourceWidget->setAttributeValue("dir", "auto");
  sourceWidget->setStyleClass("small text-body-secondary flex-grow-1 "
                              "text-break");

  auto deleteButton = header->addNew<Wt::WPushButton>("Delete");
  deleteButton->setStyleClass("btn-sm btn-outline-danger flex-shrink-0");
  deleteButton->setToolTip("Delete this text");
  deleteButton->clicked().connect(
      [this, hash = text.hash, source = db::Utils::toUtf8(text.source)] {
        confirmDelete(hash, source);
      });

  auto textWidget = textBlock->addNew<Wt::WText>(
      Wt::WString::fromUTF8(textHtml), Wt::TextFormat::XHTML);
  textWidget->setInline(false);
  textWidget->setStyleClass("card-body source-text");
  textWidget->setAttributeValue("dir", "auto");
}

void ViewSourcePage::confirmDelete(int hash, const std::string &source) {
  confirmDestructive(
      this, "Delete text",
      (source.empty() ? "Delete this text?"
                      : "Delete the text from \"" + source + "\"?") +
          " Its words' counts are lowered by their occurrences in it, and "
          "words that only appeared in this text are removed unless marked "
          "known.",
      "Delete", [this, hash] { deleteText(hash); });
}

void ViewSourcePage::deleteText(int hash) {
  db::Savepoint tx{mAppContext->dbHandler->getConnection(), "delete_text"};

  std::vector<db::models::Word> occurrences;
  for (const auto &wt : mAppContext->wordTextRepository->getAllByHash(hash))
    occurrences.emplace_back(wt.word, wt.numOccurrences, false, wt.language);
  mAppContext->wordRepository->subtractFrequencies(occurrences);
  mAppContext->wordTextRepository->removeByHash(hash);
  mAppContext->textRepository->remove(hash);
  tx.commit();

  refresh();
}
