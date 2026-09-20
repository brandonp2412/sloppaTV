# Third-party C++ dependencies

## nlohmann/json

- Upstream: https://github.com/nlohmann/json
- Version: 3.12.0
- License: MIT (nlohmann/LICENSE.MIT)
- Source layout: upstream include/nlohmann tree, vendored without project-specific modifications.

sloppaTV deliberately vendors this header-only dependency. Both the Android NDK build and the standalone host C++ tests consume the same headers, without requiring a system package manager or network access during CMake configuration.

When updating it, replace the nlohmann tree from an official upstream release, keep LICENSE.MIT, verify the version macros in detail/abi_macros.hpp, then run the host tests and C++ quality gates.
