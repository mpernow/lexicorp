// Explains sentences with the app's own request, for tools/explain_eval.py.
// Reads one JSON object per line on stdin ({"sentence", "before", "after"})
// and writes one per line on stdout: the answer JSON as Claude returned it,
// the error (empty on success), the model and the token counts. Configured
// like the app: ANTHROPIC_API_KEY, LEXICORP_CLAUDE_EXPLAIN_MODEL (or
// LEXICORP_CLAUDE_MODEL) and LEXICORP_GRAMMAR_FILE.

#include "claude/ClaudeClient.h"

#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>
#include <Wt/Json/Serializer.h>
#include <Wt/WString.h>

#include <iostream>
#include <string>

namespace Json = Wt::Json;

int main() {
  auto client = claude::ClaudeClient::fromEnvironment();
  if (!client || !client->canExplain()) {
    std::cerr << "Needs ANTHROPIC_API_KEY and LEXICORP_GRAMMAR_FILE\n";
    return 1;
  }

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty())
      continue;
    Json::Object input;
    Json::parse(line, input);
    auto text = [&](const char *key) {
      return Wt::WString::fromUTF8(input.get(key).orIfNull(std::string()))
          .value();
    };
    auto explanation =
        client->explainSentence({text("sentence"), text("before"), text("after")});

    Json::Object output;
    output["answer"] = Json::Value(Wt::WString::fromUTF8(explanation.answerJson));
    output["error"] = Json::Value(Wt::WString::fromUTF8(explanation.error));
    if (const auto &usage = explanation.usage) {
      output["model"] = Json::Value(Wt::WString::fromUTF8(usage->model));
      output["input_tokens"] = Json::Value(static_cast<long long>(usage->inputTokens));
      output["output_tokens"] = Json::Value(static_cast<long long>(usage->outputTokens));
      output["cache_read_tokens"] =
          Json::Value(static_cast<long long>(usage->cacheReadTokens));
      output["cache_creation_tokens"] =
          Json::Value(static_cast<long long>(usage->cacheCreationTokens));
      if (auto cost = claude::costOf(*usage))
        output["cost"] = Json::Value(*cost);
    }
    // One line per answer: serialize() breaks lines even at indent 0. A
    // string's own line breaks are escaped, so every raw one can go.
    auto json = Json::serialize(output, 0);
    std::erase(json, '\n');
    std::cout << json << std::endl;
  }
  return 0;
}
