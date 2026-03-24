#pragma once
#include <memory>

#include "db/DatabaseHandler.h"
#include "db/TextRepository.h"
#include "db/WordRepository.h"
#include "db/WordTextRepository.h"
#include "utils/Language.h"

class AppContext {
public:
  AppContext() {
    dbHandler = std::make_shared<db::DatabaseHandler>();
    wordRepository =
        std::make_shared<db::WordRepository>(dbHandler->getConnection());
    textRepository =
        std::make_shared<db::TextRepository>(dbHandler->getConnection());
    wordTextRepository =
        std::make_shared<db::WordTextRepository>(dbHandler->getConnection());
  }

  utils::Language selectedLanguage;

  std::shared_ptr<db::DatabaseHandler> dbHandler;
  std::shared_ptr<db::WordRepository> wordRepository;
  std::shared_ptr<db::TextRepository> textRepository;
  std::shared_ptr<db::WordTextRepository> wordTextRepository;
};
