#pragma once

#include "claude/HttpTransport.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace claude {
// The default model, unless LEXICORP_CLAUDE_MODEL names another.
inline constexpr const char *kDefaultModel = "claude-sonnet-5-5";

// A word to look up, with sentences it occurs in (at most kMaxSentences are
// sent). Sorani Kurdish only: that is what the prompt is written for.
struct WordQuery {
  std::wstring word;
  std::vector<std::wstring> sentences;
};
inline constexpr std::size_t kMaxSentences = 5;

struct Sense {
  std::wstring gloss;
  std::wstring partOfSpeech; // may be empty
};

// An idiom, fixed expression or compound verb the word is part of in one of
// the sentences.
struct Idiom {
  // Its words as they appear in the sentence, in order, without the words
  // between them (e.g. a compound verb's object).
  std::wstring expression;
  // As a dictionary lists it (verbs in the infinitive); never empty.
  std::wstring dictionaryForm;
  std::wstring gloss;
};

// What a request was billed for.
struct Usage {
  std::string model; // the one that answered
  std::int64_t inputTokens = 0;
  std::int64_t outputTokens = 0;
  std::int64_t cacheReadTokens = 0;
  std::int64_t cacheCreationTokens = 0;
};

// The answer to a WordQuery. `error` is empty on success; senses and idioms
// can both be empty if Claude didn't know the word. `usage` is set whenever
// the API answered, failed request or not.
struct WordLookup {
  std::vector<Sense> senses;
  std::vector<Idiom> idioms;
  std::optional<Usage> usage;
  std::string error; // UTF-8, for display
};

class IClaudeClient {
public:
  virtual ~IClaudeClient() = default;

  // Blocks until Claude answers (or the request fails), so call it off the
  // session thread. Safe to call from several threads at once.
  virtual WordLookup defineWord(const WordQuery &query) = 0;
};

class ClaudeClient : public IClaudeClient {
public:
  ClaudeClient(std::string apiKey, std::string model,
               std::unique_ptr<IHttpTransport> transport);

  // From ANTHROPIC_API_KEY and LEXICORP_CLAUDE_MODEL; null without a key.
  static std::shared_ptr<ClaudeClient> fromEnvironment();

  WordLookup defineWord(const WordQuery &query) override;

private:
  std::string mApiKey;
  std::string mModel;
  std::unique_ptr<IHttpTransport> mTransport;
};

// The Messages API request body for `query` (UTF-8 JSON).
std::string defineWordRequest(const std::string &model,
                              const WordQuery &query);
// Reads a Messages API response to a defineWordRequest().
WordLookup parseDefineWordResponse(const HttpResponse &response);

// Price in US dollars per million tokens.
struct Pricing {
  double input;
  double output;
  double cacheRead;
  double cacheWrite; // 5-minute cache
};
// Empty for a model whose price isn't known here.
std::optional<Pricing> pricingFor(const std::string &model);
// What `usage` cost, if its model's price is known.
std::optional<double> costOf(const Usage &usage);
} // namespace claude
