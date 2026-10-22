#include "claude/ClaudeClient.h"
#include "claude/DefineWordPrompt.h"
#include "claude/ExplainSentencePrompt.h"

#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>
#include <Wt/Json/Serializer.h>
#include <Wt/Json/Value.h>
#include <Wt/WString.h>

#include <algorithm>
#include <cstdlib>
#include <cwctype>
#include <iostream>

namespace claude {
namespace {
namespace Json = Wt::Json;

constexpr const char *kMessagesUrl = "https://api.anthropic.com/v1/messages";
// Opts in to retrying a declined request on Anthropic's recommended
// fallback model ("fallbacks": "default").
constexpr const char *kFallbackBeta = "server-side-fallback-2026-07-01";

std::string toUtf8(const std::wstring &text) {
  return Wt::WString(text).toUTF8();
}

std::wstring fromUtf8(const std::string &text) {
  return Wt::WString::fromUTF8(text).value();
}

Json::Value string(const std::wstring &text) {
  return Json::Value(Wt::WString(text));
}

Json::Value string(std::string_view utf8) {
  return Json::Value(Wt::WString::fromUTF8(std::string(utf8)));
}

std::wstring trimmed(std::wstring text) {
  auto isSpace = [](wchar_t c) { return std::iswspace(c); };
  text.erase(text.begin(), std::ranges::find_if_not(text, isSpace));
  text.erase(
      std::ranges::find_if_not(text.rbegin(), text.rend(), isSpace).base(),
      text.end());
  return text;
}

Json::Object stringProperty() {
  Json::Object property;
  property["type"] = "string";
  return property;
}

// A string that must be one of `values` (any string if there are none).
Json::Object enumProperty(const std::vector<std::string> &values) {
  auto property = stringProperty();
  if (values.empty())
    return property;
  Json::Array allowed;
  for (const auto &value : values)
    allowed.push_back(string(value));
  property["enum"] = std::move(allowed);
  return property;
}

// An object schema whose properties are all required.
Json::Object objectOf(
    std::initializer_list<std::pair<const char *, Json::Object>> fields) {
  Json::Object properties;
  Json::Array required;
  for (const auto &[name, schema] : fields) {
    properties[name] = schema;
    required.push_back(name);
  }
  Json::Object schema;
  schema["type"] = "object";
  schema["properties"] = std::move(properties);
  schema["required"] = std::move(required);
  schema["additionalProperties"] = false;
  return schema;
}

// An object schema whose properties are all strings and all required.
Json::Object stringsObject(std::initializer_list<const char *> names) {
  Json::Object properties;
  Json::Array required;
  for (const char *name : names) {
    properties[name] = stringProperty();
    required.push_back(name);
  }
  Json::Object schema;
  schema["type"] = "object";
  schema["properties"] = std::move(properties);
  schema["required"] = std::move(required);
  schema["additionalProperties"] = false;
  return schema;
}

Json::Object arrayOf(Json::Object items) {
  Json::Object array;
  array["type"] = "array";
  array["items"] = std::move(items);
  return array;
}

// The shape of the answer the prompt asks for.
Json::Object answerSchema() {
  Json::Object properties;
  properties["glosses"] = arrayOf(stringsObject({"gloss", "part_of_speech"}));
  properties["idioms"] =
      arrayOf(stringsObject({"expression", "dictionary_form", "gloss"}));
  Json::Array required;
  required.push_back("glosses");
  required.push_back("idioms");

  Json::Object schema;
  schema["type"] = "object";
  schema["properties"] = std::move(properties);
  schema["required"] = std::move(required);
  schema["additionalProperties"] = false;
  return schema;
}

// The shape of a sentence explanation; cited sections must be among
// `sectionIds`.
Json::Object explanationSchema(const std::vector<std::string> &sectionIds) {
  auto sections = arrayOf(enumProperty(sectionIds));
  auto morpheme = objectOf(
      {{"form", stringProperty()},
       {"kind", enumProperty({"stem", "prefix", "suffix", "clitic", "ezafe",
                              "particle"})},
       {"function", stringProperty()}});
  auto word = objectOf({{"surface", stringProperty()},
                        {"base_form", stringProperty()},
                        {"base_gloss", stringProperty()},
                        {"morphemes", arrayOf(std::move(morpheme))},
                        {"leipzig", stringProperty()}});
  auto construction = objectOf({{"words", arrayOf(stringProperty())},
                                {"name", stringProperty()},
                                {"explanation", stringProperty()},
                                {"sections", sections}});
  auto note =
      objectOf({{"text", stringProperty()}, {"sections", std::move(sections)}});
  return objectOf({{"translation", stringProperty()},
                   {"words", arrayOf(std::move(word))},
                   {"constructions", arrayOf(std::move(construction))},
                   {"notes", arrayOf(std::move(note))}});
}

std::wstring userMessage(const SentenceQuery &query) {
  std::wstring message;
  if (!query.before.empty())
    message += L"Before: " + query.before + L"\n";
  message += L"Sentence: " + query.sentence;
  if (!query.after.empty())
    message += L"\nAfter: " + query.after;
  return message;
}

Json::Object cachedTextBlock(Json::Value text) {
  Json::Object cacheControl;
  cacheControl["type"] = "ephemeral";
  Json::Object block;
  block["type"] = "text";
  block["text"] = std::move(text);
  block["cache_control"] = std::move(cacheControl);
  return block;
}

std::wstring userMessage(const WordQuery &query) {
  std::wstring message = L"Word: " + query.word + L"\nSentences:";
  auto count = std::min(query.sentences.size(), kMaxSentences);
  for (std::size_t i = 0; i < count; ++i)
    message += L"\n" + std::to_wstring(i + 1) + L". " + query.sentences[i];
  return message;
}

bool parseObject(const std::string &text, Json::Object &result) {
  Json::ParseError error;
  try {
    return Json::parse(text, result, error);
  } catch (const Json::TypeException &) {
    return false; // valid JSON, but not an object
  }
}

// Wt::Json throws on a missing member or a wrong type; these return a
// default instead.
const Json::Value *member(const Json::Object &object, const std::string &name) {
  auto it = object.find(name);
  return it == object.end() ? nullptr : &it->second;
}

std::string stringMember(const Json::Object &object, const std::string &name) {
  auto value = member(object, name);
  if (!value || value->type() != Json::Type::String)
    return {};
  return static_cast<const Wt::WString &>(*value).toUTF8();
}

std::int64_t countMember(const Json::Object &object, const std::string &name) {
  auto value = member(object, name);
  if (!value || value->type() != Json::Type::Number)
    return 0;
  return static_cast<long long>(*value);
}

const Json::Object *objectMember(const Json::Object &object,
                                 const std::string &name) {
  auto value = member(object, name);
  if (!value || value->type() != Json::Type::Object)
    return nullptr;
  return &static_cast<const Json::Object &>(*value);
}

const Json::Array *arrayMember(const Json::Object &object,
                               const std::string &name) {
  auto value = member(object, name);
  if (!value || value->type() != Json::Type::Array)
    return nullptr;
  return &static_cast<const Json::Array &>(*value);
}

std::string apiErrorMessage(const HttpResponse &response) {
  std::string message;
  Json::Object body;
  if (parseObject(response.body, body) && objectMember(body, "error"))
    message = stringMember(*objectMember(body, "error"), "message");

  std::string error;
  switch (response.status) {
  case 401:
    error = "The API key was rejected";
    break;
  case 429:
    error = "Too many requests; try again in a minute";
    break;
  case 529:
    error = "Claude is overloaded; try again in a minute";
    break;
  default:
    error = "Claude API error " + std::to_string(response.status);
  }
  return message.empty() ? error : error + ": " + message;
}

// The text of the answer's first text block (adaptive thinking may put a
// thinking block before it).
std::string answerText(const Json::Object &message) {
  if (auto content = arrayMember(message, "content"))
    for (const auto &block : *content)
      if (block.type() == Json::Type::Object) {
        const auto &object = static_cast<const Json::Object &>(block);
        if (stringMember(object, "type") == "text")
          return stringMember(object, "text");
      }
  return {};
}

// The answer text of a response, or nothing (with `error` set) if there is
// none to read. Sets `usage` whenever the API reports it.
std::optional<std::string> answerOf(const HttpResponse &response,
                                    std::optional<Usage> &usage,
                                    std::string &error) {
  if (!response.error.empty()) {
    error = "Could not reach Claude: " + response.error;
    return std::nullopt;
  }
  if (response.status != 200) {
    error = apiErrorMessage(response);
    return std::nullopt;
  }

  Json::Object message;
  if (!parseObject(response.body, message)) {
    error = "Claude's answer could not be read";
    return std::nullopt;
  }
  if (auto counts = objectMember(message, "usage"))
    usage = Usage{
        .model = stringMember(message, "model"),
        .inputTokens = countMember(*counts, "input_tokens"),
        .outputTokens = countMember(*counts, "output_tokens"),
        .cacheReadTokens = countMember(*counts, "cache_read_input_tokens"),
        .cacheCreationTokens =
            countMember(*counts, "cache_creation_input_tokens"),
    };

  auto stopReason = stringMember(message, "stop_reason");
  if (stopReason == "refusal") {
    error = "Claude declined to answer";
    return std::nullopt;
  }
  if (stopReason == "max_tokens") {
    error = "Claude's answer was cut off";
    return std::nullopt;
  }
  return answerText(message);
}

std::wstring trimmedMember(const Json::Object &object,
                           const std::string &name) {
  return trimmed(fromUtf8(stringMember(object, name)));
}

// The objects in `array` (other items skipped).
std::vector<const Json::Object *> objectsIn(const Json::Array *array) {
  std::vector<const Json::Object *> objects;
  if (array)
    for (const auto &item : *array)
      if (item.type() == Json::Type::Object)
        objects.push_back(&static_cast<const Json::Object &>(item));
  return objects;
}

std::vector<std::string> sectionsOf(const Json::Object &object) {
  std::vector<std::string> sections;
  if (auto array = arrayMember(object, "sections"))
    for (const auto &item : *array)
      if (item.type() == Json::Type::String)
        sections.push_back(static_cast<const Wt::WString &>(item).toUTF8());
  return sections;
}

void readExplanation(const std::string &text,
                     SentenceExplanation &explanation) {
  Json::Object answer;
  if (!parseObject(text, answer)) {
    explanation.error = "Claude's answer could not be read";
    return;
  }
  explanation.answerJson = text;
  explanation.translation = trimmedMember(answer, "translation");
  for (const auto *item : objectsIn(arrayMember(answer, "words"))) {
    WordAnalysis word{trimmedMember(*item, "surface"),
                      trimmedMember(*item, "base_form"),
                      trimmedMember(*item, "base_gloss"),
                      {},
                      trimmedMember(*item, "leipzig")};
    for (const auto *part : objectsIn(arrayMember(*item, "morphemes")))
      word.morphemes.push_back(Morpheme{trimmedMember(*part, "form"),
                                        trimmedMember(*part, "kind"),
                                        trimmedMember(*part, "function")});
    explanation.words.push_back(std::move(word));
  }
  for (const auto *item : objectsIn(arrayMember(answer, "constructions"))) {
    Construction construction{{},
                              trimmedMember(*item, "name"),
                              trimmedMember(*item, "explanation"),
                              sectionsOf(*item)};
    if (auto words = arrayMember(*item, "words"))
      for (const auto &word : *words)
        if (word.type() == Json::Type::String)
          construction.words.push_back(
              trimmed(static_cast<const Wt::WString &>(word).value()));
    explanation.constructions.push_back(std::move(construction));
  }
  for (const auto *item : objectsIn(arrayMember(answer, "notes"))) {
    auto text = trimmedMember(*item, "text");
    if (!text.empty())
      explanation.notes.push_back(ExplanationNote{text, sectionsOf(*item)});
  }
}

void readAnswer(const std::string &text, WordLookup &lookup) {
  Json::Object answer;
  if (!parseObject(text, answer)) {
    lookup.error = "Claude's answer could not be read";
    return;
  }
  if (auto glosses = arrayMember(answer, "glosses"))
    for (const auto &item : *glosses) {
      if (item.type() != Json::Type::Object)
        continue;
      const auto &object = static_cast<const Json::Object &>(item);
      auto gloss = trimmed(fromUtf8(stringMember(object, "gloss")));
      // "?" is how the prompt says "don't know".
      if (gloss.empty() || gloss == L"?")
        continue;
      lookup.senses.push_back(Sense{
          gloss, trimmed(fromUtf8(stringMember(object, "part_of_speech")))});
    }
  if (auto idioms = arrayMember(answer, "idioms"))
    for (const auto &item : *idioms) {
      if (item.type() != Json::Type::Object)
        continue;
      const auto &object = static_cast<const Json::Object &>(item);
      auto expression = trimmed(fromUtf8(stringMember(object, "expression")));
      auto dictionaryForm =
          trimmed(fromUtf8(stringMember(object, "dictionary_form")));
      auto gloss = trimmed(fromUtf8(stringMember(object, "gloss")));
      if (dictionaryForm.empty())
        dictionaryForm = expression;
      if (!expression.empty() && !gloss.empty())
        lookup.idioms.push_back(Idiom{expression, dictionaryForm, gloss});
    }
}
} // namespace

ClaudeClient::ClaudeClient(std::string apiKey, std::string model,
                           std::unique_ptr<IHttpTransport> transport,
                           std::optional<GrammarReference> grammar,
                           std::string explainModel)
    : mApiKey(std::move(apiKey)), mModel(std::move(model)),
      mTransport(std::move(transport)), mGrammar(std::move(grammar)),
      mExplainModel(explainModel.empty() ? mModel : std::move(explainModel)) {
}

std::shared_ptr<ClaudeClient> ClaudeClient::fromEnvironment() {
  auto env = [](const char *name) -> std::string {
    const char *value = std::getenv(name);
    return value ? value : "";
  };
  auto apiKey = env("ANTHROPIC_API_KEY");
  if (apiKey.empty())
    return nullptr;
  auto model = env("LEXICORP_CLAUDE_MODEL");

  std::optional<GrammarReference> grammar;
  if (auto path = env("LEXICORP_GRAMMAR_FILE"); !path.empty()) {
    grammar = loadGrammarReference(path);
    if (!grammar)
      std::cerr << "LEXICORP_GRAMMAR_FILE: can't read " << path
                << "; sentence explanations are off\n";
  }
  // An explanation at medium effort can take a few minutes.
  return std::make_shared<ClaudeClient>(
      apiKey, model.empty() ? kDefaultModel : model,
      std::make_unique<CurlTransport>(std::chrono::seconds{300}),
      std::move(grammar), env("LEXICORP_CLAUDE_EXPLAIN_MODEL"));
}

HttpResponse ClaudeClient::post(const std::string &body) {
  return mTransport->post(kMessagesUrl,
                          {"content-type: application/json",
                           "x-api-key: " + mApiKey,
                           "anthropic-version: 2023-06-01",
                           std::string("anthropic-beta: ") + kFallbackBeta},
                          body);
}

WordLookup ClaudeClient::defineWord(const WordQuery &query) {
  return parseDefineWordResponse(post(defineWordRequest(mModel, query)));
}

SentenceExplanation ClaudeClient::explainSentence(const SentenceQuery &query) {
  if (!mGrammar)
    return {.error = "Sentence explanations need the grammar reference"};
  return parseExplainSentenceResponse(
      post(explainSentenceRequest(
          query.model.empty() ? mExplainModel : query.model, *mGrammar,
          query)));
}

bool ClaudeClient::canExplain() const { return mGrammar.has_value(); }

std::string ClaudeClient::explainModel() const { return mExplainModel; }

const GrammarReference *ClaudeClient::grammar() const {
  return mGrammar ? &*mGrammar : nullptr;
}

std::string defineWordRequest(const std::string &model,
                              const WordQuery &query) {
  // The prompt is the same in every request, so it is cached (once it is
  // long enough for the model's minimum cacheable length).
  Json::Array system;
  system.push_back(cachedTextBlock(string(kDefineWordPrompt)));

  Json::Object userTurn;
  userTurn["role"] = "user";
  userTurn["content"] = string(userMessage(query));
  Json::Array messages;
  messages.push_back(std::move(userTurn));

  Json::Object format;
  format["type"] = "json_schema";
  format["schema"] = answerSchema();
  Json::Object outputConfig;
  // A short lookup: little thinking needed.
  outputConfig["effort"] = "low";
  outputConfig["format"] = std::move(format);

  Json::Object request;
  request["model"] = string(model);
  request["max_tokens"] = 4096;
  request["system"] = std::move(system);
  request["messages"] = std::move(messages);
  request["output_config"] = std::move(outputConfig);
  request["fallbacks"] = "default";
  return Json::serialize(request, 0);
}

WordLookup parseDefineWordResponse(const HttpResponse &response) {
  WordLookup lookup;
  if (auto text = answerOf(response, lookup.usage, lookup.error))
    readAnswer(*text, lookup);
  return lookup;
}

std::string explainSentenceRequest(const std::string &model,
                                   const GrammarReference &grammar,
                                   const SentenceQuery &query) {
  // The reference and the prompt are the same in every request; the
  // breakpoint on the second block caches both.
  Json::Object reference;
  reference["type"] = "text";
  reference["text"] = string("<grammar_reference>\n" + grammar.text +
                             "\n</grammar_reference>");
  Json::Array system;
  system.push_back(std::move(reference));
  system.push_back(cachedTextBlock(string(kExplainSentencePrompt)));

  Json::Object userTurn;
  userTurn["role"] = "user";
  userTurn["content"] = string(userMessage(query));
  Json::Array messages;
  messages.push_back(std::move(userTurn));

  Json::Object format;
  format["type"] = "json_schema";
  format["schema"] = explanationSchema(grammar.sectionIds);
  Json::Object outputConfig;
  // Every word analysed against the grammar: more thinking than a lookup.
  outputConfig["effort"] = "medium";
  outputConfig["format"] = std::move(format);

  Json::Object request;
  request["model"] = string(model);
  request["max_tokens"] = 16000;
  request["system"] = std::move(system);
  request["messages"] = std::move(messages);
  request["output_config"] = std::move(outputConfig);
  request["fallbacks"] = "default";
  return Json::serialize(request, 0);
}

SentenceExplanation parseExplainSentenceResponse(const HttpResponse &response) {
  SentenceExplanation explanation;
  if (auto text = answerOf(response, explanation.usage, explanation.error))
    readExplanation(*text, explanation);
  return explanation;
}

SentenceExplanation explanationFromJson(const std::string &answerJson) {
  SentenceExplanation explanation;
  readExplanation(answerJson, explanation);
  return explanation;
}

std::string modelLabel(const std::string &model) {
  for (const auto &choice : kExplainModels)
    if (model == choice.id)
      return choice.label;
  return model;
}

std::optional<Pricing> pricingFor(const std::string &model,
                                  std::int64_t promptTokens) {
  if (model.starts_with("claude-sonnet-5-5"))
    return Pricing{2.0, 10.0, 0.20, 2.50};
  // Above 100k prompt tokens every token of the request costs 5x; the cache
  // rates are assumed to scale the same way.
  if (model.starts_with("claude-haiku-5-5"))
    return promptTokens > 100'000 ? Pricing{0.50, 2.50, 0.05, 0.625}
                                  : Pricing{0.10, 0.50, 0.01, 0.125};
  if (model.starts_with("claude-opus-5-5"))
    return Pricing{4.0, 20.0, 0.20, 5.0};
  return std::nullopt;
}

std::optional<double> costOf(const Usage &usage) {
  auto pricing = pricingFor(usage.model, usage.inputTokens +
                                             usage.cacheReadTokens +
                                             usage.cacheCreationTokens);
  if (!pricing)
    return std::nullopt;
  return (usage.inputTokens * pricing->input +
          usage.outputTokens * pricing->output +
          usage.cacheReadTokens * pricing->cacheRead +
          usage.cacheCreationTokens * pricing->cacheWrite) /
         1e6;
}
} // namespace claude
