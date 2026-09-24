// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#pragma once

#include <stdexcept>
#include <string>

namespace typesafe
{

/**
 * @brief Base exception for all SDK errors.
 */
class TypeSafeError : public std::runtime_error
{
	public:
		explicit TypeSafeError(const std::string &message)
		    : std::runtime_error(message)
		{
		}
};

/**
 * @brief The service could not be reached, after the configured retries.
 */
class APIConnectionError : public TypeSafeError
{
	public:
		explicit APIConnectionError(const std::string &message)
		    : TypeSafeError(message)
		{
		}
};

/**
 * @brief The API key is missing or malformed (thrown by build()), or was
 * rejected by the service (HTTP 401 or 403).
 */
class AuthenticationError : public TypeSafeError
{
	public:
		explicit AuthenticationError(const std::string &message)
		    : TypeSafeError(message)
		{
		}
};

/**
 * @brief Rate limit exceeded (HTTP 429), after the configured retries.
 *
 * Retries honour Retry-After within the call's retry budget; a longer
 * Retry-After ends the call at once.
 */
class RateLimitError : public TypeSafeError
{
	public:
		explicit RateLimitError(const std::string &message)
		    : TypeSafeError(message)
		{
		}
};

/**
 * @brief The service refused or failed the request (any other error
 * status).
 */
class APIError : public TypeSafeError
{
	public:
		explicit APIError(const std::string &message) : TypeSafeError(message)
		{
		}
};

/**
 * @brief The request broke the API contract.
 *
 * Refused by the client before anything was sent, or by the service as
 * HTTP 422.
 */
class ValidationError : public TypeSafeError
{
	public:
		explicit ValidationError(const std::string &message)
		    : TypeSafeError(message)
		{
		}
};

}  // namespace typesafe
