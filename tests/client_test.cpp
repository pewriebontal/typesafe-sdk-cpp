// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "typesafe/client.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "typesafe/testing.h"

namespace
{

using namespace typesafe;
using typesafe::testing::BasicRequest;
using typesafe::testing::ModelsResponse;
using typesafe::testing::RecordingTransport;
using typesafe::testing::ValidResponse;

TEST(BuilderTest, RequiresAnApiKey)
{
	const char *held = std::getenv("TYPESAFE_API_KEY");

	::unsetenv("TYPESAFE_API_KEY");
	EXPECT_THROW(TypeSafeClient::builder().build(), AuthenticationError);
	if (held != nullptr)
		::setenv("TYPESAFE_API_KEY", held, 1);
}

TEST(BuilderTest, TrimsTrailingSlashesFromTheBaseUrl)
{
	auto  transport = std::make_unique<RecordingTransport>();
	auto *recording = transport.get();

	recording->responses.push_back(
	    ValidResponse({{"safe", {{"type", "noul"}, {"noul", 0.5}}}}));
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .base_url("https://example.test///")
	                  .transport(std::move(transport))
	                  .build();

	const SystemOneResponse answered = client.systemOne(BasicRequest());

	EXPECT_EQ(recording->requests.front().url,
	          "https://example.test/v1/systemone");
	EXPECT_DOUBLE_EQ(answered.nouls.at("safe").noul, 0.5);
}

TEST(BuilderTest, TrimsTheApiKey)
{
	auto  transport = std::make_unique<RecordingTransport>();
	auto *recording = transport.get();

	recording->responses.push_back(
	    ValidResponse({{"safe", {{"type", "noul"}, {"noul", 0.5}}}}));
	auto client = TypeSafeClient::builder()
	                  .api_key("  test-key\n")
	                  .transport(std::move(transport))
	                  .build();

	client.systemOne(BasicRequest());
	EXPECT_EQ(recording->requests.front().headers.at("Authorization"),
	          "Bearer test-key");
}

TEST(BuilderTest, RejectsAnApiKeyWithInnerWhitespace)
{
	EXPECT_THROW(TypeSafeClient::builder()
	                 .api_key("test key")
	                 .transport(std::make_unique<RecordingTransport>())
	                 .build(),
	             AuthenticationError);
}

TEST(BuilderTest, RejectsABaseUrlWithoutAScheme)
{
	EXPECT_THROW(TypeSafeClient::builder()
	                 .api_key("test-key")
	                 .base_url("api.typesafe.ai")
	                 .transport(std::make_unique<RecordingTransport>())
	                 .build(),
	             ValidationError);
}

TEST(BuilderTest, IgnoresEmptyEnvironmentValues)
{
	const char       *held = std::getenv("TYPESAFE_BASE_URL");
	const std::string saved = held != nullptr ? held : "";

	::setenv("TYPESAFE_BASE_URL", "   ", 1);
	auto  transport = std::make_unique<RecordingTransport>();
	auto *recording = transport.get();

	recording->responses.push_back(
	    ValidResponse({{"safe", {{"type", "noul"}, {"noul", 0.5}}}}));
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .transport(std::move(transport))
	                  .build();

	client.systemOne(BasicRequest());
	if (held != nullptr)
		::setenv("TYPESAFE_BASE_URL", saved.c_str(), 1);
	else
		::unsetenv("TYPESAFE_BASE_URL");
	EXPECT_EQ(recording->requests.front().url,
	          "https://api.typesafe.ai/v1/systemone");
}

class WireContractTest : public ::testing::Test
{
	protected:
		void SetUp() override
		{
			auto transport = std::make_unique<RecordingTransport>();

			_recording = transport.get();
			_recording->responses.push_back(
			    ValidResponse({{"safe", {{"type", "noul"}, {"noul", 0.9}}}}));
			_client = TypeSafeClient::builder()
			              .api_key("test-key")
			              .transport(std::move(transport))
			              .build();
			_client->systemOne(Request());
		}

