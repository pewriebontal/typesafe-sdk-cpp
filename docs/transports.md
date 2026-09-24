# Pluggable Transports & Networking

The `typesafe-sdk-cpp` library decouples its high-level API operations from the underlying HTTP client using an abstract transport interface. This allows applications to use the default `libcurl` backend, an optional `Boost.Beast` backend, or inject a custom HTTP implementation.

---

## The Transport Interface

Header: `<typesafe/transport.h>`

All network operations route through the `typesafe::Transport` base class:

```cpp
namespace typesafe {

struct HttpRequest {
    std::string method;                         ///< HTTP verb (e.g. "GET" or "POST")
    std::string url;                            ///< Full target URL
    std::map<std::string, std::string> headers; ///< Request headers
    std::string body;                           ///< Request payload
    int timeout_ms = 0;                         ///< Millisecond timeout (0 = disabled)
};

struct HttpResponse {
    int status_code = 0;                        ///< HTTP status code (e.g. 200, 429)
    std::string body;                           ///< Response payload
    std::map<std::string, std::string> headers; ///< Response headers (case-insensitive/lowercased)
};

class Transport {
public:
    virtual ~Transport() = default;
    virtual HttpResponse request(const HttpRequest &req) = 0;
};

} // namespace typesafe
```

---

## Built-in Transports

### 1. CurlTransport (Default)

Header: `<typesafe/curl_transport.h>`

* **Backend**: `libcurl`
* **CMake Flag**: `TYPESAFE_USE_LIBCURL=ON` (enabled by default)
* **Features**: System SSL trust store verification; HTTP/1.1, or HTTP/2 when the libcurl build and the server negotiate it via ALPN. Each thread keeps one libcurl handle and reuses it, so requests after the first to the same host reuse the open connection and skip the TCP and TLS handshakes. Handles are never shared between threads.

When `TYPESAFE_USE_LIBCURL` is enabled in CMake, `TypeSafeClientBuilder` automatically instantiates `CurlTransport` if no custom transport is injected.

```cmake
find_package(CURL REQUIRED)
add_subdirectory(typesafe-sdk-cpp)
target_link_libraries(my_app PRIVATE typesafe_cpp)
```

```cpp
#include <typesafe/typesafe.h>

// Automatically uses CurlTransport:
auto client = typesafe::TypeSafeClient::builder().build();
```

---

### 2. BoostTransport (Optional)

Header: `<typesafe/boost_transport.h>`

* **Backend**: `Boost.Beast` + `Boost.Asio` with OpenSSL
* **CMake Flag**: `TYPESAFE_USE_BOOST_ASIO=ON`
* **Use Case**: Applications already built on the Boost asynchronous framework that prefer not to link against `libcurl`.

#### Enabling Boost Transport in CMake

```cmake
set(TYPESAFE_USE_LIBCURL OFF)
set(TYPESAFE_USE_BOOST_ASIO ON)

add_subdirectory(typesafe-sdk-cpp)
target_link_libraries(my_app PRIVATE typesafe_cpp)
```

#### Configuring the Client with BoostTransport

```cpp
#include <typesafe/client.h>
#include <typesafe/boost_transport.h>

auto client = typesafe::TypeSafeClient::builder()
    .transport(std::make_unique<typesafe::BoostTransport>())
    .build();
```

---

## Writing a Custom Transport

You can implement `typesafe::Transport` to support custom requirements:
- Mocking network interactions in unit tests
- Routing requests through proprietary corporate proxies or VPN tunnels
- Running on embedded systems with specialized network hardware
- Utilizing alternative HTTP libraries (such as `cpp-httplib`, `Qt Network`, or `cpr`)

### Example: Offline Mock Transport

```cpp
#include <typesafe/transport.h>
#include <typesafe/client.h>
#include <nlohmann/json.hpp>

class MockOfflineTransport : public typesafe::Transport {
public:
    typesafe::HttpResponse request(const typesafe::HttpRequest &req) override {
        typesafe::HttpResponse res;

        if (req.url.find("/v1/systemone") != std::string::npos) {
            res.status_code = 200;
            res.headers["content-type"] = "application/json";
            res.body = nlohmann::json{
                {"model", "jev-test"},
                {"usage", {{"input_tokens", 12}, {"output_tokens", 4}}},
                {"answers", {
                    {"intent", {
                        {"type", "choice"},
                        {"choice", "inquiry"},
                        {"confidence", 0.99},
                        {"probabilities", {{"inquiry", 0.99}, {"complaint", 0.01}}}
                    }}
                }}
            }.dump();
            return res;
        }

        res.status_code = 404;
        res.body = R"({"error": "Not Found"})";
        return res;
    }
};

int main() {
    auto client = typesafe::TypeSafeClient::builder()
        .api_key("mock-key")
        .transport(std::make_unique<MockOfflineTransport>())
        .build();

    // Runs completely offline without making socket connections
}
```

See [`examples/06_custom_transport.cpp`](../examples/06_custom_transport.cpp) for a runnable demonstration.

---

## Concurrency & Thread Safety

- **Synchronous Calls**: A single `Transport` instance may be invoked concurrently across threads if the client is invoked concurrently. Custom transports that maintain internal connection pools must ensure thread safety within `request()`.
- **Asynchronous Calls**: When invoking `systemOneAsync()` or `listModelsAsync()`, the SDK dispatches the `Transport::request()` call onto a separate thread managed by `std::async`.
