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
		 * @brief Explicitly sets the API key used for authentication.
		 */
		TypeSafeClientBuilder &api_key(const std::string &key);

		/**
		 * @brief Overrides the default base URL (https://api.typesafe.ai).
		 */
		TypeSafeClientBuilder &base_url(const std::string &url);

		TypeSafeClientBuilder &model(const std::string &model);

		/**
		 * @brief Sets the global timeout for all requests in milliseconds.
		 */
		TypeSafeClientBuilder &timeout(int ms);

		/**
		 * @brief Sets the maximum number of automatic retries (default: 2).
		 */
		TypeSafeClientBuilder &max_retries(int retries);

		/**
		 * @brief Injects a custom HTTP transport engine.
		 *
		 * Replaces the default libcurl implementation.
		 */
		TypeSafeClientBuilder &transport(std::unique_ptr<Transport> transport);

		/**
		 * @brief Finalizes configuration and instantiates the client.
		 *
		 * @throws AuthenticationError if no API key is provided or found in
		 * env.
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
		    const std::string                        &path,
		    const std::optional<std::string>         &body,
		    int                                       timeout_ms,
		    const std::map<std::string, std::string> &extra_headers = {}) const;
};

}  // namespace typesafe
