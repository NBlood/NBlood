#!/bin/bash
#
# NBlood Build Script for Apple Silicon (ARM64)
# macOS Sequoia and later
#
# This script automates the compilation process for NBlood on Apple Silicon Macs
# (M1, M2, M3, M4 chips) running macOS Sequoia (15.x) or later.
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}NBlood Apple Silicon (ARM64) Build Script${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Detect architecture
ARCH=$(uname -m)
if [[ "$ARCH" != "arm64" ]]; then
    echo -e "${YELLOW}Warning: Detected architecture is $ARCH, not arm64${NC}"
    echo -e "${YELLOW}This script is optimized for Apple Silicon (M1/M2/M3/M4)${NC}"
    echo -e "${YELLOW}Build may still work but won't use ARM-specific optimizations${NC}"
    echo ""
fi

# Detect macOS version
MACOS_VERSION=$(sw_vers -productVersion)
MACOS_MAJOR=$(echo $MACOS_VERSION | cut -d. -f1)
echo -e "${GREEN}✓ macOS Version: $MACOS_VERSION${NC}"

if [[ $MACOS_MAJOR -lt 13 ]]; then
    echo -e "${YELLOW}Warning: macOS $MACOS_VERSION detected. Recommended: macOS 13.0 or later${NC}"
fi

# Check for Xcode Command Line Tools
if ! xcode-select -p &>/dev/null; then
    echo -e "${RED}✗ Xcode Command Line Tools not found${NC}"
    echo -e "${YELLOW}Installing Xcode Command Line Tools...${NC}"
    xcode-select --install
    echo -e "${YELLOW}Please run this script again after installation completes${NC}"
    exit 1
else
    echo -e "${GREEN}✓ Xcode Command Line Tools installed${NC}"
fi

# Check for Homebrew
if ! command -v brew &>/dev/null; then
    echo -e "${RED}✗ Homebrew not found${NC}"
    echo -e "${YELLOW}Please install Homebrew from https://brew.sh${NC}"
    echo -e "${YELLOW}Run: /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\"${NC}"
    exit 1
else
    echo -e "${GREEN}✓ Homebrew installed: $(brew --version | head -n1)${NC}"
fi

# Function to check and install dependencies
check_dependency() {
    local dep=$1
    local brew_package=$2

    if command -v "$dep" &>/dev/null; then
        echo -e "${GREEN}✓ $dep found: $(command -v $dep)${NC}"
        return 0
    else
        echo -e "${YELLOW}! $dep not found${NC}"
        if [[ -n "$brew_package" ]]; then
            echo -e "${YELLOW}  Installing $brew_package via Homebrew...${NC}"
            brew install "$brew_package" || {
                echo -e "${RED}✗ Failed to install $brew_package${NC}"
                return 1
            }
            echo -e "${GREEN}✓ $brew_package installed${NC}"
        else
            echo -e "${RED}✗ Please install $dep manually${NC}"
            return 1
        fi
    fi
}

echo ""
echo -e "${BLUE}Checking dependencies...${NC}"

# Check for required build tools
check_dependency "make" "" || exit 1
check_dependency "gcc" "" || check_dependency "clang" "" || exit 1
check_dependency "pkg-config" "pkg-config" || exit 1
check_dependency "nasm" "nasm" || echo -e "${YELLOW}  NASM not required for ARM64 builds (assembly disabled)${NC}"

# Check for SDL2
echo ""
echo -e "${BLUE}Checking SDL2...${NC}"
if pkg-config --exists sdl2; then
    SDL2_VERSION=$(pkg-config --modversion sdl2)
    echo -e "${GREEN}✓ SDL2 found: version $SDL2_VERSION${NC}"
else
    echo -e "${YELLOW}! SDL2 not found${NC}"
    echo -e "${YELLOW}  Installing SDL2 via Homebrew...${NC}"
    brew install sdl2 || {
        echo -e "${RED}✗ Failed to install SDL2${NC}"
        exit 1
    }
    echo -e "${GREEN}✓ SDL2 installed${NC}"
fi

# Check for FLAC
echo ""
echo -e "${BLUE}Checking FLAC...${NC}"
if pkg-config --exists flac; then
    FLAC_VERSION=$(pkg-config --modversion flac)
    echo -e "${GREEN}✓ FLAC found: version $FLAC_VERSION${NC}"
else
    echo -e "${YELLOW}! FLAC not found${NC}"
    echo -e "${YELLOW}  Installing FLAC via Homebrew...${NC}"
    brew install flac || {
        echo -e "${RED}✗ Failed to install FLAC${NC}"
        exit 1
    }
    echo -e "${GREEN}✓ FLAC installed${NC}"
fi

# Check for Vorbis
echo ""
echo -e "${BLUE}Checking Vorbis...${NC}"
if pkg-config --exists vorbis; then
    VORBIS_VERSION=$(pkg-config --modversion vorbis)
    echo -e "${GREEN}✓ Vorbis found: version $VORBIS_VERSION${NC}"
else
    echo -e "${YELLOW}! Vorbis not found${NC}"
    echo -e "${YELLOW}  Installing libvorbis via Homebrew...${NC}"
    brew install libvorbis || {
        echo -e "${RED}✗ Failed to install libvorbis${NC}"
        exit 1
    }
    echo -e "${GREEN}✓ Vorbis installed${NC}"
fi

# Check for VPX (optional but recommended)
echo ""
echo -e "${BLUE}Checking libvpx (optional)...${NC}"
if pkg-config --exists vpx; then
    VPX_VERSION=$(pkg-config --modversion vpx)
    echo -e "${GREEN}✓ libvpx found: version $VPX_VERSION${NC}"
else
    echo -e "${YELLOW}! libvpx not found (optional, for video playback)${NC}"
    read -p "Install libvpx? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        brew install libvpx || echo -e "${YELLOW}  Continuing without libvpx${NC}"
    fi
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Starting Build Process${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Set build environment variables for Apple Silicon
export ARCH=arm64
export PLATFORM=DARWIN

# Determine number of CPU cores for parallel build
if command -v sysctl &>/dev/null; then
    CORES=$(sysctl -n hw.ncpu)
else
    CORES=4
fi

echo -e "${GREEN}Building with $CORES parallel jobs${NC}"
echo ""

# Clean previous builds (optional)
read -p "Clean previous build artifacts? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}Cleaning previous builds...${NC}"
    make cleanblood || true
    echo -e "${GREEN}✓ Clean complete${NC}"
    echo ""
fi

# Build NBlood
echo -e "${BLUE}Building NBlood for Apple Silicon...${NC}"
echo ""

if make blood -j$CORES PLATFORM=DARWIN ARCH=arm64; then
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}✓ Build Successful!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""

    # Check if binary was created
    if [[ -f "nblood" ]]; then
        echo -e "${GREEN}NBlood binary created: ./nblood${NC}"

        # Verify it's an ARM64 binary
        if file nblood | grep -q "arm64"; then
            echo -e "${GREEN}✓ Verified: Native ARM64 binary${NC}"
        else
            echo -e "${YELLOW}⚠ Warning: Binary architecture check failed${NC}"
        fi

        # Show binary info
        echo ""
        echo -e "${BLUE}Binary information:${NC}"
        file nblood
        ls -lh nblood
        echo ""
        echo -e "${BLUE}To run NBlood:${NC}"
        echo -e "  ./nblood"
        echo ""
        echo -e "${BLUE}To install to /usr/local/bin:${NC}"
        echo -e "  sudo cp nblood /usr/local/bin/"
        echo ""
    else
        echo -e "${YELLOW}⚠ Warning: Binary not found in expected location${NC}"
        echo -e "${YELLOW}Check the obj/ directory for build output${NC}"
    fi
else
    echo ""
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}✗ Build Failed${NC}"
    echo -e "${RED}========================================${NC}"
    echo ""
    echo -e "${YELLOW}Troubleshooting:${NC}"
    echo -e "  1. Check that all dependencies are installed"
    echo -e "  2. Make sure you have the latest Xcode Command Line Tools"
    echo -e "  3. Try running 'make cleanblood' and building again"
    echo -e "  4. Check the build output above for specific errors"
    echo ""
    exit 1
fi

echo -e "${BLUE}Build script completed${NC}"
