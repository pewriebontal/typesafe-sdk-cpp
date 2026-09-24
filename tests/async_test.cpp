// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include <gtest/gtest.h>

#include <future>
#include <memory>

#include "typesafe/client.h"
#include "typesafe/testing.h"

namespace
{

using namespace typesafe;
using typesafe::testing::BasicRequest;
using typesafe::testing::ModelsResponse;
using typesafe::testing::RecordingTransport;
using typesafe::testing::ValidResponse;

TEST(AsyncTest, ReturnsTheParsedResponse)
{
	auto transport = std::make_unique<RecordingTransport>();

	transport->responses.push_back(
	    ValidResponse({{"safe", {{"type", "noul"}, {"noul", 0.75}}}}));
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .transport(std::move(transport))
	                  .build();

	EXPECT_DOUBLE_EQ(
	    client.systemOneAsync(BasicRequest()).get().nouls.at("safe").noul,
	    0.75);
}

TEST(AsyncTest, TheFutureOutlivesTheClientThatMadeIt)
{
	std::future<SystemOneResponse> future;
	{
		auto transport = std::make_unique<RecordingTransport>();

		transport->delay = true;
		transport->responses.push_back(
		    ValidResponse({{"safe", {{"type", "noul"}, {"noul", 0.75}}}}));
		auto client = TypeSafeClient::builder()
		                  .api_key("test-key")
		                  .transport(std::move(transport))
		                  .build();
		future = client.systemOneAsync(BasicRequest());
	}
	EXPECT_DOUBLE_EQ(future.get().nouls.at("safe").noul, 0.75);
}

TEST(ListModelsAsyncTest, ReturnsTheParsedList)
{
	auto transport = std::make_unique<RecordingTransport>();

	transport->responses.push_back(ModelsResponse());
	auto client = TypeSafeClient::builder()
	                  .api_key("test-key")
	                  .transport(std::move(transport))
	                  .build();

	const ListModelsResponse listed = client.listModelsAsync().get();

	ASSERT_EQ(listed.models.size(), 2u);
	EXPECT_EQ(listed.models.front().name, "jev-latest");
}

}  // namespace
