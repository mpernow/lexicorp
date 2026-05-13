#include "pages/ViewSourcePage.h"
#include "db/Text.h"
#include "db/Utils.h"
#include "db/WordText.h"
#include <Wt/Utils.h>
#include <Wt/WBreak.h>
#include <Wt/WText.h>
#include <algorithm>
#include <unicode/brkiter.h>
#include <unicode/unistr.h>
#include <unicode/ustream.h>

ViewSourcePage::ViewSourcePage(std::shared_ptr<AppContext> appContext)
    : mAppContext(appContext) {

  auto container = addWidget(std::make_unique<Wt::WContainerWidget>());
  container->setStyleClass("main-container");

  mTextsContainer =
      container->addWidget(std::make_unique<Wt::WContainerWidget>());

  WordSelected();

  mAppContext->wordSelected.connect([this] { WordSelected(); });
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

std::string highlightWithBreakIterator(const std::string &utf8Text,
                                       const std::string &utf8Word) {
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

void ViewSourcePage::WordSelected() {
  mSelectedWord = mAppContext->selectedWord;

  mTextsVec.clear();
  mTextsContainer->clear();

  auto wordTextVec = mAppContext->wordTextRepository->getByWord(
      mSelectedWord, mAppContext->selectedLanguage);
  std::sort(wordTextVec.begin(), wordTextVec.end(),
            [](db::models::WordText wt1, db::models::WordText wt2) {
              return wt1.numOccurrences > wt2.numOccurrences;
            });

  for (db::models::WordText &wt : wordTextVec) {
    // TODO: Handle case of std::optional being nullopt - it is an error
    if (auto text = mAppContext->textRepository->getByHash(wt.textHash)) {
      mTextsVec.push_back(*text);
    }
  }

  for (db::models::Text t : mTextsVec) {
    auto textBlock = mTextsContainer->addNew<Wt::WContainerWidget>();
    textBlock->setStyleClass("source-block");

    std::string utf8Text = Wt::Utils::htmlEncode(db::Utils::toUtf8(t.text));
    std::string utf8Word = db::Utils::toUtf8(mSelectedWord);

    std::string highlighted = highlightWithBreakIterator(utf8Text, utf8Word);

    auto textWidget = textBlock->addNew<Wt::WText>(
        Wt::WString::fromUTF8(highlighted), Wt::TextFormat::XHTML);
    textWidget->setInline(false);
    textWidget->setStyleClass("source-text");
  }
}
