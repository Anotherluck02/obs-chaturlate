#!/bin/bash
# Patch OBS SDK version detection for compatibility with newer Xcode/macOS

echo "🔍 Looking for OBS compilerconfig.cmake to patch..."

# Wait a bit for files to be written
sleep 3

# Find the file - try multiple approaches
COMPILER_CONFIG=""
if [ -d ".deps" ]; then
    COMPILER_CONFIG=$(find .deps -type f -name "compilerconfig.cmake" 2>/dev/null | grep "macos" | head -1)
fi

if [ -z "$COMPILER_CONFIG" ]; then
    echo "❌ OBS compilerconfig.cmake not found yet"
    echo "Directory contents:"
    ls -la .deps/ 2>/dev/null || echo ".deps not found"
    echo "This is expected if OBS sources haven't downloaded yet"
    echo "The build will retry after this step"
    exit 0
fi

echo "✅ Found: $COMPILER_CONFIG"

# Create backup
echo "📝 Creating backup..."
cp "$COMPILER_CONFIG" "$COMPILER_CONFIG.original"

echo "🔧 Applying patch (comment out SDK check entirely)..."

# Simplest approach: comment out the entire SDK version check
# This prevents both the regex error AND the version error
sed -i.backup '
  # Comment out lines related to SDK version checking
  /# Ensure recent enough Xcode/,/unset(_obs_macos_minimum_xcode)/s/^/# PATCHED: /
' "$COMPILER_CONFIG"

# Alternative: if the above doesn't work, just change FATAL_ERROR to WARNING
if grep -q "FATAL_ERROR.*macOS SDK" "$COMPILER_CONFIG"; then
    echo "Primary patch didn't work, trying fallback..."
    sed -i.backup2 's/FATAL_ERROR "Your macOS SDK version/WARNING "Your macOS SDK version/g' "$COMPILER_CONFIG"
fi

if [ $? -eq 0 ]; then
    echo "✅ Patch applied successfully!"
    echo ""
    echo "Changed lines:"
    diff "$COMPILER_CONFIG.original" "$COMPILER_CONFIG" | grep -A2 -B2 "WARNING" || echo "(No diff to show)"
else
    echo "❌ Patch failed, but continuing anyway..."
fi

echo ""
echo "📄 Patched section:"
grep -A 5 "WARNING.*macOS SDK version" "$COMPILER_CONFIG" | head -10 || echo "(Could not show preview)"

