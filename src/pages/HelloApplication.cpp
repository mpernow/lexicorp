#include <Wt/WBreak.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WPushButton.h>

#include "pages/HelloApplication.h"

HelloApplication::HelloApplication(const Wt::WEnvironment &env)
    : Wt::WApplication(env) {
  setTitle("Hello world");

  root()->addNew<Wt::WText>("Your name, please? ");
  textEdit_ = root()->addNew<Wt::WTextArea>();
  Wt::WPushButton *button = root()->addNew<Wt::WPushButton>("Greet me.");
  root()->addNew<Wt::WBreak>();
  greeting_ = root()->addNew<Wt::WText>();
  auto greet = [this] {
    greeting_->setText("Hello there, " + textEdit_->text());
  };
  button->clicked().connect(greet);
}
