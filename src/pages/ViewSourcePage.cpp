#include "pages/ViewSourcePage.h"
#include "pages/ConfirmDialog.h"
#include "pages/EmptyState.h"
#include "db/Savepoint.h"
#include "db/Sentence.h"
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

// Escaped HTML of `text`, with `utf8Word` (a stored word) highlighted. Tokens
// of the text get the stored words' letter normalisation before comparing.
std::string highlightWithBreakIterator(const icu::UnicodeString &text,
                                       const std::string &utf8Word,
                                       const processor::TextProcessor &letters,
                                       utils::Language language) {
  UErrorCode status = U_ZERO_ERROR;

  icu::UnicodeString word = icu::UnicodeString::fromUTF8(utf8Word);
  word.foldCase();

  std::unique_ptr<icu::BreakIterator> it(icu::BreakIterator::createWordInstance(
      processor::TextProcessor::locale(language), status));

  it->setText(text);

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

namespace {
icu::UnicodeString toUnicode(const std::wstring &text) {
  return icu::UnicodeString::fromUTF8(db::Utils::toUtf8(text));
}

// Escaped HTML of a whole text, with `sentences` (the text's, in order) marked
// and `utf8Word` highlighted.
std::string
highlightSentences(const std::wstring &fullText,
                   const std::vector<db::models::Sentence> &sentences,
                   const std::string &utf8Word,
                   const processor::TextProcessor &letters,
                   utils::Language language) {
  icu::UnicodeString text = toUnicode(fullText);
  auto highlight = [&](int32_t from, int32_t to) {
    return highlightWithBreakIterator(text.tempSubStringBetween(from, to),
                                      utf8Word, letters, language);
  };

  std::string html;
  int32_t done = 0;
  for (const auto &sentence : sentences) {
    // Positions are stored in code points; the text is indexed in UTF-16.
    int32_t start = text.moveIndex32(0, sentence.start);
    int32_t end = text.moveIndex32(start, sentence.length);
    if (start < done)
      continue;
    html += highlight(done, start);
    html += "<span class=\"highlight-sentence\">" + highlight(start, end) +
            "</span>";
    done = end;
  }
  html += highlight(done, text.length());
  return html;
}
} // namespace

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

  auto sentences = mAppContext->sentenceRepository->getByWord(
      selectedWord, mAppContext->selectedLanguage);

  // Grouped by text; the sentences come ordered by text, then position.
  std::vector<std::pair<db::models::Text, std::vector<db::models::Sentence>>>
      groups;
  int shown = 0;
  for (const auto &sentence : sentences) {
    if (groups.empty() || groups.back().first.hash != sentence.textHash) {
      auto text = mAppContext->textRepository->getByHash(sentence.textHash);
      if (!text)
        continue;
      groups.emplace_back(std::move(*text),
                          std::vector<db::models::Sentence>{});
    }
    groups.back().second.push_back(sentence);
    ++shown;
  }
  std::stable_sort(groups.begin(), groups.end(),
                   [](const auto &a, const auto &b) {
                     return a.first.source < b.first.source;
                   });

  mHeading->setText(Wt::WString::fromUTF8("Sentences containing \"" +
                                          utf8Word + "\" (" +
                                          std::to_string(shown) + ")"));
  mHeading->setTextFormat(Wt::TextFormat::Plain);

  if (groups.empty()) {
    mTextsContainer->addWidget(makeEmptyState(
        "No stored sentence contains \"" + utf8Word + "\".", false));
  }

  for (const auto &[text, textSentences] : groups)
    addSentencesBlock(text, textSentences, utf8Word);
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

ViewSourcePage::SourceCard
ViewSourcePage::addSourceCard(const db::models::Text &text) {
  auto card = mTextsContainer->addNew<Wt::WContainerWidget>();
  card->setStyleClass("card mb-3");

  auto header = card->addNew<Wt::WContainerWidget>();
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

  return {card, header};
}

void ViewSourcePage::addSourceBlock(const db::models::Text &text,
                                    const std::string &textHtml) {
  auto textWidget = addSourceCard(text).card->addNew<Wt::WText>(
      Wt::WString::fromUTF8(textHtml), Wt::TextFormat::XHTML);
  textWidget->setInline(false);
  textWidget->setStyleClass("card-body source-text");
  textWidget->setAttributeValue("dir", "auto");
}

void ViewSourcePage::addSentencesBlock(
    const db::models::Text &text,
    const std::vector<db::models::Sentence> &sentences,
    const std::string &utf8Word) {
  auto [card, header] = addSourceCard(text);
  utils::Language language = mAppContext->selectedLanguage;

  auto sentenceList = card->addNew<Wt::WContainerWidget>();
  sentenceList->setStyleClass("card-body");
  for (const auto &sentence : sentences) {
    auto sentenceWidget = sentenceList->addNew<Wt::WText>(
        Wt::WString::fromUTF8(highlightWithBreakIterator(
            toUnicode(sentence.sentence), utf8Word, mTextProcessor, language)),
        Wt::TextFormat::XHTML);
    sentenceWidget->setInline(false);
    sentenceWidget->setStyleClass("source-sentence");
    sentenceWidget->setAttributeValue("dir", "auto");
  }

  // Filled in when first shown.
  auto fullText = card->addNew<Wt::WText>();
  fullText->setTextFormat(Wt::TextFormat::XHTML);
  fullText->setInline(false);
  fullText->setStyleClass("card-body source-text");
  fullText->setAttributeValue("dir", "auto");
  fullText->hide();

  // Between the source and the Delete button.
  auto toggle = std::make_unique<Wt::WPushButton>("Show full text");
  toggle->setStyleClass("btn-sm btn-outline-secondary flex-shrink-0");
  Wt::WPushButton *togglePtr = toggle.get();
  header->insertWidget(header->count() - 1, std::move(toggle));

  togglePtr->clicked().connect([this, togglePtr, sentenceList, fullText,
                                fullTextSource = text.text, sentences,
                                utf8Word, language] {
    bool showFull = fullText->isHidden();
    if (showFull && fullText->text().empty())
      fullText->setText(Wt::WString::fromUTF8(highlightSentences(
          fullTextSource, sentences, utf8Word, mTextProcessor, language)));
    fullText->setHidden(!showFull);
    sentenceList->setHidden(showFull);
    togglePtr->setText(showFull ? "Show sentences" : "Show full text");
  });
}

void ViewSourcePage::confirmDelete(std::int64_t hash,
                                   const std::string &source) {
  confirmDestructive(
      this, "Delete text",
      (source.empty() ? "Delete this text?"
                      : "Delete the text from \"" + source + "\"?") +
          " Its words' counts are lowered by their occurrences in it, and "
          "words that only appeared in this text are removed unless marked "
          "known.",
      "Delete", [this, hash] { deleteText(hash); });
}

void ViewSourcePage::deleteText(std::int64_t hash) {
  db::Savepoint tx{mAppContext->dbHandler->getConnection(), "delete_text"};

  std::vector<db::models::Word> occurrences;
  for (const auto &wt : mAppContext->wordTextRepository->getAllByHash(hash))
    occurrences.emplace_back(wt.word, wt.numOccurrences, false, wt.language);
  mAppContext->wordRepository->subtractFrequencies(occurrences);
  mAppContext->wordTextRepository->removeByHash(hash);
  mAppContext->wordSentenceRepository->removeByHash(hash);
  mAppContext->sentenceRepository->removeByHash(hash);
  mAppContext->textRepository->remove(hash);
  tx.commit();

  refresh();
}
