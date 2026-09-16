// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#pragma once

#include "typesafe/client.h"
#include "typesafe/transport.h"
#include "typesafe/types.h"
#include "typesafe/typesafe_error.h"

#ifdef TYPESAFE_USE_LIBCURL
#include "typesafe/curl_transport.h"
#endif

#ifdef TYPESAFE_USE_BOOST_ASIO
#include "typesafe/boost_transport.h"
#endif
