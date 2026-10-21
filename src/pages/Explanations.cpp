#include "pages/Explanations.h"
#include "pages/Bidi.h"

#include "db/Savepoint.h"
#include "db/Utils.h"

#include <Wt/WText.h>

#include <algorithm>

namespace {
std::string utf8(const std::wstring &text) { return db::Utils::toUtf8(text); }

Wt::WText *addLine(Wt::WContainerWidget *parent, const std::string &text,
                   const std::string &style) {
  auto line = parent->addNew<Wt::WText>(Wt::WString::fromUTF8(text),
                                        Wt::TextFormat::Plain);
  line->setInline(false);
  line->setStyleClass(style);
  return line;
}

// " (a.b, c.d)", or nothing without sections.
std::string sectionsSuffix(const std::vector<std::string> &sections) {
  if (sections.empty())
    return "";
  std::string text = " (";
  for (std::size_t i = 0; i < sections.size(); ++i)
    text += (i ? ", " : "") + sections[i];
  return text + ")";
}

// A line of English (any Kurdish in it isolated), then the sections it
// cites in small type.
void addCitedLine(Wt::WContainerWidget *parent, const std::string &text,
                  const std::vector<std::string> &sections) {
  auto line = parent->addNew<Wt::WContainerWidget>();
  line->setStyleClass("explanation-line");
  line->setAttributeValue("dir", "ltr");
  line->addNew<Wt::WText>(Wt::WString::fromUTF8(text), Wt::TextFormat::Plain);
  if (!sections.empty())
    line->addNew<Wt::WText>(Wt::WString::fromUTF8(sectionsSuffix(sections)),
                            Wt::TextFormat::Plain)
        ->setStyleClass("explanation-sections text-body-secondary");
}

// A morpheme as written alone: an affix gets a tatweel on the side it joins
// (دەـ, ـەکە), as a hyphen would in Latin script; otherwise a suffix like ەکە
// looks like a word of its own.
std::wstring affixForm(const claude::Morpheme &morpheme) {
  std::wstring form = morpheme.form;
  std::erase(form, L'-');
  if (form.empty() || morpheme.kind == L"stem" || morpheme.kind == L"particle")
    return form;
  if (morpheme.kind == L"prefix")
    return form + L"ـ";
  return L"ـ" + form; // suffix, clitic, ezafe
}

// "کتێب (book) + ـەکە (definite) + ـم (1sg agent)": the parts in order, each
// with its function (or its kind, if Claude gave no function).
std::string partsOf(const claude::WordAnalysis &word) {
  std::string parts;
  for (const auto &morpheme : word.morphemes) {
    if (!parts.empty())
      parts += " + ";
    const auto &function =
        morpheme.function.empty() ? morpheme.kind : morpheme.function;
    parts += isolate(utf8(affixForm(morpheme))) + " (" + utf8(function) + ")";
  }
  return parts;
}
} // namespace

claude::SentenceQuery sentenceQuery(AppContext &appContext,
                                    const db::models::Sentence &sentence) {
  claude::SentenceQuery query{sentence.sentence, L"", L""};
  for (const auto &other :
       appContext.sentenceRepository->getByHash(sentence.textHash)) {
    if (other.ordinal == sentence.ordinal - 1)
      query.before = other.sentence;
    else if (other.ordinal == sentence.ordinal + 1)
      query.after = other.sentence;
  }
  return query;
}

std::int64_t sentenceHash(processor::TextProcessor &processor,
                          const std::wstring &sentence) {
  return static_cast<std::int64_t>(processor.hashWstring(sentence));
}

void saveExplanation(AppContext &appContext, std::int64_t hash,
                     const claude::SentenceQuery &query,
                     const claude::SentenceExplanation &explanation) {
  db::Savepoint tx{appContext.dbHandler->getConnection(), "save_explanation"};

  if (explanation.usage)
    appContext.claudeUsageRepository->add(db::models::ClaudeUsage{
        .model = explanation.usage->model,
        .purpose = "explain_sentence",
        .inputTokens = explanation.usage->inputTokens,
        .outputTokens = explanation.usage->outputTokens,
        .cacheReadTokens = explanation.usage->cacheReadTokens,
        .cacheCreationTokens = explanation.usage->cacheCreationTokens,
    });

  if (explanation.error.empty())
    appContext.explanationRepository->save(db::models::Explanation{
        .sentenceHash = hash,
        .sentence = query.sentence,
        .answerJson = explanation.answerJson,
        .model = explanation.usage ? explanation.usage->model : "",
    });
  tx.commit();
}

std::unique_ptr<Wt::WContainerWidget>
explanationWidget(const claude::SentenceExplanation &explanation,
                  const std::string &model) {
  auto panel = std::make_unique<Wt::WContainerWidget>();
  panel->setStyleClass("explanation");

  if (!explanation.translation.empty())
    addLine(panel.get(), utf8(explanation.translation),
            "explanation-translation");

  // One column per word, in the sentence's direction: the word, its Leipzig
  // gloss, and its base form with that form's meaning.
  auto words = panel->addNew<Wt::WContainerWidget>();
  words->setStyleClass("interlinear");
  words->setAttributeValue("dir", "rtl");
  for (const auto &word : explanation.words) {
    auto column = words->addNew<Wt::WContainerWidget>();
    column->setStyleClass("interlinear-word");
    addLine(column, utf8(word.surface), "interlinear-surface")
        ->setAttributeValue("dir", "auto");
    addLine(column, utf8(word.leipzig), "interlinear-leipzig")
        ->setAttributeValue("dir", "ltr");
    std::string base = isolate(utf8(word.baseForm));
    if (!word.baseGloss.empty())
      base += " ‘" + utf8(word.baseGloss) + "’";
    addLine(column, base, "interlinear-base text-body-secondary")
        ->setAttributeValue("dir", "ltr");
  }

  auto section = [&](const std::string &heading) {
    addLine(panel.get(), heading, "explanation-heading text-body-secondary");
  };

  if (std::ranges::any_of(explanation.words, [](const auto &word) {
        return word.morphemes.size() > 1;
      })) {
    section("Parts");
    for (const auto &word : explanation.words)
      if (word.morphemes.size() > 1)
        addCitedLine(panel.get(),
                     isolate(utf8(word.surface)) + ": " + partsOf(word), {});
  }

  if (!explanation.constructions.empty()) {
    section("Constructions");
    for (const auto &construction : explanation.constructions) {
      std::string words;
      for (const auto &word : construction.words)
        words += (words.empty() ? "" : " … ") + utf8(word);
      std::string text = utf8(construction.name);
      if (!words.empty())
        text += " " + isolate(words);
      text += ": " + utf8(construction.explanation);
      addCitedLine(panel.get(), text, construction.sections);
    }
  }

  if (!explanation.notes.empty()) {
    section("Notes");
    for (const auto &note : explanation.notes)
      addCitedLine(panel.get(), utf8(note.text), note.sections);
  }

  addLine(panel.get(),
          "Claude (AI-generated)" + (model.empty() ? "" : " · " + model),
          "explanation-source small text-body-secondary");
  return panel;
}
