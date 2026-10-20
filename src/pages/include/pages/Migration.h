#pragma once

#include "pages/AppContext.h"

// The database version migrateDatabase() brings a database to.
constexpr int kDatabaseVersion = 3;

// Brings the database up to date, tracking the version in SQLite's
// user_version. A no-op once up to date. Not safe to run concurrently, so
// main() runs it before the server starts sessions.
void migrateDatabase(AppContext &appContext);
