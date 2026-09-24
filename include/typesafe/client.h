// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#pragma once

#include <future>
#include <memory>
#include <optional>
#include <string>

#include "typesafe/transport.h"
#include "typesafe/types.h"
#include "typesafe/typesafe_error.h"

namespace typesafe
{

struct TypeSafeClientConfig
{
		std::optional<std::string> api_key;
		std::string                base_url = "https://api.typesafe.ai";
		std::string                default_model = "jev-latest";
		int                        timeout_ms = 10000;
		int                        max_retries = 2;
		int                        retry_timeout_ms = 30000;
		bool                       openrouter = false;
};

class TypeSafeClient;

/**
 * @brief Instantiates a TypeSafeClient.
 *
 * Automatically falls back to environment variables (e.g. TYPESAFE_API_KEY)
 * if values are omitted.
 */
class TypeSafeClientBuilder
{
	public:
		TypeSafeClientBuilder();

		/**
		 * @brief Sets the API key.
		 */
		TypeSafeClientBuilder &api_key(const std::string &key);

		/**
		 * @brief Overrides the default base URL (https://api.typesafe.ai).
		 */
		TypeSafeClientBuilder &base_url(const std::string &url);

		TypeSafeClientBuilder &model(const std::string &model);

		/**
		 * @brief Sets the timeout for each attempt in milliseconds (default:
		 * 10000); SystemOneRequest::timeout_ms overrides it per request.
		 */
		TypeSafeClientBuilder &timeout(int ms);

		/**
		 * @brief Sets the maximum number of automatic retries (default: 2).
		 */
		TypeSafeClientBuilder &max_retries(int retries);

		/**
		 * @brief Sets the total time budget for one call in milliseconds,
		 * counting every attempt and every wait between them (default: 30000;
		 * 0 disables the limit).
		 *
		 * No retry starts when the time spent plus its wait would reach the
		 * budget; the call then throws the last attempt's error. A running
		 * attempt keeps its full timeout. With the limit disabled, a wait
		 * longer than 60 s also ends the call.
		 */
		TypeSafeClientBuilder &retry_timeout(int ms);

		/**
		 * @brief Experimental: sends System One requests through OpenRouter.
		 *
		 * Sets the API key, the base URL https://openrouter.ai/api (so calls
		 * go to https://openrouter.ai/api/v1/systemone) and the model
		 * "typesafe/jev-1.13". Call model() afterwards to pick another model;
		 * calling base_url() afterwards leaves OpenRouter mode.
		 * listModels() throws ValidationError in this mode, because
		 * OpenRouter's /v1/models is a different endpoint.
		 */
		TypeSafeClientBuilder &openrouter(const std::string &key);

		/**
		 * @brief Injects a custom HTTP transport engine.
		 *
		 * Replaces the default libcurl implementation.
		 */
		TypeSafeClientBuilder &transport(std::unique_ptr<Transport> transport);

		/**
		 * @brief Finalizes configuration and instantiates the client.
		 *
		 * @throws AuthenticationError if the API key is missing, or contains
		 * whitespace, control, or non-ASCII characters after trimming.
		 * @throws ValidationError if the base URL is not absolute http(s), the
		 * model is empty, the timeout is not positive, or max_retries or
		 * retry_timeout is negative.
		 * @throws TypeSafeError if no transport is injected and libcurl is
		 * disabled.
		 */
		TypeSafeClient build();

	private:
		TypeSafeClientConfig       _config;
		std::unique_ptr<Transport> _transport;
};

/**
 * @brief The main SDK client for TypeSafe.
 */
class TypeSafeClient
{
	public:
		static TypeSafeClientBuilder builder();

		/**
		 * @brief Internal constructor used by the builder.
		 */
		TypeSafeClient(TypeSafeClientConfig       config,
		               std::unique_ptr<Transport> transport);

		~TypeSafeClient();
		TypeSafeClient(const TypeSafeClient &) = delete;
		TypeSafeClient &operator=(const TypeSafeClient &) = delete;
		TypeSafeClient(TypeSafeClient &&) = default;
		TypeSafeClient &operator=(TypeSafeClient &&) = default;

		/**
		 * @brief Evaluates a SystemOne Request.
		 * @throws TypeSafeError on network, authentication, or API failure.
		 */
		SystemOneResponse systemOne(const SystemOneRequest &request) const;

		/**
		 * @brief Evaluates a SystemOne Request asynchronously.
		 * @returns A std::future containing the SystemOneResponse.
		 */
		std::future<SystemOneResponse> systemOneAsync(
		    const SystemOneRequest &request) const;

		/**
		 * @brief Evaluates a SystemOne Request and parses the response into a
		 * custom model.
		 * @tparam T The custom model struct (must be compatible with
		 * nlohmann::json).
		 * @throws TypeSafeError on network, authentication, or API failure.
		 */
		template <typename T>
		T systemOneAs(const SystemOneRequest &request) const
		{
			const HttpResponse res = executeSystemOne(request);
			try
			{
				const auto res_json = nlohmann::json::parse(res.body);
				return (res_json.get<T>());
			}
			catch (const nlohmann::json::exception &e)
			{
				throw ValidationError(std::string("Invalid JSON response: ")
				                      + e.what());
			}
		}

		/**
		 * @brief Evaluates a SystemOne Request asynchronously into a custom
		 * model.
		 * @tparam T The custom model struct.
		 * @returns A std::future containing the parsed custom model.
		 */
		template <typename T>
		std::future<T> systemOneAsAsync(const SystemOneRequest &request) const
		{
			auto config = _config;
			auto transport = _transport;
			return std::async(
			    std::launch::async,
			    [config = std::move(config), transport = std::move(transport),
				 request]() mutable
			    {
				    TypeSafeClient client(std::move(config),
					                      std::move(transport));
				    return client.systemOneAs<T>(request);
			    });
		}

		/**
		 * @brief Lists available models.
		 * @throws TypeSafeError on network, authentication, or API failure.
		 */
		ListModelsResponse listModels() const;

		/**
		 * @brief Lists available models asynchronously.
		 * @throws TypeSafeError on network, authentication, or API failure.
		 */
		std::future<ListModelsResponse> listModelsAsync() const;

	private:
		TypeSafeClientConfig       _config;
		std::shared_ptr<Transport> _transport;

		TypeSafeClient(TypeSafeClientConfig       config,
		               std::shared_ptr<Transport> transport);

		HttpResponse executeSystemOne(const SystemOneRequest &request) const;

		HttpResponse sendRequest(
		    const std::string                        &method,
		    const std::string                        &url,
		    const std::optional<std::string>         &body,
		    int                                       timeout_ms,
		    const std::map<std::string, std::string> &extra_headers = {}) const;
};

}  // namespace typesafe
