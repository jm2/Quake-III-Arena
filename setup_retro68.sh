#!/bin/bash
set -e

# Detect OS
OS_NAME=$(uname -s)

# Homebrew/Path setup for macOS
if [ "$OS_NAME" = "Darwin" ]; then
    export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
fi

RETRO68_URL="https://github.com/autc04/Retro68.git"
INSTALL_DIR="$(pwd)/tools/Retro68-build"
SOURCE_DIR="$(pwd)/tools/Retro68-src"
BUILD_WORK_DIR="$(pwd)/tools/Retro68-work"

echo "=========================================="
echo "Retro68 Setup Script"
echo "=========================================="

if [ -f "$INSTALL_DIR/bin/powerpc-apple-macos-gcc" ]; then
    echo "Retro68 appears to be installed in $INSTALL_DIR."
    exit 0
fi

echo "Retro68 not found locally."
echo "NOTE: This script will download and BUILD Retro68 from source."
echo "This process can take 20-60 minutes."
echo "Press Ctrl+C to cancel in 5 seconds..."
sleep 5

# Dependency Checking
REQUIRED_CMDS="cmake git wget bison flex makeinfo"
MISSING_DEPS=0

for cmd in $REQUIRED_CMDS; do
    if ! command -v $cmd &> /dev/null; then
        echo "Error: Required command '$cmd' not found."
        MISSING_DEPS=1
    fi
done

# Check for hfsutils specific binaries
if ! command -v hmount &> /dev/null; then
     echo "Error: hfsutils (hmount) not found."
     MISSING_DEPS=1
fi

if [ $MISSING_DEPS -eq 1 ]; then
    echo "--------------------------------------------------------"
    echo "Please install missing dependencies manually."
    if [ "$OS_NAME" = "Darwin" ]; then
        echo "On macOS (Homebrew): brew install cmake boost gmp mpfr libmpc autoconf automake bison flex texinfo wget hfsutils"
    elif [ "$OS_NAME" = "Linux" ]; then
        echo "On Debian/Ubuntu: sudo apt install build-essential cmake libboost-all-dev libgmp-dev libmpfr-dev libmpc-dev autoconf automake bison flex texinfo hfsutils wget"
    fi
    echo "--------------------------------------------------------"
    exit 1
fi

echo "Cloning Retro68..."
mkdir -p tools
if [ ! -d "$SOURCE_DIR" ]; then
    git clone --recursive "$RETRO68_URL" "$SOURCE_DIR"
else
    echo "Source directory exists, pulling updates..."
    cd "$SOURCE_DIR"
    git pull
    git submodule update --init --recursive
    cd -
fi

# Broad patch: Remove "system" component from all Boost find_package calls in the source tree
echo "Patching CMakeLists.txt files to remove Boost::system dependency..."
if [ "$OS_NAME" = "Darwin" ]; then
    find "$SOURCE_DIR" -name "CMakeLists.txt" -exec sed -i '' '/find_package(Boost/s/ system//g' {} +
else
    find "$SOURCE_DIR" -name "CMakeLists.txt" -exec sed -i '/find_package(Boost/s/ system//g' {} +
fi

# Boost paths
if [ "$OS_NAME" = "Darwin" ] && command -v brew &> /dev/null; then
    export BOOST_ROOT="$(brew --prefix boost)"
    export Boost_INCLUDE_DIR="$(brew --prefix boost)/include"
    export Boost_LIBRARY_DIR="$(brew --prefix boost)/lib"
elif [ "$OS_NAME" = "Linux" ]; then
    echo "Using system Boost."
fi

echo "Building Retro68 Toolchain..."
mkdir -p "$BUILD_WORK_DIR"
cd "$BUILD_WORK_DIR"

# Call the official build script
bash "$SOURCE_DIR/build-toolchain.bash" --prefix="$INSTALL_DIR" --clean-after-build

echo "Retro68 installed to $INSTALL_DIR"
