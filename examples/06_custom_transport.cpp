// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

/**
 * @file 06_custom_transport.cpp
 * @brief Implementing a custom Transport to mock or intercept HTTP requests.
 */

#include <typesafe/typesafe.h>

#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace
{

/**
 * @brief In-memory mock transport returning canned JSON responses.
 */
class CannedTransport final : public typesafe::Transport
{
	public:
		typesafe::HttpResponse request(
		    const typesafe::HttpRequest &request) override
		{
			const nlohmann::json body = nlohmann::json{
			    {"model", "jev-1.13.0"},
			    {"answers", {{"spam", {{"type", "noul"}, {"noul", 0.98}}}}},
			    {"usage", {{"input_tokens", 42}, {"output_tokens", 4}}}};

			last_url = request.url;
			return (typesafe::HttpResponse{200, body.dump(), {}});
		}

		std::string last_url;
};

}  // namespace

int main()
{
	auto  transport = std::make_unique<CannedTransport>();
	auto *canned = transport.get();
	auto  client = typesafe::TypeSafeClient::builder()
	                   .api_key("any-key")
	                   .transport(std::move(transport))
	                   .build();

	typesafe::SystemOneRequest request;

	request.state = "You won a free cruise!";
	request.add("spam", typesafe::Noul{"Is this message spam?", std::nullopt});

	const typesafe::SystemOneResponse answer = client.systemOne(request);

	std::cout << "spam probability: " << answer.nouls.at("spam").noul << "\n";
	std::cout << "the injected transport saw: " << canned->last_url << "\n";
	return (0);
}
