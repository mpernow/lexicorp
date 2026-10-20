#pragma once

#include <Wt/WApplication.h>
#include <Wt/WServer.h>

#include <functional>
#include <thread>
#include <utility>

// Set by tests: Wt's test environment has a server, but no event loop to run
// what is posted to a session.
inline bool runBackgroundWorkInForeground = false;

// Runs `work` on its own thread and then `done` with its result in the
// session (server push), so a slow request doesn't hold up the session.
// `work` must not touch the session or its database connection. If the
// session ends first, `done` is dropped.
template <typename Result>
void runInBackground(std::function<Result()> work,
                     std::function<void(Result)> done) {
  auto *app = Wt::WApplication::instance();
  auto *server = Wt::WServer::instance();
  if (runBackgroundWorkInForeground || !server) {
    done(work());
    return;
  }

  app->enableUpdates(true);
  std::thread([server, sessionId = app->sessionId(), work = std::move(work),
               done = std::move(done)] {
    auto result = std::make_shared<Result>(work());
    server->post(sessionId, [done, result] {
      done(std::move(*result));
      Wt::WApplication::instance()->triggerUpdate();
    });
  }).detach();
}
