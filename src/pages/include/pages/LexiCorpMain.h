#pragma once

#include <Wt/WApplication.h>
#include <db/WordRepository.h>

class LexiCorpMain : public Wt::WApplication {
public:
  LexiCorpMain(const Wt::WEnvironment &env);

private:
  void handlePath(const std::string &path);
  std::unique_ptr<db::WordRepository> mWordRepository;
};
