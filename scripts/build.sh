#!/usr/bin/env bash
set -e

# Default to Debug for faster iteration and better error messages
BUILD_TYPE="Debug"
RUN_TESTS=false
CLEAN=false

usage() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  -r, --release    Build in Release mode (default: Debug)"
    echo "  -t, --test       Run unit tests after build"
    echo "  -c, --clean      Remove build directory before building"
    echo "  -h, --help       Show this help message"
    exit 0
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -r|--release) BUILD_TYPE="Release" ;;
        -t|--test) RUN_TESTS=true ;;
        -c|--clean) CLEAN=true ;;
        -h|--help) usage ;;
        *) echo "Unknown parameter passed: $1"; usage ;;
    esac
    shift
done

BUILD_DIR="build"
if [ "$BUILD_TYPE" == "Release" ]; then
    BUILD_DIR="build_release"
fi

if [ "$CLEAN" == true ]; then
    echo "Cleaning build directory: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

echo "--- Configuring ($BUILD_TYPE) ---"
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "--- Building ---"
cmake --build "$BUILD_DIR" -j$(nproc)

if [ "$RUN_TESTS" == true ]; then
    echo "--- Running Tests ---"
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

echo "--- Build Complete ---"
echo "Executable: ./$BUILD_DIR/demo"
