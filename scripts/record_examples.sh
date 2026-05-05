#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/.."
SCREENSHOTS_DIR="$REPO_ROOT/docs/screenshots"
TARGET_DIR="$REPO_ROOT/build/examples"
TAPE_FILE="/tmp/vhs_capture.tape"

mkdir -p "$SCREENSHOTS_DIR"

if [ ! -d "$TARGET_DIR" ]; then
    echo "Error: Directory $TARGET_DIR does not exist. Did you compile?"
    exit 1
fi

cd "$TARGET_DIR"

WRAPPER_CHROME="/tmp/chrome_docker_wrapper"
cat << 'EOF' > "$WRAPPER_CHROME"
#!/bin/sh
exec /usr/bin/google-chrome --no-sandbox --headless --disable-dev-shm-usage --disable-gpu "$@"
EOF
chmod +x "$WRAPPER_CHROME"

# 2. Force VHS to use our wrapper script instead of calling Chrome directly
export VHS_CHROME="$WRAPPER_CHROME"
export VHS_CHROME_FLAGS="--no-sandbox --headless --disable-dev-shm-usage --disable-gpu"
export TERM=xterm-256color

for binary in *; do
    if [ -f "$binary" ] && [ -x "$binary" ]; then
        echo "=================================================="
        echo "Processing via VHS: $binary"
        echo "=================================================="
        
        OUTPUT_GIF="$SCREENSHOTS_DIR/${binary}.gif"

        # DYNAMICALLY GENERATE THE VHS TAPE FILE
        # We start writing the standard terminal window configurations
cat << EOF > "$TAPE_FILE"
# Geometry and Theme configurations
Output "$OUTPUT_GIF"
Set Width 1200
Set Height 600
Set FontSize 12
Set Theme "Dracula"
Set Margin 20
Set MarginFill "#674EFF"
Set BorderRadius 10
Set Padding 5

# Instead of Spawn, we pass the command directly into the shell
Type "./$binary"
Enter
EOF

        # EVALUATE PROCESS TYPE (One-shot vs. Event Loop)
        "./$binary" > /dev/null 2>&1 &
        TEST_PID=$!
        sleep 0.5

        if kill -0 $TEST_PID 2>/dev/null; then
            echo "-> Detected continuous event loop. Injecting interactive macros..."
            kill $TEST_PID 2>/dev/null || true
            
            # Append interactive keystroke rules to the tape file
            # Correct repetition syntax: Key [count] 
            cat << EOF >> "$TAPE_FILE"
Sleep 3s
Tab@1s
Tab@1s
Tab@1s

Down@500ms 10

Sleep 3s
EOF
        else
            echo "-> Detected one-shot utility. Injecting static capture hold..."
            cat << EOF >> "$TAPE_FILE"
Sleep 4s
EOF
        fi

        # EXECUTE THE VHS RENDERING PASS
        echo "Running VHS engine..."
        vhs "$TAPE_FILE"

        # CLEAN UP
        rm -f "$TAPE_FILE"
        echo "✅ Successfully compiled: $OUTPUT_GIF"
    fi
done
