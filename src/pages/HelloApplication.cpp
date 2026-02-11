#include <Wt/WBreak.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>

#include "pages/HelloApplication.h"

HelloApplication::HelloApplication(const Wt::WEnvironment &env)
    : Wt::WApplication(env) {

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
    greeting_->setText("Hello there, " + textEdit_->text());
  };
  button->clicked().connect(greet);
}
