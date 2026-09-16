// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#pragma once

#include <chrono>
#include <nlohmann/json.hpp>
#include <thread>
#include <utility>
#include <vector>

#include "typesafe/client.h"

namespace typesafe::testing
{

/**
 * @brief A transport that records every request and plays back a queue of
 * canned responses, so the tests assert on exactly what would have gone on
 * the wire without anything leaving the process.
 *
 * An empty queue makes the request throw, which stands in for a connection
 * failure; `delay` holds each request briefly so asynchronous tests can
 * prove a future outlives the client that made it.
 */
struct RecordingTransport final : Transport
{
		std::vector<HttpResponse> responses;
		std::vector<HttpRequest>  requests;
		bool                      delay = false;

		HttpResponse request(const HttpRequest &request) override
		{
			requests.push_back(request);
			if (delay)
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
			if (responses.empty())
				throw std::runtime_error(
				    "connection failed (recording transport)");
			HttpResponse response = responses.front();

			responses.erase(responses.begin());
			return (response);
		}
};

/** @brief A well-formed 200 carrying the given answers object. */
inline HttpResponse ValidResponse(nlohmann::json answers)
{
	return HttpResponse{
	    200,
	    nlohmann::json{
	        {"model", "jev-1.13.0"},
	        {"answers", std::move(answers)},
	        {"usage", {{"input_tokens", 120}, {"output_tokens", 12}}}}
	        .dump(),
	    {}};
}

/** @brief A well-formed 200 from GET /v1/models, per the documented example. */
inline HttpResponse ModelsResponse()
{
	nlohmann::json alias
	    = nlohmann::json{{"name", "jev-latest"},
	                     {"description", "General-purpose system one model."},
	                     {"release_date", "2026-09-15"}};
	nlohmann::json versioned
	    = nlohmann::json{{"name", "jev-1.13.0"},
	                     {"description", "Versioned System One model."},
	                     {"release_date", "2026-09-15"}};

	return (HttpResponse{
	    200,
	    nlohmann::json{{"models", nlohmann::json::array({alias, versioned})}}
	        .dump(),
	    {}});
}

/** @brief The smallest request that passes client-side validation. */
inline SystemOneRequest BasicRequest()
{
	SystemOneRequest request;

	request.state = "hello";
	request.add("safe", Noul{"Is this safe?", std::nullopt});
	return (request);
}

}  // namespace typesafe::testing
