// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#pragma once

#include <typesafe/typesafe.h>

namespace typesafe::examples
{

/**
 * @brief Creates a TypeSafeClient configured for either TypeSafe AI or
 * OpenRouter.
 */
TypeSafeClient createClient();

/**
 * @brief Returns true if OPENROUTER_API_KEY is active and TYPESAFE_API_KEY is
 * not.
 */
bool isOpenRouter();

}  // namespace typesafe::examples
