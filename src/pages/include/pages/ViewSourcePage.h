#pragma once

#include "db/Sentence.h"
#include "db/Text.h"
#include "pages/AppContext.h"
#include "processor/TextProcessor.h"
#include <Wt/WContainerWidget.h>
#include <Wt/WText.h>
#include <memory>
#include <vector>

class ViewSourcePage : public Wt::WContainerWidget {
public:
  ViewSourcePage(std::shared_ptr<AppContext> appContext);

private:
  // Shows the texts containing the selected word (internal path
  // "/source/word"), or every text for the selected language ("/source").
  void refresh();
  void showWordSources();
  void showAllSources();
  struct SourceCard {
    Wt::WContainerWidget *card;
    Wt::WContainerWidget *header;
  };
  // A card for a text, its header showing the source and a Delete button.
  SourceCard addSourceCard(const db::models::Text &text);
  void addSourceBlock(const db::models::Text &text,
                      const std::string &textHtml);
  // The text's sentences containing the word, with a toggle to the full text
  // (those sentences marked in it).
  void addSentencesBlock(const db::models::Text &text,
                         const std::vector<db::models::Sentence> &sentences,
                         const std::string &utf8Word);
  // The selected word's gloss, with an Edit button.
  void showGloss();
  // Asks for the word's own (manual) gloss and saves it.
  void editGloss(const std::wstring &word, const std::string &current);
  void confirmDelete(std::int64_t hash, const std::string &source);
  // Removes the text and its word links, and takes its occurrences off the
  // word counts.
  void deleteText(std::int64_t hash);

  std::shared_ptr<AppContext> mAppContext;
  processor::TextProcessor mTextProcessor;
  Wt::WText *mHeading;
  Wt::WContainerWidget *mGlossContainer;
  Wt::WContainerWidget *mTextsContainer;
};
