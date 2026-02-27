#pragma once

#include "db/WordRepository.h"
#include "processor/TextProcessor.h"
#include <Wt/WApplication.h>
#include <Wt/WText.h>
#include <Wt/WTextArea.h>

class HelloApplication : public Wt::WApplication {
public:
  HelloApplication(const Wt::WEnvironment &env, db::WordRepository &wordRepo);

private:
  Wt::WTextArea *textEdit_;
  Wt::WText *greeting_;
  std::unique_ptr<processor::TextProcessor> mTextProcessor;
};
