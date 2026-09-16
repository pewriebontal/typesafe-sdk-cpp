// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "typesafe/client.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <thread>

#ifdef TYPESAFE_USE_LIBCURL
#include "typesafe/curl_transport.h"
#endif

namespace typesafe
{

constexpr const char *kSystemOnePath = "/v1/systemone";
constexpr const char *kModelsPath = "/v1/models";
constexpr const char *kSdkName = "typesafe-sdk-cpp";
constexpr const char *kSdkVersion = TYPESAFE_SDK_VERSION;
constexpr const char *kRuntime = "C++20";

bool isJsonContent(const nlohmann::json &value, bool allow_null)
{
	return value.is_string() || value.is_object() || value.is_array()
	       || (allow_null && value.is_null());
}

TypeSafeClientBuilder::TypeSafeClientBuilder()
{
	if (const char *env_key = std::getenv("TYPESAFE_API_KEY"))
		_config.api_key = env_key;
	if (const char *env_url = std::getenv("TYPESAFE_BASE_URL"))
		_config.base_url = env_url;
	if (const char *env_model = std::getenv("TYPESAFE_DEFAULT_MODEL"))
		_config.default_model = env_model;
}

TypeSafeClientBuilder &TypeSafeClientBuilder::api_key(const std::string &key)
{
	_config.api_key = key;
	return (*this);
}

TypeSafeClientBuilder &TypeSafeClientBuilder::base_url(const std::string &url)
{
	_config.base_url = url;
	return (*this);
}

TypeSafeClientBuilder &TypeSafeClientBuilder::model(const std::string &model)
{
	_config.default_model = model;
	return (*this);
}

TypeSafeClientBuilder &TypeSafeClientBuilder::timeout(int ms)
{
	_config.timeout_ms = ms;
	return (*this);
}

TypeSafeClientBuilder &TypeSafeClientBuilder::max_retries(int retries)
{
	_config.max_retries = retries;
	return (*this);
}

TypeSafeClientBuilder &TypeSafeClientBuilder::transport(
    std::unique_ptr<Transport> transport)
{
	_transport = std::move(transport);
	return (*this);
}

TypeSafeClient TypeSafeClientBuilder::build()
{
	if (!_config.api_key || _config.api_key->empty())
		throw AuthenticationError("TYPESAFE_API_KEY is not set.");
	if (_config.base_url.empty())
		throw ValidationError("Base URL must not be empty.");
	while (_config.base_url.size() > 1 && _config.base_url.back() == '/')
		_config.base_url.pop_back();
	if (_config.default_model.empty())
		throw ValidationError("Default model must not be empty.");
	if (_config.timeout_ms <= 0)
		throw ValidationError("Timeout must be greater than zero.");
	if (_config.max_retries < 0)
		throw ValidationError("Maximum retries must not be negative.");

	if (!_transport)
	{
#ifdef TYPESAFE_USE_LIBCURL
		_transport = std::make_unique<CurlTransport>();
#else
		throw TypeSafeError(
		    "No transport injected and TYPESAFE_USE_LIBCURL is OFF.");
#endif
	}

	return TypeSafeClient(std::move(_config), std::move(_transport));
}

TypeSafeClientBuilder TypeSafeClient::builder()
{
	return TypeSafeClientBuilder();
}

TypeSafeClient::TypeSafeClient(TypeSafeClientConfig       config,
                               std::unique_ptr<Transport> transport)
    : _config(std::move(config)), _transport(std::move(transport))
{
}

TypeSafeClient::TypeSafeClient(TypeSafeClientConfig       config,
                               std::shared_ptr<Transport> transport)
    : _config(std::move(config)), _transport(std::move(transport))
{
}

TypeSafeClient::~TypeSafeClient() = default;

HttpResponse TypeSafeClient::sendRequest(
    const std::string                        &method,
    const std::string                        &path,
    const std::optional<std::string>         &body,
    int                                       timeout_ms,
    const std::map<std::string, std::string> &extra_headers) const
{
	if (!_transport)
		throw APIConnectionError("No transport provided to TypeSafeClient.");

	HttpRequest req;
	req.method = method;
	req.url = _config.base_url + path;
	req.timeout_ms = timeout_ms;

	req.headers = extra_headers;
	req.headers["Authorization"] = "Bearer " + *_config.api_key;
	req.headers["Accept"] = "application/json";
	req.headers["User-Agent"] = std::string(kSdkName) + "/" + kSdkVersion;
	req.headers["X-TypeSafe-SDK"] = std::string(kSdkName) + "/" + kSdkVersion;
	req.headers["X-TypeSafe-Runtime"] = kRuntime;

	if (body)
	{
		req.headers["Content-Type"] = "application/json";
		req.body = *body;
	}

	int attempt = 0;
	int backoff_ms = 1000;

	while (true)
	{
		std::optional<HttpResponse> response;
		try
		{
			response = _transport->request(req);
		}
		catch (const std::exception &e)
		{
			if (attempt >= _config.max_retries)
				throw APIConnectionError(std::string("Connection failed: ")
				                         + e.what());
		}

		int delay_ms = backoff_ms;
		if (response)
		{
			const HttpResponse &res = *response;
			if (res.status_code >= 200 && res.status_code < 300)
				return res;
			if (res.status_code == 401 || res.status_code == 403)
				throw AuthenticationError("Authentication failed: " + res.body);

			const bool retryable
			    = res.status_code == 408 || res.status_code == 429
			      || (res.status_code >= 500 && res.status_code <= 599);
			if (!retryable || attempt >= _config.max_retries)
			{
				if (res.status_code == 429)
					throw RateLimitError("Rate limit exceeded: " + res.body);
				if (res.status_code == 422)
					throw ValidationError(
					    "Request failed validation (HTTP 422): " + res.body);
				throw APIError("API returned HTTP "
				               + std::to_string(res.status_code) + ": "
				               + res.body);
			}

			auto header_value =
			    [&res](const std::string &wanted) -> std::optional<std::string>
			{
				for (const auto &[name, value] : res.headers)
				{
					if (name.size() != wanted.size())
						continue;
					bool equal = true;
					for (std::size_t i = 0; i < name.size(); ++i)
						equal = equal
						        && std::tolower(
						               static_cast<unsigned char>(name[i]))
						               == wanted[i];
					if (equal)
						return value;
				}
				return std::nullopt;
			};
			try
			{
				if (auto milliseconds = header_value("retry-after-ms"))
				{
					const double value = std::stod(*milliseconds);
					if (std::isfinite(value) && value >= 0.0)
						delay_ms = static_cast<int>(std::min(value, 60000.0));
				}
				else if (auto seconds = header_value("retry-after"))
				{
					const double value = std::stod(*seconds) * 1000.0;
					if (std::isfinite(value) && value >= 0.0)
						delay_ms = static_cast<int>(std::min(value, 60000.0));
				}
			}
			catch (const std::exception &)
			{
			}
		}

		attempt++;
		std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
		backoff_ms = std::min(backoff_ms * 2, 60000);
	}
}

HttpResponse TypeSafeClient::executeSystemOne(
    const SystemOneRequest &request) const
{
	if (!(request.state.is_string() || request.state.is_object()
	      || request.state.is_array()))
		throw ValidationError("State must be a string, object, or array.");
	if (request.questions.empty())
		throw ValidationError("At least one question is required.");
	for (const auto &[name, question] : request.questions)
	{
		if (!question.is_object() || !question.contains("type")
		    || !question.at("type").is_string())
			throw ValidationError("Question '" + name
			                      + "' must have a string type.");
		const std::string type = question.at("type").get<std::string>();
		if (type != "noul" && type != "choice" && type != "score")
			throw ValidationError("Question '" + name
			                      + "' has an unsupported type.");
		if (question.contains("instructions")
		    && !isJsonContent(question.at("instructions"), true))
			throw ValidationError("Question '" + name
			                      + "' has invalid instructions.");
		if (type == "noul" && question.contains("criteria")
		    && !question.at("criteria").is_null())
		{
			const auto &criteria = question.at("criteria");
			if (!criteria.is_object())
				throw ValidationError("Noul question '" + name
				                      + "' has invalid criteria.");
			for (const char *key : {"true", "false"})
				if (criteria.contains(key)
				    && !isJsonContent(criteria.at(key), true))
					throw ValidationError("Noul question '" + name
					                      + "' has invalid criteria.");
		}
		if (type == "choice")
		{
			if (!question.contains("criteria")
			    || !question.at("criteria").is_object())
				throw ValidationError("Choice question '" + name
				                      + "' requires object criteria.");
			for (const auto &[key, value] : question.at("criteria").items())
				if (!isJsonContent(value, true))
					throw ValidationError("Choice criterion '" + key
					                      + "' has an invalid description.");
		}
		if (type == "score")
		{
			if (!question.contains("criteria")
			    || !question.at("criteria").is_array()
			    || question.at("criteria").empty())
				throw ValidationError("Score question '" + name
				                      + "' requires non-empty array criteria.");
			for (const auto &value : question.at("criteria"))
				if (!isJsonContent(value, false))
					throw ValidationError("Score question '" + name
					                      + "' has invalid criteria.");
		}
	}

	nlohmann::json payload;
	payload["state"] = request.state;
	payload["model"] = request.model.value_or(_config.default_model);
	if (payload["model"].get_ref<const std::string &>().empty())
		throw ValidationError("Model must not be empty.");

	nlohmann::json questions_json;
	for (const auto &[key, q] : request.questions)
		questions_json[key] = q;
	payload["questions"] = questions_json;

	for (const auto &[key, val] : request.extra_body)
		payload[key] = val;

	int timeout_ms = _config.timeout_ms;
	if (request.timeout_ms)
		timeout_ms = *request.timeout_ms;

	const std::string body_str = payload.dump();
	if (timeout_ms <= 0)
		throw ValidationError("Timeout must be greater than zero.");
	return sendRequest("POST", kSystemOnePath, body_str, timeout_ms,
	                   request.extra_headers);
}

SystemOneResponse TypeSafeClient::systemOne(
    const SystemOneRequest &request) const
{
	return systemOneAs<SystemOneResponse>(request);
}

std::future<SystemOneResponse> TypeSafeClient::systemOneAsync(
    const SystemOneRequest &request) const
{
	return systemOneAsAsync<SystemOneResponse>(request);
}

ListModelsResponse TypeSafeClient::listModels() const
{
	HttpResponse res
	    = sendRequest("GET", kModelsPath, std::nullopt, _config.timeout_ms);

	try
	{
		const auto res_json = nlohmann::json::parse(res.body);
		return (res_json.get<ListModelsResponse>());
	}
	catch (const nlohmann::json::exception &e)
	{
		throw ValidationError(std::string("Invalid JSON response: ")
		                      + e.what());
	}
}

std::future<ListModelsResponse> TypeSafeClient::listModelsAsync() const
{
	auto config = _config;
	auto transport = _transport;
	return std::async(
	    std::launch::async,
	    [config = std::move(config), transport = std::move(transport)]() mutable
	    {
		    TypeSafeClient client(std::move(config), std::move(transport));
		    return client.listModels();
	    });
}

}  // namespace typesafe
