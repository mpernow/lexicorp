#ifndef WORD_H
#define WORD_H

#include <string>
#include <vector>

namespace db::models {
struct Word {
  std::string word;
  int numOccurences = 0;
  bool known = false;

  static constexpr const std::string tableName = "Word";

  static std::vector<std::pair<std::string, std::string>> schema() {
    return {{"word", "TEXT PRIMARY KEY"},
            {"numOccurences", "INTEGER"},
            {"known", "INTEGER"}};
  }
};
} // namespace db::models

#endif // WORD_H
