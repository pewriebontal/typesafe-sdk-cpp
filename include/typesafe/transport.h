// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#pragma once

#include <map>
#include <memory>
#include <string>

namespace typesafe
{

struct HttpRequest
{
		std::string method;  ///< HTTP verb, e.g. "GET" or "POST".
		std::string url;
		std::map<std::string, std::string> headers;
		std::string                        body;
		int timeout_ms = 0;  ///< Request timeout; 0 disables it.
};

struct HttpResponse
{
		int         status_code = 0;  ///< HTTP status code.
		std::string body;
		std::map<std::string, std::string>
		    headers;  ///< Response headers, names lower-cased.
};

/**
 * @brief The HTTP layer, so an application can supply its own client in place
 * of libcurl.
 */
class Transport
{
	public:
		virtual ~Transport() = default;

		/**
		 * @brief Performs one HTTP request.
		 * @return The response, whatever its status code.
		 * @throws any exception when the request could not be performed at
		 * all; the client turns failures into APIConnectionError.
		 */
		virtual HttpResponse request(const HttpRequest &req) = 0;
};

}  // namespace typesafe
