// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

/**
 * @file 08_openrouter.cpp
 * @brief Running Jev evaluations through OpenRouter's System One endpoint.
 */

#include <typesafe/typesafe.h>

#include <cstdlib>
#include <iostream>
#include <string>

int main()
{
	const char *key = std::getenv("OPENROUTER_API_KEY");
	if (key == nullptr || std::string(key).empty())
	{
		std::cout << "Set OPENROUTER_API_KEY to run this example.\n"
		          << "Example:\n"
		          << "  export OPENROUTER_API_KEY=\"sk-or-v1-...\"\n"
		          << "  ./build/examples/08_openrouter\n";
		return (0);
	}

	try
	{
		auto client
		    = typesafe::TypeSafeClient::builder().openrouter(key).build();

		typesafe::SystemOneRequest request;
		request.state
		    = "My subscription was renewed automatically yesterday, but I "
		      "meant "
		      "to cancel last week. Can I please get a refund?";

		request.add("team",
		            typesafe::Choice{
		                "Route ticket to the appropriate team",
		                {{"billing", "Payments, invoices, renewals, refunds"},
						 {"support", "Technical bugs and errors"},
						 {"sales", "Enterprise pricing and upgrades"}}});

		request.add("urgency",
		            typesafe::Score{"Assess customer urgency",
		                            {"low (can wait)", "normal (within 24h)",
		                             "urgent (immediate)"}});

		request.add("is_refund_request",
		            typesafe::Noul{"Is the customer asking for a refund?",
		                           std::nullopt});

		std::cout << "Sending request to OpenRouter (typesafe/jev-1.13)...\n";
		const typesafe::SystemOneResponse answer = client.systemOne(request);

		std::cout << "\n=== Response from OpenRouter ===\n";
		std::cout << "Model:         " << answer.model << "\n";
		std::cout << "Team Choice:   " << answer.choices.at("team").choice
		          << " (confidence: " << answer.choices.at("team").confidence
		          << ")\n";
		std::cout << "Urgency Score: " << answer.scores.at("urgency").score
		          << " (confidence: " << answer.scores.at("urgency").confidence
		          << ")\n";
		std::cout << "Refund Request:"
		          << (answer.nouls.at("is_refund_request").noul >= 0.70 ? " YES"
		                                                                : " NO")
		          << " (probability: "
		          << answer.nouls.at("is_refund_request").noul << ")\n";
		std::cout << "Token Usage:   " << answer.usage.input_tokens
		          << " input tokens, " << answer.usage.output_tokens
		          << " output tokens\n";
		return (0);
	}
	catch (const typesafe::TypeSafeError &error)
	{
		std::cerr << "TypeSafe Error: " << error.what() << "\n";
		return (1);
	}
}
