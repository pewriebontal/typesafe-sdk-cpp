// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

/**
 * @file 11_openrouter_suite.cpp
 * @brief Multi-feature integration test suite against OpenRouter Jev 1.13.
 */

#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "openrouter_helper.h"

struct TriageReport
{
		std::string category;
		double      urgency = 0.0;
		bool        escalate = false;
};

void from_json(const nlohmann::json &j, TriageReport &r)
{
	const auto &answers = j.at("answers");
	r.category = answers.at("category").at("choice").get<std::string>();
	r.urgency = answers.at("urgency").at("score").get<double>();
	r.escalate
	    = answers.at("needs_escalation").at("noul").get<double>() >= 0.70;
}

int main()
{
	auto client = typesafe::examples::createClient();

	int passed = 0;
	int total = 6;

	std::cout << "[1/6] Noul Binary Question: ";
	try
	{
		typesafe::SystemOneRequest req;
		req.state
		    = "Congratulations! You won a $1,000 lottery voucher. Click here.";
		req.add("is_spam", typesafe::Noul{"Is this message unsolicited spam?"});

		auto   res = client.systemOne(req);
		double prob = res.nouls.at("is_spam").noul;
		if (prob >= 0.70)
		{
			std::cout << "PASS (prob: " << prob << ")\n";
			passed++;
		}
		else
		{
			std::cout << "FAIL (prob: " << prob << ")\n";
		}
	}
	catch (const std::exception &e)
	{
		std::cout << "ERROR: " << e.what() << "\n";
	}

	std::cout << "[2/6] Choice Categorical Selection: ";
	try
	{
		typesafe::SystemOneRequest req;
		req.state
		    = "My credit card was charged twice for order #49102. Please issue "
		      "a refund.";
		req.add("dept", typesafe::Choice{
		                    "Assign department",
		                    {{"billing", "Payments, invoices, refunds"},
							 {"tech_support", "Bug reports, platform down"},
							 {"sales", "Enterprise license inquiries"}}});

		auto        res = client.systemOne(req);
		const auto &choice = res.choices.at("dept");
		if (choice.choice == "billing" && choice.confidence > 0.5)
		{
			std::cout << "PASS (choice: " << choice.choice << ")\n";
			passed++;
		}
		else
		{
			std::cout << "FAIL (choice: " << choice.choice << ")\n";
		}
	}
	catch (const std::exception &e)
	{
		std::cout << "ERROR: " << e.what() << "\n";
	}

	std::cout << "[3/6] Score Ordered Rubric: ";
	try
	{
		typesafe::SystemOneRequest req;
		req.state
		    = "PRODUCTION OUTAGE: All database replicas are failing health "
		      "checks!";
		req.add("severity", typesafe::Score{"Assess incident severity",
		                                    {"P3 - Minor issue",
		                                     "P2 - Degraded performance",
		                                     "P1 - Total production outage"}});

		auto        res = client.systemOne(req);
		const auto &score = res.scores.at("severity");
		if (score.score >= 1.5)
		{
			std::cout << "PASS (score: " << score.score << ")\n";
			passed++;
		}
		else
		{
			std::cout << "FAIL (score: " << score.score << ")\n";
		}
	}
	catch (const std::exception &e)
	{
		std::cout << "ERROR: " << e.what() << "\n";
	}

	std::cout << "[4/6] Combined Multi-Question Request: ";
	try
	{
		typesafe::SystemOneRequest req;
		req.state
		    = "The mobile app crashes every time I tap the checkout button.";
		req.add("area",
		        typesafe::Choice{"Issue area",
		                         {{"checkout", "Cart and purchase flow"},
		                          {"auth", "Login and signup"}}});
		req.add("urgency",
		        typesafe::Score{"Rate urgency", {"low", "medium", "critical"}});
		req.add("is_bug", typesafe::Noul{"Is this a software bug?"});

		auto res = client.systemOne(req);
		if (res.choices.count("area") && res.scores.count("urgency")
		    && res.nouls.count("is_bug"))
		{
			std::cout << "PASS\n";
			passed++;
		}
		else
		{
			std::cout << "FAIL\n";
		}
	}
	catch (const std::exception &e)
	{
		std::cout << "ERROR: " << e.what() << "\n";
	}

	std::cout << "[5/6] Async Concurrent Dispatch: ";
	try
	{
		std::vector<std::string> states
		    = {"Meeting at 3 PM today.", "WIN CASH NOW! Reply YES to 55555.",
		       "Can you review my pull request?"};

		std::vector<std::future<typesafe::SystemOneResponse>> futures;
		for (const auto &s : states)
		{
			typesafe::SystemOneRequest r;
			r.state = s;
			r.add("spam", typesafe::Noul{"Is spam?"});
			futures.push_back(client.systemOneAsync(r));
		}

		bool async_ok = true;
		for (auto &f : futures)
		{
			auto res = f.get();
			if (!res.nouls.count("spam"))
				async_ok = false;
		}

		if (async_ok)
		{
			std::cout << "PASS\n";
			passed++;
		}
		else
		{
			std::cout << "FAIL\n";
		}
	}
	catch (const std::exception &e)
	{
		std::cout << "ERROR: " << e.what() << "\n";
	}

	std::cout << "[6/6] Structured Deserialization: ";
	try
	{
		typesafe::SystemOneRequest req;
		req.state
		    = "Security breach: multiple unauthorized SSH logins from IP "
		      "198.51.100.23!";
		req.add("category",
		        typesafe::Choice{"Ticket category",
		                         {{"security", "Security incident"},
		                          {"billing", "Billing question"}}});
		req.add("urgency", typesafe::Score{"Severity level",
		                                   {"low", "medium", "critical"}});
		req.add("needs_escalation",
		        typesafe::Noul{"Does this require immediate escalation?"});

		TriageReport report = client.systemOneAs<TriageReport>(req);
		if (report.category == "security" && report.escalate)
		{
			std::cout << "PASS\n";
			passed++;
		}
		else
		{
			std::cout << "FAIL\n";
		}
	}
	catch (const std::exception &e)
	{
		std::cout << "ERROR: " << e.what() << "\n";
	}

	std::cout << "\nResults: " << passed << " / " << total << " passed\n";

	return (passed == total ? 0 : 1);
}
