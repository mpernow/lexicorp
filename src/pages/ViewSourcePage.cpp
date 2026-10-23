#include "pages/ViewSourcePage.h"
#include "pages/Background.h"
#include "pages/Bidi.h"
#include "pages/ClaudeLookup.h"
#include "pages/ConfirmDialog.h"
#include "pages/EmptyState.h"
#include "pages/Explanations.h"
#include "pages/Glosses.h"
#include "db/Savepoint.h"
#include "db/Sentence.h"
#include "db/Text.h"
#include "db/Utils.h"
#include "db/WordText.h"
#include <Wt/Core/observing_ptr.hpp>
#include <Wt/WApplication.h>
#include <Wt/WBreak.h>
#include <Wt/WDialog.h>
#include <Wt/WLineEdit.h>
#include <Wt/WPushButton.h>
#include <Wt/WText.h>
#include <algorithm>
#include <cwctype>
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

  mGlossContainer = container->addNew<Wt::WContainerWidget>();

  mTextsContainer =
      container->addWidget(std::make_unique<Wt::WContainerWidget>());

  refresh();

  Wt::WApplication::instance()->internalPathChanged().connect(
      [this](const std::string &path) {
        if (path.starts_with("/source"))
          refresh();
      });
  mAppContext->languageChanged.connect([this] { refresh(); });
  // The Explain buttons name the model.
  mAppContext->explainModelChanged.connect([this] {
    if (Wt::WApplication::instance()->internalPath().starts_with("/source"))
      refresh();
  });
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
  mGlossContainer->clear();
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
                                          isolate(utf8Word) + "\" (" +
                                          std::to_string(shown) + ")"));
  mHeading->setTextFormat(Wt::TextFormat::Plain);

  auto expressions = expressionsOf(*mAppContext, selectedWord);
  showGloss(expressions);

  if (groups.empty()) {
    mTextsContainer->addWidget(makeEmptyState(
        "No stored sentence contains \"" + isolate(utf8Word) + "\".",
        false));
  }

  for (const auto &[text, textSentences] : groups)
    addSentencesBlock(text, textSentences, utf8Word, expressions);
}

namespace {
std::string sourceLabel(const Gloss &gloss) {
  std::string label;
  switch (gloss.entries.front().source) {
  case db::models::DictionarySource::Manual:
    label = "Your gloss";
    break;
  case db::models::DictionarySource::Wiktionary:
    label = "Wiktionary";
    break;
  case db::models::DictionarySource::Claude:
    label = "Claude (AI-generated)";
    break;
  case db::models::DictionarySource::Google:
    label = "Google Translate";
    break;
  }
  if (!gloss.lemma.empty())
    label = "Form of " + db::Utils::toUtf8(gloss.lemma) + " · " + label;
  return label;
}

// "<expression>: <gloss>", the expression in the sentences' direction.
void addExpressionLine(Wt::WContainerWidget *parent,
                       const Expression &expression, const std::string &style) {
  auto line = parent->addNew<Wt::WContainerWidget>();
  line->setStyleClass(style);
  auto words = line->addNew<Wt::WText>(
      Wt::WString::fromUTF8(db::Utils::toUtf8(expression.expression)),
      Wt::TextFormat::Plain);
  words->setAttributeValue("dir", "auto");
  words->setStyleClass("fw-semibold me-2");
  line->addNew<Wt::WText>(
      Wt::WString::fromUTF8(glossSummary(expression.gloss)),
      Wt::TextFormat::Plain);
}
} // namespace

