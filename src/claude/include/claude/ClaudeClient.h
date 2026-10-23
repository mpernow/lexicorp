#pragma once

#include "claude/GrammarReference.h"
#include "claude/HttpTransport.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace claude {
// The default model, unless LEXICORP_CLAUDE_MODEL names another (and
// LEXICORP_CLAUDE_EXPLAIN_MODEL for sentence explanations).
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

// A Sorani sentence to explain, with its neighbours for context (either may
// be empty). Only `sentence` is explained.
struct SentenceQuery {
  std::wstring sentence;
  std::wstring before;
  std::wstring after;
  std::string model; // empty: the client's explainModel()
};

// The models the sidebar offers for explanations.
struct ModelChoice {
  const char *id;
  const char *label;
};
inline constexpr std::array<ModelChoice, 2> kExplainModels{{
    {"claude-sonnet-5-5", "Sonnet"},
    {"claude-haiku-5-5", "Haiku"},
}};
// Its kExplainModels label, or the id itself.
std::string modelLabel(const std::string &model);

// One part of a word: its stem, an affix or a clitic.
struct Morpheme {
  std::wstring form;
  // stem, prefix, suffix, clitic, ezafe or particle
  std::wstring kind;
  std::wstring function; // e.g. "definite", "1sg agent"
};

struct WordAnalysis {
  std::wstring surface; // as written in the sentence
  std::wstring baseForm;
  std::wstring baseGloss;
  std::vector<Morpheme> morphemes;
  std::wstring leipzig; // e.g. "book-DEF=1SG"
};

// A structure spanning several words: a compound verb, an izafe chain, an
// idiom, a relative clause.
struct Construction {
  std::vector<std::wstring> words;
  std::wstring name;
  std::wstring explanation;
  std::vector<std::string> sections; // grammar reference ids
};

// A point a learner is likely to miss.
struct ExplanationNote {
  std::wstring text;
  std::vector<std::string> sections;
};

// The answer to a SentenceQuery. `error` is empty on success, and set for
// an answer without a translation or words. `answerJson`
// is Claude's answer as returned, to store and read back with
// explanationFromJson() without asking again. `usage` is set whenever the
// API answered, failed request or not.
struct SentenceExplanation {
  std::wstring translation;
  std::vector<WordAnalysis> words;
  std::vector<Construction> constructions;
  std::vector<ExplanationNote> notes;
  std::string answerJson;
  std::optional<Usage> usage;
  std::string error; // UTF-8, for display
};

class IClaudeClient {
public:
  virtual ~IClaudeClient() = default;

  // These block until Claude answers (or the request fails), so call them
  // off the session thread. Safe to call from several threads at once.
  virtual WordLookup defineWord(const WordQuery &query) = 0;
  virtual SentenceExplanation explainSentence(const SentenceQuery &query) = 0;

  // Whether explainSentence() is available: it needs the grammar reference.
  virtual bool canExplain() const = 0;
  // The model explainSentence() uses when the query names none.
  virtual std::string explainModel() const = 0;
  // The grammar reference explanations cite; null without one.
  virtual const GrammarReference *grammar() const = 0;
};

class ClaudeClient : public IClaudeClient {
public:
  // Without `grammar`, sentences can't be explained. An empty
  // `explainModel` means `model`. `promptGrammar`, a shorter version of
  // `grammar` with the same section ids, is the one sent with explanations
  // (grammar() is still `grammar`, for reading); it is ignored if its ids
  // differ.
  ClaudeClient(std::string apiKey, std::string model,
               std::unique_ptr<IHttpTransport> transport,
               std::optional<GrammarReference> grammar = std::nullopt,
               std::string explainModel = "",
               std::optional<GrammarReference> promptGrammar = std::nullopt);

  // From ANTHROPIC_API_KEY, LEXICORP_CLAUDE_MODEL,
  // LEXICORP_CLAUDE_EXPLAIN_MODEL, LEXICORP_GRAMMAR_FILE and
  // LEXICORP_GRAMMAR_PROMPT_FILE (the condensed reference); null without a
  // key.
  static std::shared_ptr<ClaudeClient> fromEnvironment();

  WordLookup defineWord(const WordQuery &query) override;
  SentenceExplanation explainSentence(const SentenceQuery &query) override;
  bool canExplain() const override;
  std::string explainModel() const override;
  const GrammarReference *grammar() const override;

private:
  HttpResponse post(const std::string &body);

  std::string mApiKey;
  std::string mModel;
  std::unique_ptr<IHttpTransport> mTransport;
  std::optional<GrammarReference> mGrammar;
  std::string mExplainModel;
  std::optional<GrammarReference> mPromptGrammar;
};

// The Messages API request body for `query` (UTF-8 JSON).
std::string defineWordRequest(const std::string &model,
                              const WordQuery &query);
// Reads a Messages API response to a defineWordRequest().
WordLookup parseDefineWordResponse(const HttpResponse &response);

// The Messages API request body for explaining `query` with `grammar`
// (UTF-8 JSON). The answer may only cite the grammar's section ids.
std::string explainSentenceRequest(const std::string &model,
                                   const GrammarReference &grammar,
                                   const SentenceQuery &query);
// Reads a Messages API response to an explainSentenceRequest().
SentenceExplanation parseExplainSentenceResponse(const HttpResponse &response);
// Reads a stored SentenceExplanation::answerJson (no usage).
SentenceExplanation explanationFromJson(const std::string &answerJson);

// Price in US dollars per million tokens.
struct Pricing {
  double input;
  double output;
  double cacheRead;
  double cacheWrite; // 5-minute cache
};
// The price of a request to `model` whose prompt (input, cache read and cache
// write) is `promptTokens` long: Haiku 5.5 charges more above 100k. Empty
// for a model whose price isn't known here.
std::optional<Pricing> pricingFor(const std::string &model,
                                  std::int64_t promptTokens = 0);
// What one request cost, if its model's price is known.
std::optional<double> costOf(const Usage &usage);
} // namespace claude
