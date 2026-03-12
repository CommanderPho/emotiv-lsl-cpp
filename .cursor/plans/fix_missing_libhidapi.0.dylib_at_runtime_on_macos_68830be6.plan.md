---
name: Fix Missing libhidapi.0.dylib at Runtime on macOS
overview: ""
todos:
  - id: fix-hidapi-install
    content: Add elseif(APPLE) install rule for hidapi_darwin LIBRARY in CMakeLists.txt install block
    status: completed
---

# Fix Missing libhidapi.0.dylib at Runtime on macOS

## Problem

When running `./build/install/emotiv_lsl`, macOS's dynamic linker (`dyld`) fails because it cannot find `libhidapi.0.dylib`.

**Root cause:** The binary has these RPATHs embedded:

- `@executable_path/../Frameworks`
- `@executable_path/Frameworks`
- `@executable_path`

The dylib `libhidapi.0.dylib` exists in the build tree at `build/_deps/hidapi-build/src/mac/libhidapi.0.dylib` (with install name `@rpath/libhidapi.0.dylib`), but the `cmake --install` step never copies it to `build/install/`. The install directory only contains `emotiv_lsl` and `LSLTemplate.cfg`.

On Windows, this is already handled in [CMakeLists.txt](CMakeLists.txt) (lines 206–208):

```cmake
if(WIN32)
    install(TARGETS hidapi_winapi RUNTIME DESTINATION "${INSTALL_BINDIR}")
endif()
```

But there is no equivalent `elseif(APPLE)` branch.

## Fix

Add a macOS install rule for the `hidapi_darwin` shared library target in [CMakeLists.txt](CMakeLists.txt), inside the `EMOTIVLSL_BUILD_EMOTIV` install block (lines 202–209).

Since `INSTALL_BINDIR` is `.` on macOS, this places `libhidapi.0.dylib` alongside `emotiv_lsl`, satisfying the `@executable_path` RPATH entry.

The changed block will look like:

```cmake
if(EMOTIVLSL_BUILD_EMOTIV)
    install(TARGETS emotiv_lsl RUNTIME DESTINATION "${INSTALL_BINDIR}")
    if(WIN32)
        install(TARGETS hidapi_winapi RUNTIME DESTINATION "${INSTALL_BINDIR}")
    elseif(APPLE)
        install(TARGETS hidapi_darwin LIBRARY DESTINATION "${INSTALL_BINDIR}")
    endif()
endif()
```

After re-running `cmake --install build --prefix build/install`, `libhidapi.0.dylib` will be present in `build/install/` and the binary will launch successfully.