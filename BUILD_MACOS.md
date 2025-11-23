# Building NBlood on macOS

This guide covers building NBlood natively on macOS, with special focus on Apple Silicon (ARM64) support.

## System Requirements

### macOS Version
- **Recommended**: macOS Sequoia (15.x) or later
- **Minimum**: macOS Monterey (12.x)

### Hardware
- **Apple Silicon** (M1/M2/M3/M4): Full native ARM64 support with optimizations
- **Intel**: Also supported with x86_64 optimizations

## Quick Start (Apple Silicon)

For the fastest build experience on Apple Silicon Macs, use the automated build script:

```bash
./build_macos_arm.sh
```

This script will:
- Check and install all required dependencies via Homebrew
- Configure the build for native ARM64
- Compile NBlood with Apple Silicon optimizations
- Verify the resulting binary is native ARM64

## Manual Build Instructions

### 1. Install Prerequisites

#### Xcode Command Line Tools
```bash
xcode-select --install
```

#### Homebrew Package Manager
If not already installed:
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### 2. Install Dependencies

#### Required Dependencies
```bash
# Build tools
brew install pkg-config

# SDL2 (Simple DirectMedia Layer)
brew install sdl2

# Audio libraries
brew install flac libvorbis

# NASM assembler (not needed for ARM64 builds, but useful for Intel)
brew install nasm
```

#### Optional Dependencies
```bash
# libvpx (for video playback support)
brew install libvpx
```

### 3. Build NBlood

#### Apple Silicon (ARM64)
```bash
# Clean previous builds (optional)
make cleanblood

# Build for ARM64 with optimizations
make blood PLATFORM=DARWIN ARCH=arm64 -j$(sysctl -n hw.ncpu)
```

#### Intel (x86_64)
```bash
# Clean previous builds (optional)
make cleanblood

# Build for x86_64
make blood PLATFORM=DARWIN ARCH=x86_64 -j$(sysctl -n hw.ncpu)
```

#### Universal Binary (Both architectures)
```bash
# Build both architectures
make blood PLATFORM=DARWIN ARCH="arm64 x86_64" -j$(sysctl -n hw.ncpu)

# Or use lipo to combine separately built binaries
lipo -create nblood_arm64 nblood_x86_64 -output nblood_universal
```

### 4. Verify the Build

Check that the binary was built for the correct architecture:

```bash
# Check architecture
file nblood

# Expected output for ARM64:
# nblood: Mach-O 64-bit executable arm64

# Run the binary
./nblood
```

## Build Optimizations

### Apple Silicon Optimizations

The build system automatically applies these optimizations for ARM64 builds on macOS:

- **CPU Target**: `-mcpu=apple-a14` (optimized for Apple Silicon)
- **Tuning**: `-mtune=native` (auto-detects M1/M2/M3/M4)
- **Architecture**: `-arch arm64` (native ARM64)
- **No Assembly**: Assembly code is automatically disabled on ARM64, using optimized C implementations instead

### Performance Notes

Native ARM64 builds on Apple Silicon typically show:
- **No Rosetta overhead**: Direct native execution
- **Better power efficiency**: ARM-optimized code paths
- **Improved performance**: Up to 2x faster than Rosetta 2 translation on M1/M2

## Troubleshooting

### Homebrew Not in PATH

If Homebrew commands aren't found, add to your `~/.zshrc` or `~/.bash_profile`:

```bash
# For Apple Silicon
eval "$(/opt/homebrew/bin/brew shellenv)"

# For Intel
eval "$(/usr/local/bin/brew shellenv)"
```

### SDL2 Not Found

If `pkg-config` can't find SDL2:

```bash
# Check SDL2 installation
brew list sdl2

# Reinstall if needed
brew reinstall sdl2

# Verify pkg-config can find it
pkg-config --modversion sdl2
```

### Build Errors with "Unknown Architecture"

Ensure you're passing the correct architecture flag:

```bash
# Check your system architecture
uname -m

# arm64 = Apple Silicon
# x86_64 = Intel
```

### Linker Errors

If you encounter linker errors about missing symbols:

```bash
# Clean and rebuild
make cleanblood
make blood PLATFORM=DARWIN -j$(sysctl -n hw.ncpu)
```

### Permission Denied on build_macos_arm.sh

Make the script executable:

```bash
chmod +x build_macos_arm.sh
```

## Advanced Build Options

### Debug Build

For development with debug symbols:

```bash
make blood RELEASE=0 PLATFORM=DARWIN ARCH=arm64
```

### Without Optimizations

To build without architecture-specific optimizations:

```bash
make blood PACKAGE_REPOSITORY=1 PLATFORM=DARWIN
```

### Custom Compiler

To use a specific compiler:

```bash
make blood CC=clang CXX=clang++ PLATFORM=DARWIN
```

### Enable All Features

```bash
make blood \
    PLATFORM=DARWIN \
    ARCH=arm64 \
    NOONE_EXTENSIONS=1 \
    USE_OPENGL=1 \
    POLYMER=1 \
    NETCODE=1 \
    -j$(sysctl -n hw.ncpu)
```

## Build Targets

- `blood` - Build NBlood only
- `rr` - Build Rednukem only
- `exhumed` - Build PCExhumed only
- `all` - Build all projects (default: blood, rr, exhumed)
- `cleanblood` - Clean NBlood build artifacts
- `clean` - Clean all build artifacts

## Installation

### Local Installation

The binary can be run directly from the build directory:

```bash
./nblood
```

### System-wide Installation

Copy to a system directory (optional):

```bash
sudo cp nblood /usr/local/bin/
```

### Create macOS Application Bundle

You can create a `.app` bundle for easier distribution:

```bash
# The Makefile should automatically create this on Darwin platforms
# Look for NBlood.app in the build directory

# Or manually:
mkdir -p NBlood.app/Contents/MacOS
cp nblood NBlood.app/Contents/MacOS/
```

## Known Issues

### macOS Sequoia Specific

- **Gatekeeper**: First-run may require allowing the app in System Preferences > Privacy & Security
- **SDL2 Compatibility**: Ensure SDL2 2.0.22 or later for best Sequoia compatibility

### Apple Silicon Specific

- **Assembly Code**: Automatically disabled on ARM64 (uses C fallbacks)
- **Rosetta 2**: Not required for native ARM64 builds
- **Universal Binaries**: Requires building for both architectures separately and combining with `lipo`

## Library Paths

The build system automatically checks these locations for dependencies:

1. **Homebrew (Apple Silicon)**: `/opt/homebrew/` (default M1/M2/M3/M4)
2. **Homebrew (Intel)**: `/usr/local/`
3. **MacPorts**: `/opt/local/`
4. **Fink**: `/sw/`

## Additional Resources

- **EDuke32 Wiki**: https://wiki.eduke32.com/
- **Homebrew**: https://brew.sh/
- **SDL2**: https://www.libsdl.org/

## Support

For build issues specific to macOS:
1. Check this guide's troubleshooting section
2. Verify all dependencies are installed
3. Try the automated build script: `./build_macos_arm.sh`
4. Report issues with full build output and system information

---

**Note**: This build system has been optimized for native Apple Silicon compilation on macOS Sequoia. The build automatically detects your architecture and applies appropriate optimizations.
