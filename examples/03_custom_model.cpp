// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

/**
 * @file 03_custom_model.cpp
 * @brief Parsing API responses directly into custom structs via from_json.
 */

#include <typesafe/typesafe.h>

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "openrouter_helper.h"

namespace
{

/**
 * @brief Custom application struct for ticket triage results.
 */
struct TriageSummary
{
		std::string  category;
		double       urgency = 0.0;
		std::int64_t input_tokens = 0;
};

void from_json(const nlohmann::json &j, TriageSummary &summary)
{
	const nlohmann::json &answers = j.at("answers");

	summary.category = answers.at("category").at("choice").get<std::string>();
	summary.urgency = answers.at("urgency").at("score").get<double>();
	summary.input_tokens = j.at("usage").at("input_tokens").get<std::int64_t>();
}

}  // namespace

int main()
{
	try
	{
		auto client = typesafe::examples::createClient();

		typesafe::SystemOneRequest request;

		request.state = "I was charged twice. Please fix this ASAP.";
		request.add("category",
		            typesafe::Choice{"What is this ticket about?",
		                             {{"billing", std::nullopt},
		                              {"technical", std::nullopt}}});
		request.add(
		    "urgency",
		    typesafe::Score{"How urgent?", {"can wait", "this week", "today"}});

		const TriageSummary summary
		    = client.systemOneAs<TriageSummary>(request);

		std::cout << "category: " << summary.category << "\n";
		std::cout << "urgency: " << summary.urgency << "\n";
		std::cout << "cost " << summary.input_tokens << " input tokens\n";
		return (0);
	}
	catch (const typesafe::TypeSafeError &error)
	{
		std::cerr << "TypeSafe: " << error.what() << "\n";
		return (1);
	}
}