		SystemOneRequest Request() const
		{
			SystemOneRequest request;

			request.state = nlohmann::json{{"message", "hello"}};
			request.extra_headers["X-Team"] = "sdk";
			request.extra_body["beta"] = true;
			request.add(
			    "route",
			    Choice{nlohmann::json{{"task", "Choose a route"}},
				       {{"safe", nlohmann::json{{"description", "Allowed"}}},
				        {"unsafe", std::nullopt}}});
			request.add("safe",
			            Noul{std::nullopt,
			                 NoulCriteria{nlohmann::json("Allowed"),
			                              nlohmann::json("Not allowed")}});
			return (request);
		}

		nlohmann::json BodyOf(std::size_t attempt = 0) const
		{
			return (
			    nlohmann::json::parse(_recording->requests.at(attempt).body));
		}

		RecordingTransport           *_recording = nullptr;
		std::optional<TypeSafeClient> _client;
};

TEST_F(WireContractTest, AlwaysSerializesTheDefaultModel)
{
	EXPECT_EQ(BodyOf().at("model"), "jev-latest");
}

TEST_F(WireContractTest, PerRequestModelOverridesTheDefault)
{
	SystemOneRequest request = Request();

	request.model = "jev-experimental";
	_recording->responses.push_back(
	    ValidResponse({{"safe", {{"type", "noul"}, {"noul", 0.9}}}}));
	_client->systemOne(request);
	EXPECT_EQ(BodyOf(1).at("model"), "jev-experimental");
}

TEST_F(WireContractTest, PreservesStructuredInstructions)
{
	EXPECT_TRUE(
	    BodyOf().at("questions").at("route").at("instructions").is_object());
}

TEST_F(WireContractTest, SerializesNoulCriteria)
{
	EXPECT_EQ(BodyOf().at("questions").at("safe").at("criteria").at("true"),
	          "Allowed");
}

TEST_F(WireContractTest, MergesExtraBodyFields)
{
	EXPECT_EQ(BodyOf().at("beta"), true);
}

TEST_F(WireContractTest, ForwardsPerRequestHeaders)
{
	EXPECT_EQ(_recording->requests.front().headers.at("X-Team"), "sdk");
}

TEST_F(WireContractTest, AuthenticatesAndIdentifiesItself)
{
	const auto &headers = _recording->requests.front().headers;

	EXPECT_EQ(headers.at("Authorization"), "Bearer test-key");
	EXPECT_EQ(headers.at("Accept"), "application/json");
	EXPECT_TRUE(headers.at("User-Agent").rfind("typesafe-sdk-cpp/", 0) == 0);
}

TEST_F(WireContractTest, RejectsEmptyQuestionsBeforeSendingAnything)
{
	SystemOneRequest empty;

	empty.state = "hello";
	EXPECT_THROW(_client->systemOne(empty), ValidationError);
	EXPECT_EQ(_recording->requests.size(), 1u);
}

TEST_F(WireContractTest, RejectsASingleLevelScoreBeforeSendingAnything)
{
	SystemOneRequest request;

	request.state = "hello";
	request.add("quality", Score{"Quality?", {"only"}});
	EXPECT_THROW(_client->systemOne(request), ValidationError);
	EXPECT_EQ(_recording->requests.size(), 1u);
}

TEST_F(WireContractTest, AcceptsATwoLevelScore)
{
	SystemOneRequest request;

	request.state = "hello";
	request.add("quality", Score{"Quality?", {"bad", "good"}});
	_recording->responses.push_back(
	    ValidResponse({{"quality",
		                {{"type", "score"},
		                 {"score", 0.5},
		                 {"confidence", 0.9},
		                 {"legend", {{"0", "bad"}, {"1", "good"}}},
		                 {"probabilities", {{"0", 0.5}, {"1", 0.5}}}}}}));
	EXPECT_NO_THROW(_client->systemOne(request));
	EXPECT_EQ(_recording->requests.size(), 2u);
}

TEST_F(WireContractTest, RejectsAScoreWithMoreThanTenLevels)
{
	SystemOneRequest            request;
	std::vector<nlohmann::json> levels;

	for (int level = 0; level < 11; ++level)
		levels.emplace_back("level " + std::to_string(level));
	request.state = "hello";
	request.add("quality", Score{"Quality?", levels});
	EXPECT_THROW(_client->systemOne(request), ValidationError);
	EXPECT_EQ(_recording->requests.size(), 1u);
}

TEST_F(WireContractTest, RejectsAChoiceWithMoreThan255Options)
{
	SystemOneRequest                                     request;
	std::map<std::string, std::optional<nlohmann::json>> options;

	for (int option = 0; option < 256; ++option)
		options.emplace("option " + std::to_string(option), std::nullopt);
	request.state = "hello";
	request.add("pick", Choice{"Pick one", options});
	EXPECT_THROW(_client->systemOne(request), ValidationError);
	EXPECT_EQ(_recording->requests.size(), 1u);
}

TEST_F(WireContractTest, RejectsAChoiceWithNoOptions)
{
	SystemOneRequest request;

	request.state = "hello";
	request.add("pick", Choice{"Pick one", {}});
	EXPECT_THROW(_client->systemOne(request), ValidationError);
	EXPECT_EQ(_recording->requests.size(), 1u);
}

template <typename Expected>
void ExpectHttpError(int status)
{
	auto transport = std::make_unique<RecordingTransport>();

	transport->responses.push_back(HttpResponse{status, "error", {}});
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .max_retries(0)
	                  .transport(std::move(transport))
	                  .build();
	EXPECT_THROW(client.systemOne(BasicRequest()), Expected);
}

TEST(ErrorMappingTest, Maps401ToAuthenticationError)
{
	ExpectHttpError<AuthenticationError>(401);
}

TEST(ErrorMappingTest, Maps422ToValidationError)
{
	ExpectHttpError<ValidationError>(422);
}

TEST(ErrorMappingTest, Maps429ToRateLimitError)
{
	ExpectHttpError<RateLimitError>(429);
}

TEST(ErrorMappingTest, Maps529ToAPIError)
{
	ExpectHttpError<APIError>(529);
}

TEST(RetryTest, RetriesRateLimitsThenThrowsTheTypedError)
{
	auto  transport = std::make_unique<RecordingTransport>();
	auto *recording = transport.get();

	for (int attempt = 0; attempt < 3; ++attempt)
		recording->responses.push_back(HttpResponse{429, "slow down", {}});
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .max_retries(2)
	                  .transport(std::move(transport))
	                  .build();
	EXPECT_THROW(client.systemOne(BasicRequest()), RateLimitError);
	EXPECT_EQ(recording->requests.size(), 3u);
}

TEST(RetryTest, DoesNotRetryClientErrors)
{
	auto  transport = std::make_unique<RecordingTransport>();
	auto *recording = transport.get();

	recording->responses.push_back(HttpResponse{422, "bad request", {}});
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .max_retries(5)
	                  .transport(std::move(transport))
	                  .build();
	EXPECT_THROW(client.systemOne(BasicRequest()), ValidationError);
	EXPECT_EQ(recording->requests.size(), 1u);
}

TEST(RetryTest, RetriesConnectionFailuresThenThrowsConnectionError)
{
	auto  transport = std::make_unique<RecordingTransport>();
	auto *recording = transport.get();
	auto  client = TypeSafeClient::builder()
	                   .api_key("test-key")
	                   .max_retries(1)
	                   .transport(std::move(transport))
	                   .build();
	EXPECT_THROW(client.systemOne(BasicRequest()), APIConnectionError);
	EXPECT_EQ(recording->requests.size(), 2u);
}

TEST(RetryTest, HonoursTheRetryAfterHeader)
{
	auto transport = std::make_unique<RecordingTransport>();

	transport->responses.push_back(
	    HttpResponse{429, "slow down", {{"retry-after-ms", "40"}}});
	transport->responses.push_back(
	    ValidResponse({{"safe", {{"type", "noul"}, {"noul", 0.9}}}}));
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .max_retries(1)
	                  .transport(std::move(transport))
	                  .build();

	const auto              started = std::chrono::steady_clock::now();
	const SystemOneResponse answered = client.systemOne(BasicRequest());
	const auto elapsed = std::chrono::steady_clock::now() - started;

	EXPECT_DOUBLE_EQ(answered.nouls.at("safe").noul, 0.9);
	const auto ms
	    = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
	          .count();
	EXPECT_GE(ms, 40);
	EXPECT_LT(ms, 900);
}

long long ElapsedMs(std::chrono::steady_clock::time_point started)
{
	return (std::chrono::duration_cast<std::chrono::milliseconds>(
	            std::chrono::steady_clock::now() - started)
	            .count());
}

TEST(RetryTest, StopsBeforeAWaitThatWouldReachTheBudget)
{
	auto  transport = std::make_unique<RecordingTransport>();
	auto *recording = transport.get();

	for (int attempt = 0; attempt < 4; ++attempt)
		recording->responses.push_back(
		    HttpResponse{429, "slow down", {{"retry-after", "30"}}});
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .max_retries(3)
	                  .retry_timeout(100)
	                  .transport(std::move(transport))
	                  .build();

	const auto started = std::chrono::steady_clock::now();
	EXPECT_THROW(client.systemOne(BasicRequest()), RateLimitError);
	EXPECT_LT(ElapsedMs(started), 150);
	EXPECT_EQ(recording->requests.size(), 1u);
}

TEST(RetryTest, BacksOffFromHalfASecondWithJitter)
{
	auto  transport = std::make_unique<RecordingTransport>();
	auto *recording = transport.get();

	recording->responses.push_back(HttpResponse{503, "busy", {}});
	recording->responses.push_back(
	    ValidResponse({{"safe", {{"type", "noul"}, {"noul", 0.9}}}}));
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .max_retries(1)
	                  .transport(std::move(transport))
	                  .build();

	const auto started = std::chrono::steady_clock::now();
	client.systemOne(BasicRequest());
	const long long ms = ElapsedMs(started);

	// 500 ms less up to 25% jitter.
	EXPECT_GE(ms, 370);
	EXPECT_LT(ms, 700);
}

TEST(RetryTest, StopsWhenARetryAfterWouldPassTheBudget)
{
	auto  transport = std::make_unique<RecordingTransport>();
	auto *recording = transport.get();

	recording->responses.push_back(
	    HttpResponse{429, "slow down", {{"retry-after", "120"}}});
	recording->responses.push_back(
	    ValidResponse({{"safe", {{"type", "noul"}, {"noul", 0.9}}}}));
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .max_retries(1)
	                  .transport(std::move(transport))
	                  .build();

	const auto started = std::chrono::steady_clock::now();
	EXPECT_THROW(client.systemOne(BasicRequest()), RateLimitError);
	EXPECT_LT(ElapsedMs(started), 150);
	EXPECT_EQ(recording->requests.size(), 1u);
}

TEST(RetryTest, KeepsTheFullAttemptTimeoutWithinABudget)
{
	auto  transport = std::make_unique<RecordingTransport>();
	auto *recording = transport.get();

	recording->responses.push_back(
	    ValidResponse({{"safe", {{"type", "noul"}, {"noul", 0.9}}}}));
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .timeout(60000)
	                  .retry_timeout(2000)
	                  .transport(std::move(transport))
	                  .build();

	client.systemOne(BasicRequest());
	EXPECT_EQ(recording->requests.front().timeout_ms, 60000);
}

TEST(RetryTest, WithoutABudgetStopsBeforeAWaitOverAMinute)
{
	auto  transport = std::make_unique<RecordingTransport>();
	auto *recording = transport.get();

	recording->responses.push_back(
	    HttpResponse{429, "slow down", {{"retry-after", "86400"}}});
	recording->responses.push_back(
	    ValidResponse({{"safe", {{"type", "noul"}, {"noul", 0.9}}}}));
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .max_retries(1)
	                  .retry_timeout(0)
	                  .transport(std::move(transport))
	                  .build();

	const auto started = std::chrono::steady_clock::now();
	EXPECT_THROW(client.systemOne(BasicRequest()), RateLimitError);
	EXPECT_LT(ElapsedMs(started), 150);
	EXPECT_EQ(recording->requests.size(), 1u);
}

TEST(RetryTest, WithoutABudgetStillHonoursAShortRetryAfter)
{
	auto  transport = std::make_unique<RecordingTransport>();
	auto *recording = transport.get();

	recording->responses.push_back(
	    HttpResponse{429, "slow down", {{"retry-after-ms", "40"}}});
	recording->responses.push_back(
	    ValidResponse({{"safe", {{"type", "noul"}, {"noul", 0.9}}}}));
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .max_retries(1)
	                  .retry_timeout(0)
	                  .transport(std::move(transport))
	                  .build();

	EXPECT_NO_THROW(client.systemOne(BasicRequest()));
	EXPECT_EQ(recording->requests.size(), 2u);
}

TEST(RetryTest, RejectsANegativeBudget)
{
	EXPECT_THROW(TypeSafeClient::builder()
	                 .api_key("test-key")
	                 .retry_timeout(-1)
	                 .transport(std::make_unique<RecordingTransport>())
	                 .build(),
	             ValidationError);
}

TEST(ErrorDetailTest, ReadsTheValidationDetailList)
{
	auto transport = std::make_unique<RecordingTransport>();

	transport->responses.push_back(HttpResponse{
	    422,
	    R"({"detail":[{"loc":["body","questions","urgency","criteria"],)"
	    R"("msg":"Field required","type":"missing"}]})",
	    {}});
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .transport(std::move(transport))
	                  .build();

	try
	{
		client.systemOne(BasicRequest());
		FAIL() << "expected ValidationError";
	}
	catch (const ValidationError &error)
	{
		EXPECT_NE(std::string(error.what())
		              .find("body.questions.urgency.criteria: Field required"),
		          std::string::npos);
	}
}

TEST(ErrorDetailTest, ReadsAnOpenRouterErrorMessage)
{
	auto transport = std::make_unique<RecordingTransport>();

	transport->responses.push_back(HttpResponse{
	    402, R"({"error":{"code":402,"message":"Insufficient credits"}})", {}});
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .transport(std::move(transport))
	                  .build();

	try
	{
		client.systemOne(BasicRequest());
		FAIL() << "expected APIError";
	}
	catch (const APIError &error)
	{
		EXPECT_EQ(std::string(error.what()),
		          "API returned HTTP 402: Insufficient credits");
	}
}

TEST(ErrorDetailTest, KeepsABodyThatIsNotJson)
{
	auto transport = std::make_unique<RecordingTransport>();

	transport->responses.push_back(HttpResponse{400, "plain text", {}});
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .transport(std::move(transport))
	                  .build();

	try
	{
		client.systemOne(BasicRequest());
		FAIL() << "expected APIError";
	}
	catch (const APIError &error)
	{
		EXPECT_EQ(std::string(error.what()),
		          "API returned HTTP 400: plain text");
	}
}

TEST(OpenRouterTest, SendsToOpenRoutersSystemOneEndpointWithJev)
{
	auto  transport = std::make_unique<RecordingTransport>();
	auto *recording = transport.get();

	recording->responses.push_back(
	    ValidResponse({{"safe", {{"type", "noul"}, {"noul", 0.9}}}}));
	auto client = TypeSafeClient::builder()
	                  .openrouter("k")
	                  .transport(std::move(transport))
	                  .build();

	client.systemOne(BasicRequest());
	const HttpRequest &sent = recording->requests.front();

	EXPECT_EQ(sent.url, "https://openrouter.ai/api/v1/systemone");
	EXPECT_EQ(nlohmann::json::parse(sent.body).at("model"),
	          "typesafe/jev-1.13");
	EXPECT_EQ(sent.headers.at("Authorization"), "Bearer k");
}

TEST(OpenRouterTest, ModelCalledAfterThePresetOverridesIt)
{
	auto  transport = std::make_unique<RecordingTransport>();
	auto *recording = transport.get();

	recording->responses.push_back(
	    ValidResponse({{"safe", {{"type", "noul"}, {"noul", 0.9}}}}));
	auto client = TypeSafeClient::builder()
	                  .openrouter("k")
	                  .model("typesafe/jev-next")
	                  .transport(std::move(transport))
	                  .build();

	client.systemOne(BasicRequest());
	EXPECT_EQ(
	    nlohmann::json::parse(recording->requests.front().body).at("model"),
	    "typesafe/jev-next");
}

TEST(OpenRouterTest, BaseUrlCalledAfterThePresetLeavesOpenRouterMode)
{
	auto  transport = std::make_unique<RecordingTransport>();
	auto *recording = transport.get();

	recording->responses.push_back(ModelsResponse());
	auto client = TypeSafeClient::builder()
	                  .openrouter("k")
	                  .base_url("https://api.typesafe.ai")
	                  .transport(std::move(transport))
	                  .build();

	EXPECT_NO_THROW(client.listModels());
	EXPECT_EQ(recording->requests.front().url,
	          "https://api.typesafe.ai/v1/models");
}

TEST(OpenRouterTest, ListModelsThrowsWithoutSendingAnything)
{
	auto  transport = std::make_unique<RecordingTransport>();
	auto *recording = transport.get();
	auto  client = TypeSafeClient::builder()
	                   .openrouter("k")
	                   .transport(std::move(transport))
	                   .build();

	EXPECT_THROW(client.listModels(), ValidationError);
	EXPECT_TRUE(recording->requests.empty());
}

class ListModelsTest : public ::testing::Test
{
	protected:
		void SetUp() override
		{
			auto transport = std::make_unique<RecordingTransport>();

			_recording = transport.get();
			_recording->responses.push_back(ModelsResponse());
			_client = TypeSafeClient::builder()
			              .api_key("test-key")
			              .transport(std::move(transport))
			              .build();
		}

