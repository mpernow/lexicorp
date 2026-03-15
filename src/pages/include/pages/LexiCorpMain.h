#pragma once

#include <Wt/WApplication.h>
#include <db/DatabaseHandler.h>
#include <db/WordRepository.h>
#include <pages/AppContext.h>
#include <utils/Language.h>

class LexiCorpMain : public Wt::WApplication {
public:
  LexiCorpMain(const Wt::WEnvironment &env);

private:
  void handlePath(const std::string &path);
  std::shared_ptr<AppContext> mAppContext;
};
