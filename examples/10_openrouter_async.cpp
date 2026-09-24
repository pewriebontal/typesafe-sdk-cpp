// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

/**
 * @file 10_openrouter_async.cpp
 * @brief Concurrent parallel classification through OpenRouter's System One
 * endpoint.
 */

#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "openrouter_helper.h"

int main()
{
	auto client = typesafe::examples::createClient();

	const std::vector<std::string> messages
	    = {"You won a free $1,000 Walmart gift card! Claim now.",
	       "Hey Sarah, let's grab coffee tomorrow at 10 AM.",
	       "Your AWS invoice for September ($412.50) is ready for download.",
	       "URGENT: Suspicious login detected from Moscow, Russia.",
	       "Can we reschedule the team sync to Thursday afternoon?"};

	const auto start_time = std::chrono::steady_clock::now();

	std::vector<std::future<typesafe::SystemOneResponse>> futures;
	futures.reserve(messages.size());

	for (const auto &msg : messages)
	{
		typesafe::SystemOneRequest req;
		req.state = msg;
		req.add(
		    "is_spam",
		    typesafe::Noul{"Is this message spam, phishing, or advertising?",
			               std::nullopt});
		req.add("urgency", typesafe::Score{"Urgency level",
		                                   {"low", "medium", "critical"}});

		futures.push_back(client.systemOneAsync(req));
	}

	for (std::size_t i = 0; i < messages.size(); ++i)
	{
		const auto   res = futures[i].get();
		const double spam_prob = res.nouls.at("is_spam").noul;
		const double urgency = res.scores.at("urgency").score;

		std::cout << "[" << (i + 1) << "/" << messages.size() << "] "
		          << (spam_prob >= 0.70 ? "SPAM" : "CLEAN")
		          << " (prob: " << std::fixed << std::setprecision(2)
		          << spam_prob << ", urgency: " << urgency << ")\n";
	}

	const auto end_time = std::chrono::steady_clock::now();
	const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
	                          end_time - start_time)
	                          .count();

	std::cout << "\nConcurrent total: " << total_ms << " ms ("
	          << (total_ms / messages.size()) << " ms/req effective)\n";

	return (0);
}
