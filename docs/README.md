# TypeSafe C++ SDK Documentation

Welcome to the technical documentation for the `typesafe-sdk-cpp` library.

---

## Documentation Guides

- **[API Reference](api_reference.md)**: Complete guide to client options, question types (`Choice`, `Score`, `Noul`), response structures, error hierarchy, and asynchronous execution.
- **[Pluggable Transports](transports.md)**: Networking architecture, the default `libcurl` transport, optional `Boost.Beast` transport, and writing custom transports for test fixtures or embedded systems.
- **[Examples Directory](../examples)**: 11 self-contained walkthrough programs and benchmarks demonstrating core SDK capabilities with both native TypeSafe AI and OpenRouter.

---

## Generating Doxygen Documentation

All public header files in `include/typesafe/` contain Doxygen docstrings. To generate local HTML API documentation:

```bash
# Generate HTML documentation to ./doxygen_output
doxygen -g Doxyfile # if generating a configuration file
doxygen Doxyfile
```

---

## Online Documentation

For platform concepts, model capabilities, and cloud service documentation, visit the [TypeSafe Documentation](https://docs.typesafe.ai/).
