# Emotiv LSL C++

A high-performance C++ implementation of the `emotiv_lsl` server. It uses `hidapi`, `liblsl`, and `tiny-AES-c` to directly interface with Emotiv headsets via USB, decrypt the data streams, and publish them to LSL, without the overhead of Python.

## Features

- **Modern CMake** (3.28+) with clean, documented structure
- **Automatic dependency management** via CMake FetchContent (hidapi, liblsl, tiny-AES-c)
- **CLI and GUI separation** with shared core library
- **Qt6** for the GUI (with Qt5 intentionally dropped for simplicity)
- **Cross-platform**: Linux, macOS, Windows
- **macOS code signing** with entitlements for network capabilities
- **Automated CI/CD** via GitHub Actions

## Project Structure

```
emotiv-lsl-cpp/
├── CMakeLists.txt           # Root build configuration
├── app.entitlements         # macOS network capabilities
├── LSLTemplate.cfg          # Default configuration file
├── src/
│   ├── emotiv/              # Emotiv LSL application (emotiv_lsl)
│   │   ├── config.h             # Sampling rate constants
│   │   ├── emotiv_base.h/cpp    # Base class: HID access, LSL outlets, decryption
│   │   ├── emotiv_epoc_x.h/cpp  # Emotiv EPOC X headset implementation
│   │   ├── main.cpp             # Entry point
│   │   └── CMakeLists.txt
│   ├── core/                # Qt-independent core library (LSL template)
│   │   ├── include/lsltemplate/
│   │   │   ├── Device.hpp       # Device interface
│   │   │   ├── LSLOutlet.hpp    # LSL outlet wrapper
│   │   │   ├── Config.hpp       # Configuration management
│   │   │   └── StreamThread.hpp # Background streaming
│   │   └── src/
│   ├── cli/                 # Command-line application (LSL template)
│   │   └── main.cpp
│   └── gui/                 # Qt6 GUI application (LSL template)
│       ├── MainWindow.hpp/cpp
│       ├── MainWindow.ui
│       └── main.cpp
├── scripts/
│   └── sign_and_notarize.sh # macOS signing script
└── .github/workflows/
    └── build.yml            # CI/CD workflow
```

## Building the Emotiv LSL Application

### Prerequisites

- CMake 3.28 or later
- A C++17 compatible compiler (MSVC, GCC, or Clang)
- Git (for FetchContent to download dependencies automatically)
- Qt6.8 (only required for the GUI build; pass `-DLSLTEMPLATE_BUILD_GUI=OFF` to skip)

### Installing Qt 6.8 on Ubuntu

