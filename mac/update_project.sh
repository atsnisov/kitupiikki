#!/bin/sh

#  update_project.sh
#  kitsas
#
#  Created by Petri Aarnio on 14/02/2019.
#  

# Find qmake
QMAKE=""

# 1. Check for qmake in PATH
if command -v qmake >/dev/null 2>&1; then
    QMAKE=$(command -v qmake)
    echo "Found qmake in PATH: $QMAKE"
fi

# 2. Check common Homebrew paths if not found yet
if [ -z "$QMAKE" ]; then
    if [ -x "/opt/homebrew/opt/qt/bin/qmake" ]; then
        QMAKE="/opt/homebrew/opt/qt/bin/qmake"
        echo "Found qmake in Homebrew (Apple Silicon): $QMAKE"
    elif [ -x "/usr/local/opt/qt/bin/qmake" ]; then
        QMAKE="/usr/local/opt/qt/bin/qmake"
        echo "Found qmake in Homebrew (Intel): $QMAKE"
    fi
fi

# 3. Check for Qt installations in user's home directory, looking for the latest version
if [ -z "$QMAKE" ]; then
    # This find command is a bit broad, let's try to be more specific
    QMAKE_CANDIDATE=$(find ~/Qt -name qmake -type f -path '*/macos/bin/qmake' -perm +111 2>/dev/null | sort -V | tail -n 1)
    if [ -n "$QMAKE_CANDIDATE" ]; then
        QMAKE=$QMAKE_CANDIDATE
        echo "Found qmake in ~/Qt: $QMAKE"
    fi
fi

# 4. Check for Qt installations in the hardcoded path's parent, looking for the latest version
if [ -z "$QMAKE" ]; then
    QMAKE_CANDIDATE=$(find /Users/dst/Qt -name qmake -type f -path '*/macos/bin/qmake' -perm +111 2>/dev/null | sort -V | tail -n 1)
    if [ -n "$QMAKE_CANDIDATE" ]; then
        QMAKE=$QMAKE_CANDIDATE
        echo "Found qmake in /Users/dst/Qt: $QMAKE"
    fi
fi

if [ -z "$QMAKE" ]; then
    echo "Error: qmake not found."
    echo "Please install Qt, ensure qmake is in your PATH, or set the QMAKE environment variable."
    exit 1
fi

echo "Using qmake at: $QMAKE"
"$QMAKE" -spec macx-xcode ../kitsas/kitsas.pro
