#include "claude/ClaudeClient.h"
#include "claude/DefineWordPrompt.h"
#include "claude/ExplainSentencePrompt.h"
#include "claude/GrammarReference.h"

#include <Wt/Json/Array.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>
#include <Wt/Json/Serializer.h>
#include <Wt/WString.h>

#include <filesystem>
#include <fstream>
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

const claude::GrammarReference kGrammar = claude::grammarReferenceFrom(
    "<section id=\"noun.definite\" title=\"The definite state\">\n"
    "-aka\n</section>\n"
    "<section id=\"clitics.agent\" title=\"Agent clitics\">\n"
    "...\n</section>");

std::vector<std::string> strings(const Json::Array &array) {
  std::vector<std::string> values;
  for (const auto &value : array)
    values.push_back(utf8(value));
  return values;
}

const char *kExplanation = R"({
  "translation": " I read the book. ",
  "words": [{"surface": "کتێبەکەم", "base_form": "کتێب",
             "base_gloss": "book",
             "morphemes": [
               {"form": "کتێب", "kind": "stem", "function": "book"},
               {"form": "ەکە", "kind": "suffix", "function": "definite"},
               {"form": "م", "kind": "clitic",
                "function": "1sg agent of خوێندەوە"}],
             "leipzig": "book-DEF=1SG"},
            {"surface": "خوێندەوە", "base_form": "خوێندنەوە",
             "base_gloss": "to read", "morphemes": [], "leipzig": "read.PST"}],
  "constructions": [{"words": ["کتێبەکەم", " خوێندەوە"],
                     "name": "ergative past",
                     "explanation": "The agent clitic sits on the object.",
                     "sections": ["clitics.agent"]}],
  "notes": [{"text": "-م is the agent, not 'my'.",
             "sections": ["clitics.agent", "noun.definite"]},
            {"text": " ", "sections": []}]
})";
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

TEST(ClaudeClientTests, GrammarReferenceListsSectionIdsOnce) {
  auto reference = claude::grammarReferenceFrom(
      "<section id=\"a.one\" title=\"One\">x</section>\n"
      "<section  id=\"b.two\">y</section>\n"
      "<section id=\"a.one\" title=\"Again\">z</section>");
  EXPECT_EQ(reference.sectionIds,
            (std::vector<std::string>{"a.one", "b.two"}));
  EXPECT_TRUE(reference.text.starts_with("<section"));
}

TEST(ClaudeClientTests, GrammarReferenceLoadsFromAFile) {
  auto dir = std::filesystem::temp_directory_path();
  auto path = dir / "lexicorp_grammar_test.md";
  std::ofstream(path) << "<section id=\"x.y\" title=\"X\">text</section>";
  auto reference = claude::loadGrammarReference(path.string());
  ASSERT_TRUE(reference);
  EXPECT_EQ(reference->sectionIds, std::vector<std::string>{"x.y"});

  std::ofstream(path) << " \n\t";
  EXPECT_FALSE(claude::loadGrammarReference(path.string()));
  std::filesystem::remove(path);
  EXPECT_FALSE(claude::loadGrammarReference(path.string()));
}

