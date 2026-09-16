// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

/**
 * @file 01_classify_message.cpp
 * @brief Binary classification with Noul questions.
 */

#include <typesafe/typesafe.h>

#include <iostream>

#include "openrouter_helper.h"

int main()
{
	try
	{
		auto client = typesafe::examples::createClient();

		typesafe::SystemOneRequest request;

		request.state = "You won a free cruise! Click here to claim.";
		request.add("spam",
		            typesafe::Noul{"Is this message unsolicited advertising?",
		                           std::nullopt});

		const typesafe::SystemOneResponse answer = client.systemOne(request);

		std::cout << "spam probability: " << answer.nouls.at("spam").noul
		          << "\n";
		std::cout << "tokens: " << answer.usage.input_tokens << " in, "
		          << answer.usage.output_tokens << " out\n";
		return (0);
	}
	catch (const typesafe::TypeSafeError &error)
	{
		std::cerr << "TypeSafe: " << error.what() << "\n";
		return (1);
	}
}