void ViewSourcePage::showGloss(const std::vector<Expression> &expressions) {
  const std::wstring word = mAppContext->selectedWord;
  auto gloss = glossOf(*mAppContext, word);

  auto card = mGlossContainer->addNew<Wt::WContainerWidget>();
  card->setStyleClass("card mb-3 gloss-card");
  auto body = card->addNew<Wt::WContainerWidget>();
  body->setStyleClass("card-body d-flex align-items-start gap-2");
  auto lines = body->addNew<Wt::WContainerWidget>();
  lines->setStyleClass("flex-grow-1");

  // The text the editor starts with: the word's own gloss, if it has one.
  std::string current;
  if (!gloss) {
    auto none = lines->addNew<Wt::WText>("No gloss yet.");
    none->setStyleClass("text-body-secondary");
  } else {
    if (gloss->lemma.empty() &&
        gloss->entries.front().source == db::models::DictionarySource::Manual)
      current = glossSummary(*gloss);

    std::vector<std::wstring> shown;
    for (const auto &entry : gloss->entries) {
      if (std::ranges::find(shown, entry.gloss) != shown.end())
        continue;
      shown.push_back(entry.gloss);

      auto line = lines->addNew<Wt::WContainerWidget>();
      line->setStyleClass("gloss-line");
      if (!entry.partOfSpeech.empty()) {
        line->addNew<Wt::WText>(
                Wt::WString::fromUTF8(db::Utils::toUtf8(entry.partOfSpeech)),
                Wt::TextFormat::Plain)
            ->setStyleClass("gloss-pos text-body-secondary fst-italic me-2");
      }
      line->addNew<Wt::WText>(
          Wt::WString::fromUTF8(db::Utils::toUtf8(entry.gloss)),
          Wt::TextFormat::Plain);
    }

    auto source = lines->addNew<Wt::WText>(
        Wt::WString::fromUTF8(sourceLabel(*gloss)), Wt::TextFormat::Plain);
    source->setInline(false);
    source->setStyleClass("gloss-source small text-body-secondary mt-1");
  }

  if (!expressions.empty()) {
    auto heading = lines->addNew<Wt::WText>("Part of");
    heading->setInline(false);
    heading->setStyleClass("small text-body-secondary mt-2");
    for (const auto &expression : expressions)
      addExpressionLine(lines, expression, "gloss-line");
  }

  if (mLookupWord == word && !mLookupMessage.empty()) {
    auto message = lines->addNew<Wt::WText>(
        Wt::WString::fromUTF8(mLookupMessage), Wt::TextFormat::Plain);
    message->setInline(false);
    message->setStyleClass(mLookupFailed ? "small text-danger mt-1"
                                         : "small text-body-secondary mt-1");
  }
  mLookupMessage.clear();

  auto buttons = body->addNew<Wt::WContainerWidget>();
  buttons->setStyleClass("d-flex flex-column gap-2 flex-shrink-0");
  if (!gloss && mAppContext->claudeClient &&
      mAppContext->selectedLanguage == kClaudeLanguage) {
    auto askButton = buttons->addNew<Wt::WPushButton>("Ask Claude");
    askButton->setStyleClass("btn-sm btn-outline-primary busy-button");
    askButton->setToolTip("Look up this word with Claude (AI-generated, "
                          "a paid API request)");
    // Stays disabled, with a spinner, until the answer arrives and the page
    // is redrawn.
    askButton->clicked().connect(askButton, &Wt::WWidget::disable);
    askButton->clicked().connect([this, word] { askClaude(word); });
  }
  auto editButton = buttons->addNew<Wt::WPushButton>("Edit gloss");
  editButton->setStyleClass("btn-sm btn-outline-secondary");
  editButton->clicked().connect(
      [this, word, current] { editGloss(word, current); });
}

void ViewSourcePage::askClaude(const std::wstring &word) {
  utils::Language language = mAppContext->selectedLanguage;
  auto query = wordQuery(*mAppContext, word, language);
  auto client = mAppContext->claudeClient;

  runInBackground<claude::WordLookup>(
      [client, query] { return client->defineWord(query); },
      [this, language, query](claude::WordLookup lookup) {
        saveLookup(*mAppContext, language, query, lookup);

        mLookupWord = query.word;
        mLookupFailed = !lookup.error.empty();
        if (mLookupFailed)
          mLookupMessage = lookup.error + ".";
        else if (lookup.senses.empty())
          mLookupMessage = "Claude doesn't know this word.";
        if (mAppContext->selectedWord == query.word)
          refresh();
      });
}

