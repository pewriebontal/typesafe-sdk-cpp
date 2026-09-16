// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

/**
 * @file 04_parallel_classification.cpp
 * @brief Concurrent classification across multiple messages using
 * systemOneAsync.
 */

#include <typesafe/typesafe.h>

#include <future>
#include <iostream>
#include <string>
#include <vector>

#include "openrouter_helper.h"

int main()
{
	try
	{
		auto client = typesafe::examples::createClient();

		const std::vector<std::string> messages = {
		    "You won a free cruise! Click here to claim.",
		    "Hey, are we still on for lunch tomorrow?",
		    "Your invoice for August is attached. Thank you!",
		    "URGENT: your account will be suspended in 24 hours",
		};

		std::vector<std::future<typesafe::SystemOneResponse>> futures;

		futures.reserve(messages.size());
		for (const std::string &message : messages)
		{
			typesafe::SystemOneRequest request;

			request.state = message;
			request.add("spam", typesafe::Noul{
			                        "Is this message unsolicited advertising?",
			                        std::nullopt});
			futures.push_back(client.systemOneAsync(request));
		}

		for (std::size_t index = 0; index < messages.size(); ++index)
		{
			const typesafe::SystemOneResponse answer = futures[index].get();

			std::cout << (answer.nouls.at("spam").noul > 0.5 ? "SPAM  "
			                                                 : "clean")
			          << "  " << messages[index] << "\n";
		}
		return (0);
	}
	catch (const typesafe::TypeSafeError &error)
	{
		std::cerr << "TypeSafe: " << error.what() << "\n";
		return (1);
	}
}
