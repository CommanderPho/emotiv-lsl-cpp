## Cursor Cloud specific instructions

### Project overview

Emotiv LSL C++ — a cross-platform C++20 application that reads raw EEG data from Emotiv EPOC X headsets via USB HID, decrypts it (AES-128 ECB), and publishes to Lab Streaming Layer (LSL). Also includes a generic LSL template with CLI and optional Qt6 GUI apps.

### System dependencies (installed in VM snapshot)

- `libudev-dev`, `libusb-1.0-0-dev` — required by hidapi for USB HID on Linux
- `ninja-build` — CMake build generator
- `libstdc++-13-dev` — the default C++ compiler (`c++`) is Clang, which cannot link without this; alternatively pass `-DCMAKE_CXX_COMPILER=g++`

### Build

The default `c++` on the VM points to Clang, which fails to link due to a missing `libstdc++` search path. Always specify GCC explicitly:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++ \
  -DLSLTEMPLATE_BUILD_GUI=OFF \
  -DLSLTEMPLATE_BUILD_CLI=ON \
  -DEMOTIVLSL_BUILD_EMOTIV=ON

cmake --build build --config Release --parallel
cmake --install build --prefix build/install
```

All C++ library dependencies (liblsl, hidapi, tiny-AES-c) are fetched automatically by CMake FetchContent — no manual installation needed. The GUI (`-DLSLTEMPLATE_BUILD_GUI=ON`) requires Qt 6.8 which is not installed in the VM; keep it OFF unless explicitly requested.

### Running

- **emotiv_lsl**: Requires a physical Emotiv USB dongle. Without one it exits with `"Emotiv headset not found"` which is expected.
- **LSLTemplateCLI**: Can run without hardware and streams synthetic data to LSL.
  ```bash
  LD_LIBRARY_PATH=build/install/lib:build/_deps/hidapi-build/src/libusb \
    ./build/install/bin/LSLTemplateCLI --name TestStream --rate 10 --channels 3
  ```
- The `hidapi` shared library (`libhidapi-libusb.so.0`) is built but not installed by default. Either add `build/_deps/hidapi-build/src/libusb` to `LD_LIBRARY_PATH` or use the installed `build/install/` path for liblsl.

### Testing

There is no automated test suite in this project. Verification is done by building successfully and running the binaries.

### Lint

No linter configuration is present. `compile_commands.json` is generated in the build directory (via `CMAKE_EXPORT_COMPILE_COMMANDS ON`) for use with clang-tidy or similar tools if desired.
