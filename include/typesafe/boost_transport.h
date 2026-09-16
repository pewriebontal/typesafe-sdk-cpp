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
 * @brief HTTP transport backed by Boost.Beast.
 *
 * For applications that already run on Boost.Asio and do not use libcurl.
 */
class BoostTransport : public Transport
{
	public:
		BoostTransport();
		~BoostTransport() override;

		HttpResponse request(const HttpRequest &req) override;
};

}  // namespace typesafe
