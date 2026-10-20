#include "claude/ClaudeClient.h"
#include "claude/DefineWordPrompt.h"

#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>
#include <Wt/Json/Serializer.h>
#include <Wt/WString.h>

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {
namespace Json = Wt::Json;

// Records the request and answers with a canned response.
class FakeTransport : public claude::IHttpTransport {
public:
  explicit FakeTransport(claude::HttpResponse response)
      : mResponse(std::move(response)) {}

  claude::HttpResponse post(const std::string &url,
                            const std::vector<std::string> &headers,
                            const std::string &body) override {
    mUrl = url;
    mHeaders = headers;
    mBody = body;
    return mResponse;
  }

  claude::HttpResponse mResponse;
  std::string mUrl;
  std::vector<std::string> mHeaders;
  std::string mBody;
};

std::string utf8(const Json::Value &value) {
  return static_cast<const Wt::WString &>(value).toUTF8();
}

// A Messages API response whose text block is `answer`.
std::string messageWith(const std::string &answer,
                        const std::string &stopReason = "end_turn") {
  Json::Object text;
  text["type"] = "text";
  text["text"] = Json::Value(Wt::WString::fromUTF8(answer));
  // Adaptive thinking can come first, with its text omitted.
  Json::Object thinking;
  thinking["type"] = "thinking";
  thinking["thinking"] = "";
  Json::Array content;
  content.push_back(std::move(thinking));
  content.push_back(std::move(text));

  Json::Object usage;
  usage["input_tokens"] = 120;
  usage["output_tokens"] = 45;
  usage["cache_read_input_tokens"] = 600;
  usage["cache_creation_input_tokens"] = 0;

  Json::Object message;
  message["model"] = "claude-sonnet-5-5";
  message["stop_reason"] = Json::Value(Wt::WString::fromUTF8(stopReason));
  message["content"] = std::move(content);
  message["usage"] = std::move(usage);
  return Json::serialize(message, 0);
}

claude::HttpResponse ok(const std::string &body) { return {200, body, ""}; }

const claude::WordQuery kQuery{L"پێم", {L"پێم وایە درەنگە", L"پێم بڵێ"}};
} // namespace

TEST(ClaudeClientTests, RequestCarriesPromptQueryAndSchema) {
  std::vector<std::wstring> sentences;
  for (int i = 1; i <= 7; ++i)
    sentences.push_back(L"sentence " + std::to_wstring(i));
  auto body =
      claude::defineWordRequest("claude-sonnet-5-5", {L"word", sentences});

  Json::Object request;
  Json::parse(body, request);
  EXPECT_EQ(utf8(request.get("model")), "claude-sonnet-5-5");
  EXPECT_EQ(utf8(request.get("fallbacks")), "default");

  const Json::Array &system = request.get("system");
  ASSERT_EQ(system.size(), 1u);
  const Json::Object &prompt = system[0];
  EXPECT_EQ(utf8(prompt.get("text")), std::string(claude::kDefineWordPrompt));
  const Json::Object &cacheControl = prompt.get("cache_control");
  EXPECT_EQ(utf8(cacheControl.get("type")), "ephemeral");

  // Only the first kMaxSentences sentences, numbered.
  const Json::Array &messages = request.get("messages");
  ASSERT_EQ(messages.size(), 1u);
  const Json::Object &user = messages[0];
  EXPECT_EQ(utf8(user.get("role")), "user");
  EXPECT_EQ(utf8(user.get("content")),
            "Word: word\nSentences:\n1. sentence 1\n2. sentence 2\n"
            "3. sentence 3\n4. sentence 4\n5. sentence 5");

  const Json::Object &outputConfig = request.get("output_config");
  const Json::Object &format = outputConfig.get("format");
  EXPECT_EQ(utf8(format.get("type")), "json_schema");
  const Json::Object &schema = format.get("schema");
  const Json::Object &properties = schema.get("properties");
  EXPECT_TRUE(properties.contains("glosses"));
  EXPECT_TRUE(properties.contains("idioms"));
  EXPECT_FALSE(static_cast<bool>(schema.get("additionalProperties")));
}

TEST(ClaudeClientTests, SendsKeyAndVersionHeaders) {
  auto transport = std::make_unique<FakeTransport>(
      ok(messageWith(R"({"glosses": [], "idioms": []})")));
  auto *fake = transport.get();
  claude::ClaudeClient client{"secret", "claude-sonnet-5-5",
                              std::move(transport)};

  client.defineWord(kQuery);

  EXPECT_EQ(fake->mUrl, "https://api.anthropic.com/v1/messages");
  auto has = [&](const std::string &header) {
    return std::ranges::find(fake->mHeaders, header) != fake->mHeaders.end();
  };
  EXPECT_TRUE(has("x-api-key: secret"));
  EXPECT_TRUE(has("anthropic-version: 2023-06-01"));
  EXPECT_TRUE(has("anthropic-beta: server-side-fallback-2026-07-01"));
  EXPECT_TRUE(has("content-type: application/json"));
}

