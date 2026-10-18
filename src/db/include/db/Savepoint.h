#pragma once

#include <sqlite/connection.hpp>
#include <sqlite/execute.hpp>
#include <string>

namespace db {

// A transaction that can be nested: SQLite's SAVEPOINT starts a transaction
// when none is open and becomes a nested one inside an outer transaction
// (BEGIN can't be nested). The changes are kept only if commit() is called;
// leaving the scope without it (e.g. by an exception) rolls them back. Unlike
// sqlite::savepoint, which releases (keeps) the changes in its destructor.
class Savepoint {
public:
  Savepoint(sqlite::connection &connection, std::string name)
      : mConnection(connection), mName(std::move(name)) {
    exec("SAVEPOINT " + mName);
  }

  ~Savepoint() {
    if (mActive) {
      try {
        exec("ROLLBACK TO SAVEPOINT " + mName);
        exec("RELEASE SAVEPOINT " + mName);
      } catch (...) {
        // Destructors can't report errors; the outer scope will see the
        // database in whatever state SQLite left it.
      }
    }
  }

  Savepoint(const Savepoint &) = delete;
  Savepoint &operator=(const Savepoint &) = delete;

  void commit() {
    exec("RELEASE SAVEPOINT " + mName);
    mActive = false;
  }

private:
  void exec(const std::string &sql) { sqlite::execute(mConnection, sql, true); }

  sqlite::connection &mConnection;
  std::string mName;
  bool mActive = true;
};

} // namespace db
