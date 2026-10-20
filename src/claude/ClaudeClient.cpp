#include "claude/ClaudeClient.h"
#include "claude/DefineWordPrompt.h"

#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>
#include <Wt/Json/Serializer.h>
#include <Wt/Json/Value.h>
#include <Wt/WString.h>

#include <algorithm>
#include <cstdlib>
#include <cwctype>

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
                           std::unique_ptr<IHttpTransport> transport)
    : mApiKey(std::move(apiKey)), mModel(std::move(model)),
      mTransport(std::move(transport)) {}

std::shared_ptr<ClaudeClient> ClaudeClient::fromEnvironment() {
  const char *apiKey = std::getenv("ANTHROPIC_API_KEY");
  if (!apiKey || !*apiKey)
    return nullptr;
  const char *model = std::getenv("LEXICORP_CLAUDE_MODEL");
  return std::make_shared<ClaudeClient>(
      apiKey, model && *model ? model : kDefaultModel,
      std::make_unique<CurlTransport>());
}

WordLookup ClaudeClient::defineWord(const WordQuery &query) {
  return parseDefineWordResponse(mTransport->post(
      kMessagesUrl,
      {"content-type: application/json", "x-api-key: " + mApiKey,
       "anthropic-version: 2023-06-01",
       std::string("anthropic-beta: ") + kFallbackBeta},
      defineWordRequest(mModel, query)));
}

std::string defineWordRequest(const std::string &model,
                              const WordQuery &query) {
  // The prompt is the same in every request, so it is cached (once it is
  // long enough for the model's minimum cacheable length).
  Json::Object cacheControl;
  cacheControl["type"] = "ephemeral";
  Json::Object systemBlock;
  systemBlock["type"] = "text";
  systemBlock["text"] = string(kDefineWordPrompt);
  systemBlock["cache_control"] = std::move(cacheControl);
  Json::Array system;
  system.push_back(std::move(systemBlock));

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
  if (!response.error.empty()) {
    lookup.error = "Could not reach Claude: " + response.error;
    return lookup;
  }
  if (response.status != 200) {
    lookup.error = apiErrorMessage(response);
    return lookup;
  }

  Json::Object message;
  if (!parseObject(response.body, message)) {
    lookup.error = "Claude's answer could not be read";
    return lookup;
  }
  if (auto usage = objectMember(message, "usage"))
    lookup.usage = Usage{
        .model = stringMember(message, "model"),
        .inputTokens = countMember(*usage, "input_tokens"),
        .outputTokens = countMember(*usage, "output_tokens"),
        .cacheReadTokens = countMember(*usage, "cache_read_input_tokens"),
        .cacheCreationTokens =
            countMember(*usage, "cache_creation_input_tokens"),
    };

  auto stopReason = stringMember(message, "stop_reason");
  if (stopReason == "refusal") {
    lookup.error = "Claude declined to answer";
    return lookup;
  }
  if (stopReason == "max_tokens") {
    lookup.error = "Claude's answer was cut off";
    return lookup;
  }
  readAnswer(answerText(message), lookup);
  return lookup;
}

std::optional<Pricing> pricingFor(const std::string &model) {
  if (model.starts_with("claude-sonnet-5-5"))
    return Pricing{2.0, 10.0, 0.20, 2.50};
  if (model.starts_with("claude-haiku-5-5"))
    return Pricing{0.10, 0.50, 0.01, 0.125};
  if (model.starts_with("claude-opus-5-5"))
    return Pricing{4.0, 20.0, 0.20, 5.0};
  return std::nullopt;
}

std::optional<double> costOf(const Usage &usage) {
  auto pricing = pricingFor(usage.model);
  if (!pricing)
    return std::nullopt;
  return (usage.inputTokens * pricing->input +
          usage.outputTokens * pricing->output +
          usage.cacheReadTokens * pricing->cacheRead +
          usage.cacheCreationTokens * pricing->cacheWrite) /
         1e6;
}
} // namespace claude