TEST(ClaudeClientTests, ParsesGlossesIdiomsAndUsage) {
  auto lookup = claude::parseDefineWordResponse(ok(messageWith(
      R"({"glosses": [{"gloss": " to me ", "part_of_speech": "preposition"},
                      {"gloss": "my foot", "part_of_speech": "noun"}],
          "idioms": [{"expression": "پێم وایە", "dictionary_form": "",
                      "gloss": "i think"},
                     {"expression": "تاوتوێی کرد",
                      "dictionary_form": "تاوتوێ کردن",
                      "gloss": "to discuss"}]})")));

  EXPECT_TRUE(lookup.error.empty()) << lookup.error;
  ASSERT_EQ(lookup.senses.size(), 2u);
  EXPECT_EQ(lookup.senses[0].gloss, L"to me");
  EXPECT_EQ(lookup.senses[0].partOfSpeech, L"preposition");
  EXPECT_EQ(lookup.senses[1].gloss, L"my foot");
  ASSERT_EQ(lookup.idioms.size(), 2u);
  EXPECT_EQ(lookup.idioms[0].expression, L"پێم وایە");
  // Without a dictionary form, the expression is its own.
  EXPECT_EQ(lookup.idioms[0].dictionaryForm, L"پێم وایە");
  EXPECT_EQ(lookup.idioms[0].gloss, L"i think");
  EXPECT_EQ(lookup.idioms[1].expression, L"تاوتوێی کرد");
  EXPECT_EQ(lookup.idioms[1].dictionaryForm, L"تاوتوێ کردن");

  ASSERT_TRUE(lookup.usage);
  EXPECT_EQ(lookup.usage->model, "claude-sonnet-5-5");
  EXPECT_EQ(lookup.usage->inputTokens, 120);
  EXPECT_EQ(lookup.usage->outputTokens, 45);
  EXPECT_EQ(lookup.usage->cacheReadTokens, 600);
}

TEST(ClaudeClientTests, UnknownWordGivesNoSenses) {
  auto lookup = claude::parseDefineWordResponse(ok(messageWith(
      R"({"glosses": [{"gloss": "?", "part_of_speech": ""}], "idioms": []})")));
  EXPECT_TRUE(lookup.error.empty());
  EXPECT_TRUE(lookup.senses.empty());
}

TEST(ClaudeClientTests, RefusalAndTruncationAreErrorsWithUsage) {
  auto refused =
      claude::parseDefineWordResponse(ok(messageWith("", "refusal")));
  EXPECT_EQ(refused.error, "Claude declined to answer");
  EXPECT_TRUE(refused.usage);

  auto cutOff = claude::parseDefineWordResponse(
      ok(messageWith(R"({"glosses": [)", "max_tokens")));
  EXPECT_EQ(cutOff.error, "Claude's answer was cut off");
  EXPECT_TRUE(cutOff.senses.empty());
  EXPECT_TRUE(cutOff.usage);
}

TEST(ClaudeClientTests, ApiAndTransportErrors) {
  auto overloaded = claude::parseDefineWordResponse(
      {529,
       R"({"type": "error", "error": {"type": "overloaded_error",
           "message": "Overloaded"}})",
       ""});
  EXPECT_EQ(overloaded.error,
            "Claude is overloaded; try again in a minute: Overloaded");
  EXPECT_FALSE(overloaded.usage);

  auto badRequest = claude::parseDefineWordResponse({400, "not json", ""});
  EXPECT_EQ(badRequest.error, "Claude API error 400");

  auto offline =
      claude::parseDefineWordResponse({0, "", "Couldn't resolve host name"});
  EXPECT_EQ(offline.error,
            "Could not reach Claude: Couldn't resolve host name");

  auto garbled = claude::parseDefineWordResponse(ok(messageWith("not json")));
  EXPECT_EQ(garbled.error, "Claude's answer could not be read");
}

TEST(ClaudeClientTests, CostUsesTheModelsPrices) {
  claude::Usage usage{.model = "claude-sonnet-5-5",
                      .inputTokens = 1'000'000,
                      .outputTokens = 100'000,
                      .cacheReadTokens = 1'000'000,
                      .cacheCreationTokens = 0};
  ASSERT_TRUE(claude::costOf(usage));
  EXPECT_DOUBLE_EQ(*claude::costOf(usage), 2.0 + 1.0 + 0.2);

  usage.model = "some-future-model";
  EXPECT_FALSE(claude::costOf(usage));
}
