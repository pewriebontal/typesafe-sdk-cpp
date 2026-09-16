// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

/**
 * @file 02_triage_ticket.cpp
 * @brief Ticket triage combining Choice, Score, and Noul questions.
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

		request.state = nlohmann::json{
		    {"subject", "Duplicate charge"},
		    {"message", "I was charged twice. Please fix this ASAP."}};

		request.add("billing",
		            typesafe::Noul{"Is this about billing?", std::nullopt});

		request.add("category",
		            typesafe::Choice{
		                "What is this ticket about?",
		                {{"billing", "A charge, refund or invoice problem"},
						 {"technical", "Something is broken or slow"},
						 {"other", std::nullopt}}});

		request.add("urgency",
		            typesafe::Score{"How urgent is this?",
		                            {"can wait", "this week", "today"}});

		const typesafe::SystemOneResponse answer = client.systemOne(request);

		const auto &category = answer.choices.at("category");
		const auto &urgency = answer.scores.at("urgency");

		std::cout << "billing: " << answer.nouls.at("billing").noul << "\n";
		std::cout << "category: " << category.choice << " (confidence "
		          << category.confidence << ")\n";
		for (const auto &[name, chance] : category.probabilities)
			std::cout << "  " << name << ": " << chance << "\n";
		std::cout << "urgency: " << urgency.score << " (";
		for (const auto &[level, meaning] : urgency.legend)
			std::cout << level << "=" << meaning.dump() << " ";
		std::cout << ")\n";
		std::cout << "model: " << answer.model << "\n";
		return (0);
	}
	catch (const typesafe::TypeSafeError &error)
	{
		std::cerr << "TypeSafe: " << error.what() << "\n";
		return (1);
	}
}
