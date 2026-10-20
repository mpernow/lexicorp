#pragma once
#include <memory>

#include "claude/ClaudeClient.h"
#include "db/ClaudeUsageRepository.h"
#include "db/DatabaseHandler.h"
#include "db/DictionaryRepository.h"
#include "db/ExpressionWordRepository.h"
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
    expressionWordRepository = std::make_shared<db::ExpressionWordRepository>(
        dbHandler->getConnection());
    claudeUsageRepository =
        std::make_shared<db::ClaudeUsageRepository>(dbHandler->getConnection());
  }

  // The client every session uses, set by main() when an API key is
  // configured (and by tests); null turns the Claude features off.
  static inline std::shared_ptr<claude::IClaudeClient> sharedClaudeClient;

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
  std::shared_ptr<db::ExpressionWordRepository> expressionWordRepository;
  std::shared_ptr<db::ClaudeUsageRepository> claudeUsageRepository;
  std::shared_ptr<claude::IClaudeClient> claudeClient = sharedClaudeClient;
};
