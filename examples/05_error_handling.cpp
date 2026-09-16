// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

/**
 * @file 05_error_handling.cpp
 * @brief Demonstrates exception handling for TypeSafeError hierarchies.
 *
 * Runs offline: the empty request is refused locally, before anything
 * leaves the process.
 */

#include <typesafe/typesafe.h>

#include <iostream>

int main()
{
	auto client
	    = typesafe::TypeSafeClient::builder().api_key("not-a-real-key").build();

	typesafe::SystemOneRequest request;

	request.state = "hello";
	try
	{
		client.systemOne(request);
		std::cout << "unexpectedly accepted an empty request\n";
		return (1);
	}
	catch (const typesafe::ValidationError &)
	{
		std::cout << "caught ValidationError: the request was refused "
		             "before anything was sent\n";
		return (0);
	}
	catch (const typesafe::AuthenticationError &)
	{
		std::cout << "caught AuthenticationError\n";
	}
	catch (const typesafe::RateLimitError &)
	{
		std::cout << "caught RateLimitError\n";
	}
	catch (const typesafe::APIError &)
	{
		std::cout << "caught APIError\n";
	}
	catch (const typesafe::APIConnectionError &)
	{
		std::cout << "caught APIConnectionError\n";
	}
	catch (const typesafe::TypeSafeError &error)
	{
		std::cout << "caught TypeSafeError: " << error.what() << "\n";
	}
	return (0);
}