		RecordingTransport           *_recording = nullptr;
		std::optional<TypeSafeClient> _client;
};

TEST_F(ListModelsTest, SendsAGetToTheModelsPathWithoutABody)
{
	_client->listModels();
	const HttpRequest &sent = _recording->requests.front();

	EXPECT_EQ(sent.method, "GET");
	EXPECT_EQ(sent.url, "https://api.typesafe.ai/v1/models");
	EXPECT_EQ(sent.headers.at("Authorization"), "Bearer test-key");
	EXPECT_EQ(sent.headers.count("Content-Type"), 0u);
	EXPECT_TRUE(sent.body.empty());
}

TEST_F(ListModelsTest, DecodesAliasesAndVersionedModels)
{
	const ListModelsResponse listed = _client->listModels();

	ASSERT_EQ(listed.models.size(), 2u);
	EXPECT_EQ(listed.models.front().name, "jev-latest");
	EXPECT_EQ(listed.models.front().release_date, "2026-09-15");
	EXPECT_EQ(listed.models.back().name, "jev-1.13.0");
	EXPECT_FALSE(listed.models.back().description.empty());
}

TEST_F(ListModelsTest, RejectsAModelEntryMissingARequiredField)
{
	_recording->responses.clear();
	_recording->responses.push_back(HttpResponse{
	    200,
	    nlohmann::json{{"models", nlohmann::json::array({nlohmann::json{
	                                  {"name", "jev-latest"},
	                                  {"description", "no release date"}}})}}
	        .dump(),
	    {}});
	EXPECT_THROW(_client->listModels(), ValidationError);
}

TEST(ResponseValidationTest, ReportsAMalformedSuccessResponse)
{
	auto transport = std::make_unique<RecordingTransport>();

	transport->responses.push_back(
	    ValidResponse({{"quality", {{"type", "score"}, {"score", 1.7}}}}));
	auto             client = TypeSafeClient::builder()
	                              .api_key("test-key")
	                              .transport(std::move(transport))
	                              .build();
	SystemOneRequest request;

	request.state = "hello";
	request.add("quality", Score{"Quality?", {"bad", "ok"}});
	EXPECT_THROW(client.systemOne(request), ValidationError);
}

}  // namespace