TEST(ClaudeClientTests, ExplainRequestCarriesReferencePromptAndSchema) {
  auto body = claude::explainSentenceRequest(
      "claude-opus-5-5", kGrammar,
      {L"کتێبەکەم خوێندەوە", L"ڕۆژی شەممە بوو.", L"زۆر خۆش بوو."});

  Json::Object request;
  Json::parse(body, request);
  EXPECT_EQ(utf8(request.get("model")), "claude-opus-5-5");
  EXPECT_EQ(utf8(request.get("fallbacks")), "default");

  // The reference first, then the prompt, cached together by the
  // breakpoint on the prompt.
  const Json::Array &system = request.get("system");
  ASSERT_EQ(system.size(), 2u);
  const Json::Object &reference = system[0];
  EXPECT_EQ(utf8(reference.get("text")),
            "<grammar_reference>\n" + kGrammar.text + "\n</grammar_reference>");
  EXPECT_FALSE(reference.contains("cache_control"));
  const Json::Object &prompt = system[1];
  EXPECT_EQ(utf8(prompt.get("text")),
            std::string(claude::kExplainSentencePrompt));
  EXPECT_TRUE(prompt.contains("cache_control"));

  const Json::Array &messages = request.get("messages");
  ASSERT_EQ(messages.size(), 1u);
  const Json::Object &user = messages[0];
  EXPECT_EQ(utf8(user.get("content")),
            "Before: ڕۆژی شەممە بوو.\nSentence: کتێبەکەم خوێندەوە\n"
            "After: زۆر خۆش بوو.");

  const Json::Object &outputConfig = request.get("output_config");
  EXPECT_EQ(utf8(outputConfig.get("effort")), "medium");
  const Json::Object &format = outputConfig.get("format");
  const Json::Object &schema = format.get("schema");
  const Json::Object &properties = schema.get("properties");
  for (const char *name : {"translation", "words", "constructions", "notes"})
    EXPECT_TRUE(properties.contains(name)) << name;

  // Cited sections can only be the reference's ids.
  const Json::Object &notes = properties.get("notes");
  const Json::Object &note = notes.get("items");
  const Json::Object &noteProperties = note.get("properties");
  const Json::Object &sections = noteProperties.get("sections");
  const Json::Object &sectionId = sections.get("items");
  EXPECT_EQ(strings(sectionId.get("enum")),
            (std::vector<std::string>{"noun.definite", "clitics.agent"}));

  const Json::Object &words = properties.get("words");
  const Json::Object &word = words.get("items");
  const Json::Object &wordProperties = word.get("properties");
  const Json::Object &morphemes = wordProperties.get("morphemes");
  const Json::Object &morpheme = morphemes.get("items");
  const Json::Object &morphemeProperties = morpheme.get("properties");
  const Json::Object &kind = morphemeProperties.get("kind");
  EXPECT_EQ(strings(kind.get("enum")),
            (std::vector<std::string>{"stem", "prefix", "suffix", "clitic",
                                      "ezafe", "particle"}));
}

TEST(ClaudeClientTests, ExplainRequestWithoutContextOrSectionIds) {
  auto body = claude::explainSentenceRequest(
      "claude-sonnet-5-5", claude::grammarReferenceFrom("No sections."),
      {L"باشە", L"", L""});

  Json::Object request;
  Json::parse(body, request);
  const Json::Array &messages = request.get("messages");
  const Json::Object &user = messages[0];
  EXPECT_EQ(utf8(user.get("content")), "Sentence: باشە");

  // With no ids to choose from, a section is any string.
  const Json::Object &outputConfig = request.get("output_config");
  const Json::Object &format = outputConfig.get("format");
  const Json::Object &schema = format.get("schema");
  const Json::Object &properties = schema.get("properties");
  const Json::Object &notes = properties.get("notes");
  const Json::Object &note = notes.get("items");
  const Json::Object &noteProperties = note.get("properties");
  const Json::Object &sections = noteProperties.get("sections");
  const Json::Object &sectionId = sections.get("items");
  EXPECT_FALSE(sectionId.contains("enum"));
}

