#pragma once
#include <memory>
#include <string>

#include "db/DatabaseHandler.h"
#include "db/WordRepository.h"
#include "utils/Language.h"

class AppContext {
public:
  AppContext() {
    dbHandler = std::make_shared<db::DatabaseHandler>();
    wordRepository =
        std::make_shared<db::WordRepository>(dbHandler->getConnection());
  }

  utils::Language selectedLanguage;

  std::shared_ptr<db::DatabaseHandler> dbHandler;
  std::shared_ptr<db::WordRepository> wordRepository;
};
