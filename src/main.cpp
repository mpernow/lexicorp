#include <Wt/WApplication.h>

#include "db/DatabaseHandler.h"
#include "pages/HelloApplication.h"

int main(int argc, char **argv) {
  db::DatabaseHandler dbHandler = db::DatabaseHandler{};
  return Wt::WRun(argc, argv, [](const Wt::WEnvironment &env) {
    return std::make_unique<HelloApplication>(env);
  });
}
