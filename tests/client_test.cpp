// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "typesafe/client.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "fake_transport.h"

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
