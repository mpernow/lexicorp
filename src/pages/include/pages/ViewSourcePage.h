#pragma once

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
  void addSourceBlock(const db::models::Text &text,
                      const std::string &textHtml);
  void confirmDelete(int hash, const std::string &source);
  // Removes the text and its word links, and takes its occurrences off the
  // word counts.
  void deleteText(int hash);

  std::shared_ptr<AppContext> mAppContext;
  processor::TextProcessor mTextProcessor;
  Wt::WText *mHeading;
  Wt::WContainerWidget *mTextsContainer;
};
