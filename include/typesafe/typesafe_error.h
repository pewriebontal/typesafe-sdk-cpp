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
 * @brief The API key was rejected (HTTP 401 or 403).
 *
 * Fix the key, not the call.
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
 * The retries already honoured Retry-After; back off longer before the
 * next attempt.
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
 * HTTP 422. Fix the request; sending it unchanged will fail again.
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
