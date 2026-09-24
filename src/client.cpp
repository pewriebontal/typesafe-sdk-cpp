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
#include <random>
#include <thread>

#ifdef TYPESAFE_USE_LIBCURL
#include "typesafe/curl_transport.h"
#endif

namespace typesafe
{

constexpr const char *kSystemOnePath = "/v1/systemone";
constexpr const char *kModelsPath = "/v1/models";
constexpr const char *kOpenRouterBaseUrl = "https://openrouter.ai/api";
constexpr const char *kOpenRouterModel = "typesafe/jev-1.13";
constexpr int         kBackoffInitialMs = 500;
constexpr int         kBackoffMaxMs = 5000;
constexpr double      kBackoffJitter = 0.25;
constexpr std::size_t kMaxChoiceOptions = 255;
constexpr std::size_t kMaxScoreLevels = 10;
constexpr int         kMaxUnbudgetedWaitMs = 60000;
constexpr const char *kSdkName = "typesafe-sdk-cpp";
constexpr const char *kSdkVersion = TYPESAFE_SDK_VERSION;
constexpr const char *kRuntime = "C++20";

bool isJsonContent(const nlohmann::json &value, bool allow_null)
{
	return value.is_string() || value.is_object() || value.is_array()
	       || (allow_null && value.is_null());
}

std::string trimmed(const std::string &text)
{
	const std::size_t first = text.find_first_not_of(" \t\r\n\f\v");
	if (first == std::string::npos)
		return ("");
	const std::size_t last = text.find_last_not_of(" \t\r\n\f\v");
	return (text.substr(first, last - first + 1));
}

std::optional<std::string> readEnv(const char *name)
{
	const char *raw = std::getenv(name);
	if (raw == nullptr)
		return (std::nullopt);
	std::string value = trimmed(raw);
	if (value.empty())
		return (std::nullopt);
	return (value);
}

/**
 * @brief The readable part of an error body: TypeSafe's 422 `detail` list
 * ("loc: msg; ..."), or OpenRouter's `error.message`; else the raw body.
 */
std::string errorDetail(const std::string &body)
{
	try
	{
		const auto parsed = nlohmann::json::parse(body);
		if (parsed.contains("detail") && parsed.at("detail").is_array())
		{
			std::string joined;
			for (const auto &issue : parsed.at("detail"))
			{
				std::string location;
				if (issue.contains("loc") && issue.at("loc").is_array())
					for (const auto &part : issue.at("loc"))
					{
						if (!location.empty())
							location += '.';
						location += part.is_string() ? part.get<std::string>()
						                             : part.dump();
					}
				if (!joined.empty())
					joined += "; ";
				if (!location.empty())
					joined += location + ": ";
				joined += issue.value("msg", issue.dump());
			}
			if (!joined.empty())
				return (joined);
		}
		if (parsed.contains("detail") && parsed.at("detail").is_string())
			return (parsed.at("detail").get<std::string>());
		if (parsed.contains("error") && parsed.at("error").is_object()
		    && parsed.at("error").contains("message")
		    && parsed.at("error").at("message").is_string())
			return (parsed.at("error").at("message").get<std::string>());
	}
	catch (const nlohmann::json::exception &)
	{
	}
	return (body);
}

[[noreturn]] void throwForStatus(const HttpResponse &res)
{
	const std::string detail = errorDetail(res.body);

	if (res.status_code == 401 || res.status_code == 403)
		throw AuthenticationError("Authentication failed: " + detail);
	if (res.status_code == 429)
		throw RateLimitError("Rate limit exceeded: " + detail);
	if (res.status_code == 422)
		throw ValidationError("Request failed validation (HTTP 422): "
		                      + detail);
	throw APIError("API returned HTTP " + std::to_string(res.status_code) + ": "
	               + detail);
}

int backoffDelayMs(int retry)
{
	thread_local std::mt19937              random(std::random_device{}());
	std::uniform_real_distribution<double> jitter(0.0, kBackoffJitter);
	const int       exponent = std::clamp(retry - 1, 0, 30);
	const long long exponential = std::min<long long>(
	    static_cast<long long>(kBackoffInitialMs) << exponent, kBackoffMaxMs);

	return (static_cast<int>(static_cast<double>(exponential)
	                         * (1.0 - jitter(random))));
}

std::optional<int> retryAfterMs(const HttpResponse &res)
{
	auto header_value
	    = [&res](const std::string &wanted) -> std::optional<std::string>
	{
		for (const auto &[name, value] : res.headers)
		{
			if (name.size() != wanted.size())
				continue;
			bool equal = true;
			for (std::size_t i = 0; i < name.size(); ++i)
				equal = equal
				        && std::tolower(static_cast<unsigned char>(name[i]))
				               == wanted[i];
			if (equal)
				return value;
		}
		return std::nullopt;
	};
	try
	{
		double value = -1.0;
		if (auto milliseconds = header_value("retry-after-ms"))
			value = std::stod(*milliseconds);
		else if (auto seconds = header_value("retry-after"))
			value = std::stod(*seconds) * 1000.0;
		if (std::isfinite(value) && value >= 0.0)
			return (static_cast<int>(std::min(value, 1e9)));
	}
	catch (const std::exception &)
	{
	}
	return (std::nullopt);
}

TypeSafeClientBuilder::TypeSafeClientBuilder()
{
	if (auto env_key = readEnv("TYPESAFE_API_KEY"))
		_config.api_key = *env_key;
	if (auto env_url = readEnv("TYPESAFE_BASE_URL"))
		_config.base_url = *env_url;
	if (auto env_model = readEnv("TYPESAFE_DEFAULT_MODEL"))
		_config.default_model = *env_model;
}

TypeSafeClientBuilder &TypeSafeClientBuilder::api_key(const std::string &key)
{
	_config.api_key = key;
	return (*this);
}

TypeSafeClientBuilder &TypeSafeClientBuilder::base_url(const std::string &url)
{
	_config.base_url = url;
	_config.openrouter = false;
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

TypeSafeClientBuilder &TypeSafeClientBuilder::retry_timeout(int ms)
{
	_config.retry_timeout_ms = ms;
	return (*this);
}

TypeSafeClientBuilder &TypeSafeClientBuilder::openrouter(const std::string &key)
{
	_config.api_key = key;
	_config.default_model = kOpenRouterModel;
	_config.base_url = kOpenRouterBaseUrl;
	_config.openrouter = true;
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
	if (_config.api_key)
		_config.api_key = trimmed(*_config.api_key);
	if (!_config.api_key || _config.api_key->empty())
		throw AuthenticationError("TYPESAFE_API_KEY is not set.");
	for (const char c : *_config.api_key)
		if (c < '!' || c > '~')
			throw AuthenticationError(
			    "API key must not contain whitespace, control, or non-ASCII "
			    "characters.");
	_config.base_url = trimmed(_config.base_url);
	if (!(_config.base_url.rfind("https://", 0) == 0
	      && _config.base_url.size() > 8)
	    && !(_config.base_url.rfind("http://", 0) == 0
	         && _config.base_url.size() > 7))
		throw ValidationError(
		    "Base URL must be an absolute http or https URL.");
	while (_config.base_url.size() > 1 && _config.base_url.back() == '/')
		_config.base_url.pop_back();
	if (_config.default_model.empty())
		throw ValidationError("Default model must not be empty.");
	if (_config.timeout_ms <= 0)
		throw ValidationError("Timeout must be greater than zero.");
	if (_config.max_retries < 0)
		throw ValidationError("Maximum retries must not be negative.");
	if (_config.retry_timeout_ms < 0)
		throw ValidationError("Retry timeout must not be negative.");

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
    const std::string                        &url,
    const std::optional<std::string>         &body,
    int                                       timeout_ms,
    const std::map<std::string, std::string> &extra_headers) const
{
	if (!_transport)
		throw APIConnectionError("No transport provided to TypeSafeClient.");

	HttpRequest req;
	req.method = method;
	req.url = url;
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

	const auto started = std::chrono::steady_clock::now();
	auto       elapsed_ms = [&started]()
	{
		return (std::chrono::duration_cast<std::chrono::milliseconds>(
		            std::chrono::steady_clock::now() - started)
		            .count());
	};
	const long long budget_ms = _config.retry_timeout_ms;
	int             retry = 0;

	while (true)
	{
		std::optional<HttpResponse> response;
		std::string                 connection_error;
		try
		{
			response = _transport->request(req);
		}
		catch (const std::exception &e)
		{
			connection_error = e.what();
		}

		if (response)
		{
			const int status = response->status_code;
			if (status >= 200 && status < 300)
				return (*response);
			const bool retryable = status == 408 || status == 429
			                       || (status >= 500 && status <= 599);
			if (!retryable || retry >= _config.max_retries)
				throwForStatus(*response);
		}
		else if (retry >= _config.max_retries)
			throw APIConnectionError("Connection failed: " + connection_error);

		const std::optional<int> requested
		    = response ? retryAfterMs(*response) : std::nullopt;
		const int delay_ms = requested ? *requested : backoffDelayMs(retry + 1);

		const bool over_budget = budget_ms > 0
		                             ? elapsed_ms() + delay_ms >= budget_ms
		                             : delay_ms > kMaxUnbudgetedWaitMs;
		if (over_budget)
		{
			if (response)
				throwForStatus(*response);
			throw APIConnectionError("Connection failed: " + connection_error);
		}
		retry++;
		std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
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
			const std::size_t options = question.at("criteria").size();
			if (options == 0 || options > kMaxChoiceOptions)
				throw ValidationError(
				    "Choice question \"" + name + "\" has "
				    + std::to_string(options)
				    + " options; between 1 and 255 are required.");
			for (const auto &[key, value] : question.at("criteria").items())
				if (!isJsonContent(value, true))
					throw ValidationError("Choice criterion '" + key
					                      + "' has an invalid description.");
		}
		if (type == "score")
		{
			if (!question.contains("criteria")
			    || !question.at("criteria").is_array())
				throw ValidationError("Score question '" + name
				                      + "' requires array criteria.");
			const std::size_t levels = question.at("criteria").size();
			if (levels < 2)
				throw ValidationError(
				    "Score question \"" + name + "\" has "
				    + std::to_string(levels)
				    + " criteria; at least two scores are required.");
			if (levels > kMaxScoreLevels)
				throw ValidationError(
				    "Score question \"" + name + "\" has "
				    + std::to_string(levels)
				    + " criteria; at most ten scores are allowed.");
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
	return sendRequest("POST", _config.base_url + kSystemOnePath, body_str,
	                   timeout_ms, request.extra_headers);
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
	if (_config.openrouter)
		throw ValidationError(
		    "listModels() is only available on the native TypeSafe API, "
		    "not through OpenRouter.");
	HttpResponse res = sendRequest("GET", _config.base_url + kModelsPath,
	                               std::nullopt, _config.timeout_ms);

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