void ViewSourcePage::editGloss(const std::wstring &word,
                               const std::string &current) {
  auto dialog = addChild(std::make_unique<Wt::WDialog>(Wt::WString::fromUTF8(
      "Gloss for \"" + isolate(db::Utils::toUtf8(word)) + "\"")));
  dialog->setClosable(true);
  dialog->rejectWhenEscapePressed();

  auto edit = dialog->contents()->addNew<Wt::WLineEdit>(
      Wt::WString::fromUTF8(current));
  edit->setPlaceholderText("Leave empty to use the dictionary's gloss");
  edit->setStyleClass("form-control");
  edit->setFocus();

  auto cancel = dialog->footer()->addNew<Wt::WPushButton>("Cancel");
  cancel->setStyleClass("btn-secondary");
  cancel->clicked().connect(dialog, &Wt::WDialog::reject);
  auto save = dialog->footer()->addNew<Wt::WPushButton>("Save");
  save->setStyleClass("btn-primary");
  save->clicked().connect(dialog, &Wt::WDialog::accept);
  edit->enterPressed().connect(dialog, &Wt::WDialog::accept);

  dialog->finished().connect([this, dialog, edit, word](Wt::DialogCode code) {
    std::wstring gloss = db::Utils::fromUtf8(edit->text().toUTF8());
    // Destroyed at the end of this handler, not during it.
    auto removed = removeChild(dialog);
    if (code != Wt::DialogCode::Accepted)
      return;

    // Trimmed, so a gloss of only spaces removes it.
    auto isSpace = [](wchar_t c) { return std::iswspace(c); };
    gloss.erase(gloss.begin(), std::ranges::find_if_not(gloss, isSpace));
    gloss.erase(std::ranges::find_if_not(gloss.rbegin(), gloss.rend(), isSpace)
                    .base(),
                gloss.end());
    mAppContext->dictionaryRepository->setManualGloss(
        word, mAppContext->selectedLanguage, gloss);
    refresh();
  });
  dialog->show();
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
      Wt::WString::fromUTF8("Source: " + isolate(source)),
      Wt::TextFormat::Plain);
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
    const std::string &utf8Word, const std::vector<Expression> &expressions) {
  auto [card, header] = addSourceCard(text);
  utils::Language language = mAppContext->selectedLanguage;

  auto client = mAppContext->claudeClient;
  bool explain =
      client && client->canExplain() && language == kClaudeLanguage;

  auto sentenceList = card->addNew<Wt::WContainerWidget>();
  sentenceList->setStyleClass("card-body");
  for (const auto &sentence : sentences) {
    auto row = sentenceList->addNew<Wt::WContainerWidget>();
    row->setStyleClass("sentence-row");
    auto sentenceWidget = row->addNew<Wt::WText>(
        Wt::WString::fromUTF8(highlightWithBreakIterator(
            toUnicode(sentence.sentence), utf8Word, mTextProcessor, language)),
        Wt::TextFormat::XHTML);
    sentenceWidget->setInline(false);
    sentenceWidget->setStyleClass("source-sentence");
    sentenceWidget->setAttributeValue("dir", "auto");

    if (!expressions.empty()) {
      auto words = mTextProcessor.words(sentence.sentence, language);
      for (const auto &expression : expressions)
        if (containsExpression(words, mAppContext->selectedWord, expression))
          addExpressionLine(row, expression,
                            "sentence-expression small text-body-secondary");
    }
    if (explain)
      addExplanation(row, sentence);
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

void ViewSourcePage::addExplanation(Wt::WContainerWidget *row,
                                    const db::models::Sentence &sentence) {
  auto hash = sentenceHash(mTextProcessor, sentence.sentence);
  // An unreadable or empty stored answer counts as none: explaining again
  // replaces it.
  auto storedAnswer = mAppContext->explanationRepository->get(hash);
  bool stored = storedAnswer &&
                claude::explanationFromJson(storedAnswer->answerJson)
                    .error.empty();
  auto current = mAppContext->currentExplainModel();

  auto button =
      row->addNew<Wt::WPushButton>(stored ? "Show explanation" : "Explain");
  button->setStyleClass("btn-sm btn-link p-0 explain-button busy-button");
  if (!stored)
    button->setToolTip("Explain this sentence with Claude " +
                       claude::modelLabel(current) +
                       " (AI-generated, a paid API request)");
  auto error = row->addNew<Wt::WText>();
  error->setTextFormat(Wt::TextFormat::Plain);
  error->setInline(false);
  error->setStyleClass("small text-danger");
  error->hide();
  auto panel = row->addNew<Wt::WContainerWidget>();
  panel->hide();
  // Under an explanation from another model than the sidebar's. Outside the
  // panel, which is cleared when the new answer arrives.
  auto again = row->addNew<Wt::WPushButton>("Explain again with " +
                                            claude::modelLabel(current));
  again->setStyleClass("btn-sm btn-link p-0 explain-button busy-button");
  again->setToolTip("Replaces this explanation (a paid API request)");
  again->hide();
  // The model of the explanation in the panel.
  auto shownModel = std::make_shared<std::string>();

  auto show = [this, button, panel, again, shownModel, current](
                  const claude::SentenceExplanation &explanation,
                  const std::string &model) {
    panel->clear();
    panel->addWidget(explanationWidget(explanation, model,
                                       mAppContext->claudeClient->grammar()));
    panel->show();
    *shownModel = model;
    again->setHidden(model == current);
    button->setText("Hide explanation");
    button->setToolTip("");
  };

  auto ask = [this, sentence, hash, button, error, panel, again, show,
              current] {
    auto query = sentenceQuery(*mAppContext, sentence);
    query.model = current;
    auto client = mAppContext->claudeClient;
    // The page may be redrawn before the answer arrives.
    Wt::Core::observing_ptr<Wt::WPushButton> buttonPtr = button;
    Wt::Core::observing_ptr<Wt::WPushButton> againPtr = again;
    Wt::Core::observing_ptr<Wt::WText> errorPtr = error;
    Wt::Core::observing_ptr<Wt::WContainerWidget> panelPtr = panel;
    runInBackground<claude::SentenceExplanation>(
        [client, query] { return client->explainSentence(query); },
        [this, query, hash, buttonPtr, againPtr, errorPtr, panelPtr,
         show](claude::SentenceExplanation explanation) {
          saveExplanation(*mAppContext, hash, query, explanation);
          if (!buttonPtr || !againPtr || !errorPtr || !panelPtr)
            return;
          buttonPtr->enable();
          againPtr->enable();
          if (!explanation.error.empty()) {
            errorPtr->setText(Wt::WString::fromUTF8(explanation.error + "."));
            errorPtr->show();
            return;
          }
          show(explanation,
               explanation.usage ? explanation.usage->model : query.model);
        });
  };

  // Disabled, with a spinner, as soon as it is clicked: re-enabled at once
  // unless Claude is asked, then when the answer arrives.
  button->clicked().connect(button, &Wt::WWidget::disable);
  button->clicked().connect(
      [this, button, error, panel, again, hash, show, ask, shownModel,
       current] {
        error->hide();
        if (panel->count() > 0) {
          button->enable();
          panel->setHidden(!panel->isHidden());
          again->setHidden(panel->isHidden() || *shownModel == current);
          button->setText(panel->isHidden() ? "Show explanation"
                                            : "Hide explanation");
          return;
        }
        // Possibly explained since the page was drawn, e.g. in another text.
        if (auto stored = mAppContext->explanationRepository->get(hash)) {
          auto explanation = claude::explanationFromJson(stored->answerJson);
          if (explanation.error.empty()) {
            button->enable();
            show(explanation, stored->model);
            return;
          }
        }
        ask();
      });
  again->clicked().connect(again, &Wt::WWidget::disable);
  again->clicked().connect([error, ask] {
    error->hide();
    ask();
  });
}

void ViewSourcePage::confirmDelete(std::int64_t hash,
                                   const std::string &source) {
  confirmDestructive(
      this, "Delete text",
      (source.empty()
           ? "Delete this text?"
           : "Delete the text from \"" + isolate(source) + "\"?") +
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
