// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

/**
 * @file 07_list_models.cpp
 * @brief Listing available models and metadata from GET /v1/models.
 */

#include <typesafe/typesafe.h>

#include <cstdlib>
#include <iostream>

#include "openrouter_helper.h"

int main()
{
	if (typesafe::examples::isOpenRouter())
	{
		std::cout << "Notice: 'listModels()' (GET /v1/models) is a native "
		             "TypeSafe AI API endpoint.\n"
		          << "OpenRouter directly routes decisions via model "
		             "'typesafe/jev-1.13'.\n"
		          << "To query the native TypeSafe AI models endpoint, set "
		             "TYPESAFE_API_KEY.\n";
		return (0);
	}

	const char *typesafe_key = std::getenv("TYPESAFE_API_KEY");

	if (typesafe_key == nullptr || *typesafe_key == '\0')
	{
		std::cout << "Set TYPESAFE_API_KEY to run this example against "
		             "TypeSafe AI.\n";
		return (0);
	}
	try
	{
		auto client = typesafe::TypeSafeClient::builder().build();

		for (const typesafe::ModelMetadata &model : client.listModels().models)
			std::cout << model.name << "  released " << model.release_date
			          << "  - " << model.description << "\n";
		return (0);
	}
	catch (const typesafe::TypeSafeError &error)
	{
		std::cerr << "TypeSafe: " << error.what() << "\n";
		return (1);
	}
}
