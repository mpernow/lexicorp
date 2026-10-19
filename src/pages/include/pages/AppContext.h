#pragma once
#include <memory>

#include "db/DatabaseHandler.h"
#include "db/DictionaryRepository.h"
#include "db/SentenceRepository.h"
#include "db/TextRepository.h"
#include "db/WordFormRepository.h"
#include "db/WordRepository.h"
#include "db/WordSentenceRepository.h"
#include "db/WordTextRepository.h"
#include "utils/Language.h"
#include <Wt/WSignal.h>

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
    sentenceRepository =
        std::make_shared<db::SentenceRepository>(dbHandler->getConnection());
    wordSentenceRepository = std::make_shared<db::WordSentenceRepository>(
        dbHandler->getConnection());
    dictionaryRepository =
        std::make_shared<db::DictionaryRepository>(dbHandler->getConnection());
    wordFormRepository =
        std::make_shared<db::WordFormRepository>(dbHandler->getConnection());
  }

  // Matches the first entry of the sidebar language combo box.
  utils::Language selectedLanguage = utils::Language::English;
  Wt::Signal<std::string> languageChanged;
  Wt::Signal<> wordSelected;
  std::wstring selectedWord;

  std::shared_ptr<db::DatabaseHandler> dbHandler;
  std::shared_ptr<db::WordRepository> wordRepository;
  std::shared_ptr<db::TextRepository> textRepository;
  std::shared_ptr<db::WordTextRepository> wordTextRepository;
  std::shared_ptr<db::SentenceRepository> sentenceRepository;
  std::shared_ptr<db::WordSentenceRepository> wordSentenceRepository;
  std::shared_ptr<db::DictionaryRepository> dictionaryRepository;
  std::shared_ptr<db::WordFormRepository> wordFormRepository;
};
