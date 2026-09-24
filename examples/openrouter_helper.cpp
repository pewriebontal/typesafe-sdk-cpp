// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "openrouter_helper.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace typesafe::examples
{

TypeSafeClient createClient()
{
	const char *ts_key = std::getenv("TYPESAFE_API_KEY");
	const char *or_key = std::getenv("OPENROUTER_API_KEY");

	if (ts_key != nullptr && std::string(ts_key).size() > 0)
	{
		return TypeSafeClient::builder().api_key(ts_key).build();
	}
	if (or_key != nullptr && std::string(or_key).size() > 0)
	{
		return TypeSafeClient::builder().openrouter(or_key).build();
	}

	std::cerr
	    << "Error: Neither TYPESAFE_API_KEY nor OPENROUTER_API_KEY is set.\n"
	    << "Set one of them to run this example:\n"
	    << "  export OPENROUTER_API_KEY=\"sk-or-v1-...\"\n"
	    << "  or\n"
	    << "  export TYPESAFE_API_KEY=\"ts_live_...\"\n";
	std::exit(1);
}

bool isOpenRouter()
{
	const char *ts_key = std::getenv("TYPESAFE_API_KEY");
	const char *or_key = std::getenv("OPENROUTER_API_KEY");
	return ((ts_key == nullptr || *ts_key == '\0') && or_key != nullptr
	        && *or_key != '\0');
}

}  // namespace typesafe::examples