TEST(ClaudeClientTests, ParsesExplanationAndKeepsItsJson) {
  auto explanation =
      claude::parseExplainSentenceResponse(ok(messageWith(kExplanation)));

  EXPECT_TRUE(explanation.error.empty()) << explanation.error;
  EXPECT_EQ(explanation.translation, L"I read the book.");
  ASSERT_EQ(explanation.words.size(), 2u);
  const auto &book = explanation.words[0];
  EXPECT_EQ(book.surface, L"کتێبەکەم");
  EXPECT_EQ(book.baseForm, L"کتێب");
  EXPECT_EQ(book.baseGloss, L"book");
  EXPECT_EQ(book.leipzig, L"book-DEF=1SG");
  ASSERT_EQ(book.morphemes.size(), 3u);
  EXPECT_EQ(book.morphemes[1].form, L"ەکە");
  EXPECT_EQ(book.morphemes[1].kind, L"suffix");
  EXPECT_EQ(book.morphemes[2].function, L"1sg agent of خوێندەوە");
  EXPECT_TRUE(explanation.words[1].morphemes.empty());

  ASSERT_EQ(explanation.constructions.size(), 1u);
  const auto &ergative = explanation.constructions[0];
  EXPECT_EQ(ergative.words,
            (std::vector<std::wstring>{L"کتێبەکەم", L"خوێندەوە"}));
  EXPECT_EQ(ergative.name, L"ergative past");
  EXPECT_EQ(ergative.sections, std::vector<std::string>{"clitics.agent"});

  // Blank notes are dropped.
  ASSERT_EQ(explanation.notes.size(), 1u);
  EXPECT_EQ(explanation.notes[0].sections,
            (std::vector<std::string>{"clitics.agent", "noun.definite"}));

  EXPECT_EQ(explanation.answerJson, kExplanation);
  ASSERT_TRUE(explanation.usage);
  EXPECT_EQ(explanation.usage->outputTokens, 45);

  // A stored answer reads back the same, without usage.
  auto stored = claude::explanationFromJson(explanation.answerJson);
  EXPECT_TRUE(stored.error.empty());
  EXPECT_EQ(stored.translation, explanation.translation);
  EXPECT_EQ(stored.words.size(), 2u);
  EXPECT_EQ(stored.notes.size(), 1u);
  EXPECT_FALSE(stored.usage);
}

TEST(ClaudeClientTests, ExplanationErrors) {
  auto refused =
      claude::parseExplainSentenceResponse(ok(messageWith("", "refusal")));
  EXPECT_EQ(refused.error, "Claude declined to answer");
  EXPECT_TRUE(refused.usage);
  EXPECT_TRUE(refused.answerJson.empty());

  auto cutOff = claude::parseExplainSentenceResponse(
      ok(messageWith(R"({"translation": "I re)", "max_tokens")));
  EXPECT_EQ(cutOff.error, "Claude's answer was cut off");

  EXPECT_EQ(claude::explanationFromJson("not json").error,
            "Claude's answer could not be read");
}

TEST(ClaudeClientTests, ExplainingNeedsTheGrammarReference) {
  auto transport =
      std::make_unique<FakeTransport>(ok(messageWith(kExplanation)));
  auto *fake = transport.get();
  claude::ClaudeClient client{"secret", "claude-sonnet-5-5",
                              std::move(transport)};

  EXPECT_FALSE(client.canExplain());
  auto explanation = client.explainSentence({L"باشە", L"", L""});
  EXPECT_FALSE(explanation.error.empty());
  EXPECT_TRUE(fake->mBody.empty()); // nothing sent
}

TEST(ClaudeClientTests, ExplainsWithTheExplainModel) {
  for (const auto &[explainModel, expected] :
       {std::pair{"claude-opus-5-5", "claude-opus-5-5"},
        std::pair{"", "claude-sonnet-5-5"}}) {
    auto transport =
        std::make_unique<FakeTransport>(ok(messageWith(kExplanation)));
    auto *fake = transport.get();
    claude::ClaudeClient client{"secret", "claude-sonnet-5-5",
                                std::move(transport), kGrammar, explainModel};

    EXPECT_TRUE(client.canExplain());
    auto explanation = client.explainSentence({L"کتێبەکەم خوێندەوە", L"", L""});
    EXPECT_TRUE(explanation.error.empty()) << explanation.error;
    EXPECT_EQ(explanation.translation, L"I read the book.");

    Json::Object request;
    Json::parse(fake->mBody, request);
    EXPECT_EQ(utf8(request.get("model")), expected);
    EXPECT_TRUE(std::ranges::find(fake->mHeaders, "x-api-key: secret") !=
                fake->mHeaders.end());
  }
}
