#!/bin/bash

# MSU-1 Prep Studio - Setup Script
# This script downloads JUCE and prepares the build environment

echo "MSU-1 Prep Studio - Setup"
echo "=========================="
echo ""

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "Error: CMake is not installed!"
    echo "Please install CMake 3.22 or higher"
    exit 1
fi

echo "✓ CMake found: $(cmake --version | head -n1)"

# Check for Git
if ! command -v git &> /dev/null; then
    echo "Error: Git is not installed!"
    echo "Please install Git"
    exit 1
fi

echo "✓ Git found"

# Clone JUCE if not present
if [ ! -d "JUCE" ]; then
    echo ""
    echo "Downloading JUCE Framework..."
    git clone --depth 1 --branch 8.0.0 https://github.com/juce-framework/JUCE.git
    
    if [ $? -eq 0 ]; then
        echo "✓ JUCE downloaded successfully"
    else
        echo "Error: Failed to download JUCE"
        exit 1
    fi
else
    echo "✓ JUCE already present"
fi

# Create build directory
echo ""
echo "Creating build directory..."
mkdir -p build

echo ""
echo "Setup complete!"
echo ""
echo "To build the project:"
echo "  cd build"
echo "  cmake .."
echo "  cmake --build . --config Release"
echo ""
