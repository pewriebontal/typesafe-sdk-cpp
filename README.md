# TypeSafe C++ SDK

A C++20 SDK for [TypeSafe AI](https://typesafe.ai). 

It configures clients through a Builder and types every question (`Choice`, `Score`, `Noul`) so answers arrive as fields, not strings to parse. It targets System One models such as Jev.

## Quickstart

```cpp
#include <typesafe/typesafe.h>
#include <iostream>

using namespace typesafe;

int main() {
    // 1. Automatically picks up TYPESAFE_API_KEY from the environment
    auto client = TypeSafeClient::builder().build();

    // 2. Prepare the Request
    SystemOneRequest req;
    req.state = nlohmann::json{{"document", "I was charged twice. Please fix this ASAP."}};
    
    req.add("category", Choice{
        "What is this ticket about?", 
        {{"billing", std::nullopt}, {"technical", std::nullopt}, {"other", std::nullopt}}
    });

    req.add("urgency", Score{
        "How urgent is this ticket?", 
        {"can wait", "this week", "today"}
    });

    // 3. Make the API Call
    try {
        SystemOneResponse response = client.systemOne(req);

        std::cout << "Category: " << response.choices["category"].choice << "\n";
        std::cout << "Urgency Score: " << response.scores["urgency"].score << "\n";
    } catch (const TypeSafeError& e) {
        std::cerr << "API Error: " << e.what() << "\n";
    }

    return 0;
}
```

## Installation (CMake)

The SDK uses standard CMake. It depends on `nlohmann/json`.

```cmake
add_subdirectory(typesafe-sdk-cpp)
target_link_libraries(your_target PUBLIC typesafe_cpp)
```

### Batteries Included (libcurl)
By default, the SDK compiles with a built-in `libcurl` transport so it works out of the box. 

To embed the SDK in an application that owns its HTTP stack, turn `libcurl` off in your CMake:

```cmake
set(TYPESAFE_USE_LIBCURL OFF)
add_subdirectory(typesafe-sdk-cpp)
```
Then, implement the `typesafe::Transport` interface and pass it to the builder:
```cpp
auto client = TypeSafeClient::builder()
    .transport(std::make_unique<MyCustomTransport>())
    .build();
```

### Boost.Beast Transport (Optional)
If your application is built on Boost.Asio and does not use `libcurl`, compile with Boost.Beast and OpenSSL:

```cmake
set(TYPESAFE_USE_LIBCURL OFF)
set(TYPESAFE_USE_BOOST_ASIO ON)
add_subdirectory(typesafe-sdk-cpp)
```
```cpp
#include <typesafe/boost_transport.h>

auto client = TypeSafeClient::builder()
    .transport(std::make_unique<BoostTransport>())
    .build();
```

## Examples

`examples/` holds eleven small, self-contained programs, one idea each:

| Example | Shows |
|---|---|
| `01_classify_message` | The smallest call: one yes/no question, answer read from `noul` |
| `02_triage_ticket` | Every question type in one request (`Noul`; `Choice` with option descriptions; `Score` against an ordered rubric), plus probabilities, legend and usage |
| `03_custom_model` | `systemOneAs<T>` parsing the response straight into your own struct via a `from_json` adapter |
| `04_parallel_classification` | Concurrent requests with `systemOneAsync` |
| `05_error_handling` | The typed error ladder; runs offline |
| `06_custom_transport` | Injecting your own `Transport`; runs offline against a canned response |
| `07_list_models` | Querying model names and aliases with `listModels` |
| `08_openrouter` | Evaluating Jev 1.13 live via OpenRouter's decisions endpoint |
| `09_openrouter_sync` | Sequential evaluation of 5 messages with per-request latency |
| `10_openrouter_async` | The same 5 messages concurrently, comparing the timings |
| `11_openrouter_suite` | A six-part live suite exercising every primitive end to end |

```bash
cmake -S . -B build && cmake --build build
./build/examples/05_error_handling        # runs offline
./build/examples/06_custom_transport      # runs offline
OPENROUTER_API_KEY="sk-..." ./build/examples/08_openrouter # runs live against OpenRouter Jev
```

## Models

`listModels()` (and `listModelsAsync()`) call `GET /v1/models` and return every name your account may send in the `model` field:

```cpp
for (const typesafe::ModelMetadata &model : client.listModels().models)
    std::cout << model.name << "  " << model.release_date << "\n";
```

The endpoint returns aliases and versioned model identifiers:
- **Aliases** (`jev-latest`, `jev-preview`): Point to the most recent release and update over time.
- **Versioned IDs** (`jev-1.13.0`): Fixed versions suitable for production pinning via `.model("jev-1.13.0")`.

The `model` field in `SystemOneResponse` always reports the exact versioned model that evaluated the request.

## Errors

Every failure is a `TypeSafeError`; the specific class says what went wrong and what to do about it:

| Exception | Meaning |
|---|---|
| `ValidationError` | The request broke the contract: refused locally, or by the service as HTTP 422. Fix the request; do not retry. |
| `AuthenticationError` | The key was rejected (401/403). Fix the key, not the call. |
| `RateLimitError` | 429, already retried with backoff (honouring `Retry-After` / `retry-after-ms`) and still limited. Back off longer. |
| `APIError` | The service refused or failed the request otherwise (5xx are retried, 529 included). |
| `APIConnectionError` | Could not reach the service at all; retried, still unreachable. |

## Testing

The suite is GoogleTest, fetched and built by CMake when tests are on (the default):

```bash
cmake -S . -B build -DTYPESAFE_USE_LIBCURL=OFF   # the tests inject their own transport
cmake --build build
ctest --test-dir build --output-on-failure
```

It covers the wire contract (default model, structured instructions, per-request headers, extra body fields), the error mapping and retry policy (which statuses retry, which throw which type, `Retry-After` handling), strict response validation, the models endpoint, and the async future's independence from the client that made it.

## Documentation

Detailed documentation and architecture guides are available in the [`docs/`](docs/) directory:

- **[Documentation Index](docs/README.md)**: Overview of all guides and technical documentation.
- **[API Reference](docs/api_reference.md)**: Detailed specification for `TypeSafeClientBuilder`, question types (`Choice`, `Score`, `Noul`), response containers, error classes, and asynchronous execution.
- **[Pluggable Transports](docs/transports.md)**: HTTP networking layer, `libcurl` default, `Boost.Beast` backend, and creating custom transports.
- **[Examples](examples/)**: Self-contained reference applications illustrating each core feature.
- **Header Documentation**: All public C++ headers in [`include/typesafe/`](include/typesafe/) are fully documented with Doxygen comments.

For cloud platform documentation and API reference, visit the official [TypeSafe Documentation](https://docs.typesafe.ai/).

## License

Copyright (c) 2026 Bontal LLC. Licensed under the [MIT License](LICENSE).

