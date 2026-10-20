#include <Wt/WApplication.h>

#include "pages/AppContext.h"
#include "pages/LexiCorpMain.h"
#include "pages/Migration.h"

int main(int argc, char **argv) {
  {
    AppContext appContext;
    migrateDatabase(appContext);
  }
  AppContext::sharedClaudeClient = claude::ClaudeClient::fromEnvironment();

  return Wt::WRun(argc, argv, [](const Wt::WEnvironment &env) {
    return std::make_unique<LexiCorpMain>(env);
  });
}
