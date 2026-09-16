# Examples

This directory contains standalone, runnable examples demonstrating the TypeSafe C++ SDK features.

Most examples run against either the native **TypeSafe AI API** (via `TYPESAFE_API_KEY`) or **OpenRouter's Jev 1.13 endpoint** (via `OPENROUTER_API_KEY`); the shared helper prefers `TYPESAFE_API_KEY` and falls back to `OPENROUTER_API_KEY`. Exceptions: `05` and `06` run offline with no key at all, `07` requires `TYPESAFE_API_KEY` (`GET /v1/models` is a native endpoint; the example prints a notice under OpenRouter), and `08` requires `OPENROUTER_API_KEY`.

---

## Example Index

| Executable | Description | Primitives / Focus |
|---|---|---|
| [`01_classify_message`](01_classify_message.cpp) | Binary classification with confidence | `Noul` |
| [`02_triage_ticket`](02_triage_ticket.cpp) | Multi-question ticket categorization and urgency | `Noul`, `Choice`, `Score` |
| [`03_custom_model`](03_custom_model.cpp) | Type deserialization directly into custom C++ structs | `systemOneAs<T>` |
| [`04_parallel_classification`](04_parallel_classification.cpp) | Concurrent asynchronous evaluation across 4 messages | `systemOneAsync` |
| [`05_error_handling`](05_error_handling.cpp) | Client error hierarchy, local validation rejection | `TypeSafeError`, `ValidationError` |
| [`06_custom_transport`](06_custom_transport.cpp) | In-memory mock transport for testing without network | Custom `Transport` |
| [`07_list_models`](07_list_models.cpp) | Fetching model aliases and version metadata | `listModels()`, `GET /v1/models` |
| [`08_openrouter`](08_openrouter.cpp) | Dedicated OpenRouter synchronous evaluation | OpenRouter `typesafe/jev-1.13` |
| [`09_openrouter_sync`](09_openrouter_sync.cpp) | Sequential (sync) evaluation of 5 messages with latency measurement | Non-async timing |
| [`10_openrouter_async`](10_openrouter_async.cpp) | Concurrent evaluation of 5 messages with latency measurement | Concurrency speedup |
| [`11_openrouter_suite`](11_openrouter_suite.cpp) | Comprehensive 6-test integration suite testing all primitives live | Full regression test suite |

---

## Running Examples

### Using OpenRouter (`OPENROUTER_API_KEY`)
```bash
export OPENROUTER_API_KEY="sk-or-v1-..."

# Run the synchronous sequential evaluation:
./build/examples/09_openrouter_sync

# Run the asynchronous concurrent evaluation:
./build/examples/10_openrouter_async

# Run the complete live integration test suite:
./build/examples/11_openrouter_suite
```

### Using TypeSafe AI (`TYPESAFE_API_KEY`)
```bash
export TYPESAFE_API_KEY="ts_live_..."

# Run any example directly:
./build/examples/01_classify_message
./build/examples/02_triage_ticket
./build/examples/04_parallel_classification
```
