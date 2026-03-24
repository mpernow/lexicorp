#pragma once
#include "sqlite/connection.hpp"

#include "db/IWordRepository.h"
#include "db/Word.h"

namespace db {
class WordRepository : public IWordRepository {
public:
  WordRepository(sqlite::connection &dbConn);
  ~WordRepository();

  void add(const models::Word &word) override;
  std::optional<models::Word>
  getByText(const std::wstring &text, const utils::Language &language) override;
  std::vector<models::Word> getAll() override;
  void update(const models::Word &word) override;
  void
  updateFrequencies(const std::unordered_map<std::wstring, int> &frequencies,
                    const utils::Language &language) override;
  void updateFrequencies(const std::vector<models::Word> &words) override;

private:
  sqlite::connection &mDbConn;
};

} // namespace db