Ubuntu's default repositories don't include Qt 6.8. Use [aqtinstall](https://github.com/miurahr/aqtinstall) to install it:

```bash
# Install aqtinstall
pip install aqtinstall

# Install Qt 6.8 (adjust version as needed)
aqt install-qt linux desktop 6.8.3 gcc_64 -O ~/Qt

# Set environment for CMake to find Qt
export CMAKE_PREFIX_PATH=~/Qt/6.8.3/gcc_64

# Install system dependencies
sudo apt-get install libgl1-mesa-dev libxkbcommon-dev libxcb-cursor0
```

To run the GUI application, ensure Qt libraries are in your library path:

```bash
export LD_LIBRARY_PATH=~/Qt/6.8.3/gcc_64/lib:$LD_LIBRARY_PATH
```

Alternatively, build CLI-only with `-DLSLTEMPLATE_BUILD_GUI=OFF` to avoid the Qt dependency.

### Quick Start

```bash
# Clone and build
git clone https://github.com/CommanderPho/emotiv-lsl-cpp.git
cd emotiv-lsl-cpp

# Configure — all dependencies (hidapi, liblsl, tiny-AES-c) are fetched automatically
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLSLTEMPLATE_BUILD_GUI=OFF

# Build
cmake --build build --config Release --parallel

# Install
cmake --install build --prefix build/install
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `EMOTIVLSL_BUILD_EMOTIV` | ON | Build the Emotiv LSL application |
| `LSLTEMPLATE_BUILD_GUI` | ON | Build the GUI application (requires Qt6) |
| `LSLTEMPLATE_BUILD_CLI` | ON | Build the CLI application |
| `LSL_FETCH_REF` | v1.17.4 | liblsl git ref to fetch (tag, branch, or commit) |
| `LSL_INSTALL_ROOT` | - | Path to a pre-installed liblsl |

### Emotiv-only Build (no Qt required)

```bash
cmake -S . -B build -DLSLTEMPLATE_BUILD_GUI=OFF -DLSLTEMPLATE_BUILD_CLI=OFF
# Build
cmake --build build --config Release --parallel
# Install
cmake --install build --prefix build/install

```

### liblsl Discovery Priority

The build system searches for liblsl in this order:

1. **LSL_INSTALL_ROOT** - Explicit installation path
2. **FetchContent** - Automatic download from GitHub

### Building with Local liblsl

For parallel development with liblsl:

```bash
cmake -S . -B build -DLSL_SOURCE_DIR=/path/to/liblsl
```

## Running the Emotiv LSL Server

Connect your Emotiv headset dongle before running. The server will automatically locate the device, set up LSL outlets, and begin streaming EEG, motion, and electrode quality data.

**Windows (PowerShell):**
```powershell
# If you used the preset (recommended): self-contained install
.\build\install\emotiv_lsl.exe

# If you built manually without install
.\build\Release\emotiv_lsl.exe
```

**Linux / macOS:**
```bash
# If you used the preset (recommended): self-contained install
./build/install/emotiv_lsl

# If you built manually without install
./build/emotiv_lsl
```

You can use standard LSL tools like `bsl_stream_viewer` to visualize the incoming data streams.

## Usage

### GUI Application

```bash
./LSLTemplate                    # Use default config
./LSLTemplate myconfig.cfg       # Use custom config
```

### CLI Application

```bash
./LSLTemplateCLI --help
./LSLTemplateCLI --name MyStream --rate 256 --channels 8
./LSLTemplateCLI --config myconfig.cfg
```

## macOS Code Signing

For local development, the build automatically applies ad-hoc signing with network entitlements. This allows the app to use LSL's multicast discovery.

For distribution, use the signing script:

```bash
# Sign only
./scripts/sign_and_notarize.sh build/install/LSLTemplate.app

# Sign and notarize
export APPLE_CODE_SIGN_IDENTITY_APP="Developer ID Application: Your Name"
export APPLE_NOTARIZE_KEYCHAIN_PROFILE="your-profile"
./scripts/sign_and_notarize.sh build/install/LSLTemplate.app --notarize
```

## GitHub Actions Secrets

For automated signing and notarization, the workflow expects these secrets from the `labstreaminglayer` organization:

| Secret | Description |
|--------|-------------|
| `PROD_MACOS_CERTIFICATE` | Base64-encoded Developer ID Application certificate (.p12) |
| `PROD_MACOS_CERTIFICATE_PWD` | Certificate password |
| `PROD_MACOS_CI_KEYCHAIN_PWD` | Password for temporary CI keychain |
| `PROD_MACOS_NOTARIZATION_APPLE_ID` | Apple ID email for notarization |
| `PROD_MACOS_NOTARIZATION_PWD` | App-specific password for Apple ID |
| `PROD_MACOS_NOTARIZATION_TEAM_ID` | Apple Developer Team ID |

**Important:** These organization secrets must be shared with your repository. In GitHub:
1. Go to Organization Settings → Secrets and variables → Actions
2. For each secret, click to edit and under "Repository access" select the repositories that need access

### Single-command build and run

From the repo root, one command can configure, build, install, and start the server. Dependencies (liblsl, hidapi, tiny-AES-c) are fetched automatically. Requires CMake 3.25+ and PowerShell Core (`pwsh`) for the run script.

**Windows (PowerShell):**

```powershell
# Build self-contained install only
cmake --workflow --preset emotiv
cmake --install build --config Release

# Build and start collecting data (single command)
pwsh ./run.ps1
```

**Linux / macOS:**
`brew install --cask powershell`

```bash
# Build self-contained install only
cmake --workflow --preset emotiv
cmake --install build --config Release

# Build and start collecting data (single command)
pwsh ./run.ps1
```

The preset builds only `emotiv_lsl` (no Qt/CLI). The executable and bundled LSL library end up in `build/install/`.
