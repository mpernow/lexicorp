#include <Wt/WBreak.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>
#include <unordered_map>

#include "pages/HelloApplication.h"

HelloApplication::HelloApplication(const Wt::WEnvironment &env,
                                   db::WordRepository &wordRepo)
    : Wt::WApplication(env) {
  mTextProcessor = std::make_unique<processor::TextProcessor>();

  useStyleSheet("resources/style.css");

  auto container = root()->addNew<Wt::WContainerWidget>();
  container->setStyleClass("main-container");

  setTitle("Hello world");

  container->addNew<Wt::WText>("Your name, please? ");
  textEdit_ = container->addNew<Wt::WTextArea>();
  textEdit_->setStyleClass("textarea");

  Wt::WPushButton *button = container->addNew<Wt::WPushButton>("Greet me.");
  button->setStyleClass("button");

  container->addNew<Wt::WBreak>();

  greeting_ = container->addNew<Wt::WText>();
  greeting_->setStyleClass("greeting");
  auto greet = [this] {
    std::unordered_map<std::string, int> wordCounts =
        mTextProcessor->computeFrequencies(textEdit_->text().narrow());

    std::string outputText = "";
    for (const auto &[word, count] : wordCounts) {
      outputText += word + ": " + std::to_string(count) + "\n";
    }
    greeting_->setText(outputText);
  };
  button->clicked().connect(greet);
}
