// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "typesafe/curl_transport.h"

#include <curl/curl.h>

#include <memory>
#include <mutex>
#include <stdexcept>

namespace typesafe
{

static size_t curlWriteCallback(void  *contents,
                                size_t size,
                                size_t nmemb,
                                void  *userp)
{
	const size_t realsize = size * nmemb;
	auto        *str = static_cast<std::string *>(userp);
	str->append(static_cast<char *>(contents), realsize);
	return (realsize);
}

static size_t curlHeaderCallback(char  *buffer,
                                 size_t size,
                                 size_t nitems,
                                 void  *userdata)
{
	const size_t realsize = size * nitems;
	auto *headers = static_cast<std::map<std::string, std::string> *>(userdata);
	std::string header(buffer, realsize);

	const size_t colon = header.find(':');
	if (colon != std::string::npos)
	{
		std::string key = header.substr(0, colon);
		std::string val = header.substr(colon + 1);

		for (char &c : key)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

		const size_t first = val.find_first_not_of(" \t\r\n");
		if (first != std::string::npos)
		{
			const size_t last = val.find_last_not_of(" \t\r\n");
			val = val.substr(first, (last - first + 1));
		}
		else
		{
			val.clear();
		}

		(*headers)[key] = val;
	}
	return (realsize);
}

/**
 * One easy handle per thread, reset between requests. libcurl keeps finished
 * connections alive inside the handle, so later requests to the same host
 * skip the TCP and TLS handshakes; no handle is ever shared between threads.
 */
static CURL *threadHandle()
{
	thread_local std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle(
	    curl_easy_init(), &curl_easy_cleanup);
	return (handle.get());
}

CurlTransport::CurlTransport()
{
	static std::once_flag initialized;
	std::call_once(initialized,
	               []()
	               {
		               const CURLcode result
		                   = curl_global_init(CURL_GLOBAL_DEFAULT);
		               if (result != CURLE_OK)
			               throw std::runtime_error(
			                   "Failed to initialize libcurl globally.");
	               });
}

CurlTransport::~CurlTransport() = default;

HttpResponse CurlTransport::request(const HttpRequest &req)
{
	CURL *curl = threadHandle();
	if (!curl)
		throw std::runtime_error("Failed to initialize libcurl.");
	curl_easy_reset(curl);

	HttpResponse response;
	curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

	if (req.method == "POST")
	{
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		if (!req.body.empty())
		{
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.c_str());
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
			                 static_cast<curl_off_t>(req.body.size()));
		}
	}
	else if (req.method != "GET")
	{
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req.method.c_str());
	}

	curl_slist *chunk = nullptr;
	for (const auto &[key, val] : req.headers)
	{
		const std::string header = key + ": " + val;
		chunk = curl_slist_append(chunk, header.c_str());
	}
	if (chunk)
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlHeaderCallback);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);

	if (req.timeout_ms > 0)
		curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,
		                 static_cast<long>(req.timeout_ms));

	const CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
		curl_slist_free_all(chunk);
		throw std::runtime_error(std::string("curl_easy_perform() failed: ")
		                         + curl_easy_strerror(res));
	}

	long status_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
	response.status_code = static_cast<int>(status_code);

	curl_slist_free_all(chunk);

	return (response);
}

}  // namespace typesafe
