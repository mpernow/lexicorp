#include <Wt/WApplication.h>

#include "db/DatabaseHandler.h"
#include "db/WordRepository.h"
#include "pages/InsertTextPage.h"

int main(int argc, char **argv) {
  db::DatabaseHandler dbHandler = db::DatabaseHandler{};
  db::WordRepository wordRepository =
      db::WordRepository(dbHandler.getConnection());
  return Wt::WRun(argc, argv, [&wordRepository](const Wt::WEnvironment &env) {
    return std::make_unique<InsertTextPage>(env, wordRepository);
  });
}
