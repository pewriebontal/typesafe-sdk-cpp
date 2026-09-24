# TypeSafe C++ SDK — API Reference

This document provides a comprehensive technical reference for the `typesafe-sdk-cpp` library.

---

## Table of Contents

1. [Client & Configuration](#client--configuration)
   - [TypeSafeClientBuilder](#typesafeclientbuilder)
   - [Environment Variables](#environment-variables)
   - [TypeSafeClient](#typesafeclient)
2. [Evaluation Methods](#evaluation-methods)
   - [Synchronous: systemOne](#synchronous-systemone)
   - [Asynchronous: systemOneAsync](#asynchronous-systemoneasync)
   - [Structured Deserialization: systemOneAs](#structured-deserialization-systemoneas)
3. [Question Primitives & Request Types](#question-primitives--request-types)
   - [Choice](#choice)
   - [Score](#score)
   - [Noul](#noul)
   - [SystemOneRequest](#systemonerequest)
4. [Response Types](#response-types)
   - [SystemOneResponse](#systemoneresponse)
   - [ChoiceAnswer](#choiceanswer)
   - [ScoreAnswer](#scoreanswer)
   - [NoulAnswer](#noulanswer)
   - [Usage](#usage)
5. [Models Discovery API](#models-discovery-api)
   - [listModels & listModelsAsync](#listmodels--listmodelsasync)
   - [ModelMetadata & ListModelsResponse](#modelmetadata--listmodelsresponse)
6. [Error Handling & Exceptions](#error-handling--exceptions)
   - [Exception Hierarchy](#exception-hierarchy)
   - [Error Handling Guidelines](#error-handling-guidelines)

---

## Client & Configuration

Header: `<typesafe/client.h>`

### TypeSafeClientBuilder

`TypeSafeClientBuilder` provides a fluent interface for configuring and constructing a `TypeSafeClient`.

```cpp
#include <typesafe/client.h>

auto client = typesafe::TypeSafeClient::builder()
    .api_key("ts_live_...")
    .base_url("https://api.typesafe.ai")
    .model("jev-latest")
    .timeout(10000)
    .max_retries(2)
    .retry_timeout(30000)
    .build();
```

#### Builder Methods

| Method | Parameters | Default | Description |
|---|---|---|---|
| `api_key` | `const std::string &key` | `TYPESAFE_API_KEY` env | Sets the API bearer token used for authorization. Surrounding whitespace is trimmed; a key with whitespace, control or non-ASCII characters inside throws `AuthenticationError`. |
| `base_url` | `const std::string &url` | `https://api.typesafe.ai` | Base URL for all HTTP endpoints; must start with `http://` or `https://`. Overrides `TYPESAFE_BASE_URL`. |
| `model` | `const std::string &model` | `jev-latest` | Default model alias or version identifier. Overrides `TYPESAFE_DEFAULT_MODEL`. |
| `timeout` | `int ms` | `10000` (10s) | Request timeout in milliseconds; must be greater than zero. |
| `max_retries` | `int retries` | `2` | Retries after the first attempt for connection failures, 408, 429 and 5xx. Waits follow `Retry-After` / `retry-after-ms` when the server sends one, otherwise exponential backoff from 500 ms, doubling up to 5 s, less up to 25% jitter. |
| `retry_timeout` | `int ms` | `30000` (30s) | Total budget for one call, counting every attempt and every wait. No retry starts when the time spent plus its wait would reach the budget, so a `Retry-After` longer than the budget ends the call; the last attempt's error is thrown. A running attempt keeps its full `timeout`. `0` disables the limit, except that a wait longer than 60 s still ends the call; negative throws `ValidationError`. |
| `openrouter` | `const std::string &key` | — | Experimental OpenRouter preset: sets the key, base URL `https://openrouter.ai/api` (calls go to `/api/v1/systemone`) and model `typesafe/jev-1.13`. Call `model` afterwards to override the model; calling `base_url` afterwards leaves OpenRouter mode. `listModels()` throws `ValidationError` on such a client. |
| `transport` | `std::unique_ptr<Transport>` | `CurlTransport` | Injects a custom HTTP transport layer. |
| `build` | *None* | — | Validates settings and constructs a `TypeSafeClient` instance. |

### Environment Variables

If configuration values are not explicitly supplied to the builder, the SDK automatically reads from the environment. Values are trimmed, and empty or whitespace-only values are ignored:

- **`TYPESAFE_API_KEY`**: Fallback bearer token. If missing from both the builder and environment, `builder.build()` throws `typesafe::AuthenticationError`.
- **`TYPESAFE_BASE_URL`**: Fallback base URL (defaults to `https://api.typesafe.ai` if unset).
- **`TYPESAFE_DEFAULT_MODEL`**: Fallback default model identifier (defaults to `jev-latest` if unset).

### TypeSafeClient

The central entry point for evaluating requests against TypeSafe AI services.

```cpp
class TypeSafeClient {
public:
    static TypeSafeClientBuilder builder();

    ~TypeSafeClient();
    TypeSafeClient(const TypeSafeClient &) = delete;
    TypeSafeClient &operator=(const TypeSafeClient &) = delete;
    TypeSafeClient(TypeSafeClient &&) = default;
    TypeSafeClient &operator=(TypeSafeClient &&) = default;

    SystemOneResponse systemOne(const SystemOneRequest &request) const;
    std::future<SystemOneResponse> systemOneAsync(const SystemOneRequest &request) const;

    template <typename T>
    T systemOneAs(const SystemOneRequest &request) const;

    template <typename T>
    std::future<T> systemOneAsAsync(const SystemOneRequest &request) const;

    ListModelsResponse listModels() const;
    std::future<ListModelsResponse> listModelsAsync() const;
};
```

---

## Evaluation Methods

### Synchronous: systemOne

```cpp
SystemOneResponse systemOne(const SystemOneRequest &request) const;
```

Executes a blocking HTTP POST request against the System One evaluation endpoint (`/v1/systemone`), with automated exponential backoff and retry handling.

* **Throws**:
  - `ValidationError`: Invalid question schema or HTTP 422.
  - `AuthenticationError`: Missing or rejected API key (HTTP 401 / 403).
  - `RateLimitError`: Rate limit exceeded after retries (HTTP 429).
  - `APIError`: Server error (HTTP 5xx) or unexpected status code.
  - `APIConnectionError`: Network unreachable or DNS failure.

### Asynchronous: systemOneAsync

```cpp
std::future<SystemOneResponse> systemOneAsync(const SystemOneRequest &request) const;
```

Executes the request asynchronously on a background worker thread (`std::async`).

* **Thread-Safety & Lifetime Guarantee**: The returned `std::future` owns its own copy of the request payload, the client configuration, and a shared reference to the transport. The future remains completely valid and safe to await even if the originating `TypeSafeClient` instance is destructed.

```cpp
auto future = client.systemOneAsync(req);

// Do other work concurrently...

SystemOneResponse response = future.get(); // Throws TypeSafeError subclasses on failure
```

### Structured Deserialization: systemOneAs

```cpp
template <typename T>
T systemOneAs(const SystemOneRequest &request) const;
```

Evaluates the request and automatically converts the raw JSON response into a user-defined struct `T` using `nlohmann::json` deserialization (`from_json`).

```cpp
struct TriageResult {
    std::string category;
    double urgency = 0.0;
    bool needs_escalation = false;
};

void from_json(const nlohmann::json &j, TriageResult &res) {
    const nlohmann::json &answers = j.at("answers");
    res.category = answers.at("category").at("choice").get<std::string>();
    res.urgency = answers.at("urgency").at("score").get<double>();
    res.needs_escalation = answers.at("needs_escalation").at("noul").get<double>() > 0.5;
}

// Direct strongly-typed call:
TriageResult result = client.systemOneAs<TriageResult>(req);
```

---

## Question Primitives & Request Types

Header: `<typesafe/types.h>`

TypeSafe distinguishes evaluations into three typed mathematical primitives:

### Choice

Categorical multiple-choice questions where the model selects exactly one key from the specified criteria options.

```cpp
struct Choice {
    std::optional<nlohmann::json> instructions;
    std::map<std::string, std::optional<nlohmann::json>> criteria;
};
```

* **`instructions`**: Optional prompt or rubric defining the selection task. Can be a string or structured JSON.
* **`criteria`**: Key-value map of option names to descriptions (or `std::nullopt`). Between 1 and 255 options are required; anything else throws `ValidationError` before anything is sent.

```cpp
typesafe::Choice category_q{
    "Classify the support ticket type",
    {
        {"billing", "Invoices, payment failures, refunds"},
        {"technical", "Bugs, downtime, error messages"},
        {"feature_request", "Feedback and new feature proposals"}
    }
};
```

### Score

Continuous or graded evaluation where the model evaluates a subject against an ordered rubric continuum.

```cpp
struct Score {
    std::optional<nlohmann::json> instructions;
    std::vector<nlohmann::json> criteria;
};
```

* **`instructions`**: Scoring instructions or evaluation guide.
* **`criteria`**: Ordered sequence representing ascending levels of the metric. Between 2 and 10 levels are required; anything else throws `ValidationError` before anything is sent.

```cpp
typesafe::Score urgency_q{
    "Assess operational urgency",
    {"low (can wait)", "medium (within 24h)", "high (within 1h)", "critical (blocker)"}
};
```

### Noul

Calibrated binary yes/no probability evaluation.

```cpp
struct NoulCriteria {
    std::optional<nlohmann::json> true_meaning;
    std::optional<nlohmann::json> false_meaning;
};

struct Noul {
    std::optional<nlohmann::json> instructions;
    std::optional<NoulCriteria> criteria;
};
```

* **`instructions`**: The binary question.
* **`criteria`**: Optional semantic definitions of what constitutes "true" vs "false".

```cpp
typesafe::Noul churn_risk{
    "Does this customer show intent to cancel their subscription?"
};
```

### SystemOneRequest

Represents an evaluation request payload.

```cpp
struct SystemOneRequest {
    nlohmann::json state;                            // Document, message, or state context
    std::optional<std::string> model;                // Optional model override
    std::optional<int> timeout_ms;                   // Optional per-request timeout override
    std::map<std::string, nlohmann::json> questions; // Serialized question descriptors
    std::map<std::string, nlohmann::json> extra_body;// Additional top-level fields
    std::map<std::string, std::string> extra_headers;// Per-request HTTP headers

    void add(const std::string &key, const Choice &q);
    void add(const std::string &key, const Score &q);
    void add(const std::string &key, const Noul &q);
};
```

---

## Response Types

Header: `<typesafe/types.h>`

### SystemOneResponse

Container for all returned answers, token counts, and model metadata.

```cpp
struct SystemOneResponse {
    std::string model;
    Usage usage;
    std::map<std::string, ChoiceAnswer> choices;
    std::map<std::string, ScoreAnswer> scores;
    std::map<std::string, NoulAnswer> nouls;
};
```

### ChoiceAnswer

```cpp
struct ChoiceAnswer {
    std::string choice;                          // The chosen key from criteria
    double confidence = 0.0;                     // Model confidence [0.0, 1.0]
    std::map<std::string, double> probabilities; // Probability distribution across options
};
```

### ScoreAnswer

```cpp
struct ScoreAnswer {
    double score = 0.0;                          // Numeric score position (interpolated)
    double confidence = 0.0;                     // Confidence in the score placement
    std::map<int, nlohmann::json> legend;        // Mapped criteria levels
    std::map<int, double> probabilities;         // Probabilities per discrete criteria point
};
```

### NoulAnswer

```cpp
struct NoulAnswer {
    double noul = 0.0;  // Calibrated probability [0.0, 1.0] that the condition is true
};
```

### Usage

```cpp
struct Usage {
    std::int64_t input_tokens = 0;
    std::int64_t output_tokens = 0;
};
```

---

## Models Discovery API

Header: `<typesafe/client.h>`, `<typesafe/types.h>`

### listModels & listModelsAsync

Queries the `/v1/models` endpoint for available models and release metadata.

```cpp
// Synchronous:
typesafe::ListModelsResponse models_info = client.listModels();

// Asynchronous:
std::future<typesafe::ListModelsResponse> future = client.listModelsAsync();
typesafe::ListModelsResponse models_info = future.get();
```

### ModelMetadata & ListModelsResponse

```cpp
struct ModelMetadata {
    std::string name;         // e.g. "jev-1.13.0" or "jev-latest"
    std::string description;  // Model description
    std::string release_date; // ISO 8601 release date (e.g. "2026-01-15")
};

struct ListModelsResponse {
    std::vector<ModelMetadata> models;
};
```

---

## Error Handling & Exceptions

Header: `<typesafe/typesafe_error.h>`

### Exception Hierarchy

All SDK exceptions derive from `typesafe::TypeSafeError`, which inherits `std::runtime_error`:

```
std::runtime_error
 └── typesafe::TypeSafeError
      ├── ValidationError         (Schema violations, HTTP 422)
      ├── AuthenticationError     (Missing or invalid credentials, HTTP 401/403)
      ├── RateLimitError          (HTTP 429 after retries and backoff)
      ├── APIError                (Server errors, HTTP 5xx, or unexpected responses)
      └── APIConnectionError      (Network connectivity failure, socket drops, DNS)
```

### Error Handling Guidelines

```cpp
try {
    auto res = client.systemOne(req);
} catch (const typesafe::ValidationError &e) {
    // Bad request parameters or invalid schema; fix client payload
    std::cerr << "Validation failed: " << e.what() << "\n";
} catch (const typesafe::AuthenticationError &e) {
    // Authentication failed; check TYPESAFE_API_KEY
    std::cerr << "Auth rejected: " << e.what() << "\n";
} catch (const typesafe::RateLimitError &e) {
    // Rate limit hit despite automatic retries; back off before retrying
    std::cerr << "Rate limited: " << e.what() << "\n";
} catch (const typesafe::APIConnectionError &e) {
    // Service unreachable
    std::cerr << "Connection failure: " << e.what() << "\n";
} catch (const typesafe::APIError &e) {
    // Other API error or 5xx server error
    std::cerr << "Server API error: " << e.what() << "\n";
} catch (const typesafe::TypeSafeError &e) {
    // Generic catch-all for any TypeSafe error
    std::cerr << "TypeSafe error: " << e.what() << "\n";
}
```
