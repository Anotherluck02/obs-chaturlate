#!/bin/bash
# Patch OBS SDK version detection for compatibility with newer Xcode/macOS

set -e

echo "Looking for OBS compilerconfig.cmake to patch..."

COMPILER_CONFIG=$(find .deps -name "compilerconfig.cmake" -path "*/macos/*" 2>/dev/null | head -1)

if [ -z "$COMPILER_CONFIG" ]; then
    echo "Warning: OBS compilerconfig.cmake not found, skipping patch"
    exit 0
fi

echo "Found: $COMPILER_CONFIG"
echo "Patching SDK version detection..."

# Create backup
cp "$COMPILER_CONFIG" "$COMPILER_CONFIG.backup"

# Apply patch using cat and here-document for reliability
cat > /tmp/sdk_patch.txt << 'PATCH'
# Ensure recent enough Xcode and platform SDK
set(_obs_macos_minimum_sdk 14.2) # Keep in sync with Xcode
set(_obs_macos_minimum_xcode 15.1) # Keep in sync with SDK
message(DEBUG "macOS SDK Path: ${CMAKE_OSX_SYSROOT}")
if(CMAKE_OSX_SYSROOT)
  string(REGEX MATCH ".+/MacOSX.platform/Developer/SDKs/MacOSX([0-9]+\\.[0-9]+)\\.sdk$" _ "${CMAKE_OSX_SYSROOT}")
  set(_obs_macos_current_sdk ${CMAKE_MATCH_1})
  message(DEBUG "macOS SDK version: ${_obs_macos_current_sdk}")
  if(_obs_macos_current_sdk AND _obs_macos_current_sdk VERSION_LESS _obs_macos_minimum_sdk)
    message(
      FATAL_ERROR "Your macOS SDK version (${_obs_macos_current_sdk}) is too low. "
                  "The macOS ${_obs_macos_minimum_sdk} SDK (Xcode ${_obs_macos_minimum_xcode}) is required to build OBS.")
  endif()
endif()
unset(_obs_macos_current_sdk)
unset(_obs_macos_minimum_sdk)
unset(_obs_macos_minimum_xcode)
PATCH

# Find the line numbers for the SDK check section
START_LINE=$(grep -n "# Ensure recent enough Xcode" "$COMPILER_CONFIG" | cut -d: -f1)
END_LINE=$(grep -n "unset(_obs_macos_minimum_xcode)" "$COMPILER_CONFIG" | cut -d: -f1)

if [ -z "$START_LINE" ] || [ -z "$END_LINE" ]; then
    echo "Warning: Could not find SDK check section, skipping patch"
    exit 0
fi

echo "Replacing lines $START_LINE to $END_LINE with patched version..."

# Extract parts before and after the section to replace
head -n $((START_LINE - 1)) "$COMPILER_CONFIG.backup" > /tmp/before.txt
tail -n +$((END_LINE + 1)) "$COMPILER_CONFIG.backup" > /tmp/after.txt

# Rebuild the file
cat /tmp/before.txt /tmp/sdk_patch.txt /tmp/after.txt > "$COMPILER_CONFIG"

echo "✅ Patch applied successfully!"
echo "SDK version check will now handle newer Xcode versions"

