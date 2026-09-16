// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#pragma once

#include "typesafe/transport.h"

namespace typesafe
{

/**
 * @brief Default libcurl-based HTTP transport.
 */
class CurlTransport : public Transport
{
	public:
		CurlTransport();
		~CurlTransport() override;

		HttpResponse request(const HttpRequest &req) override;
};

}  // namespace typesafe
