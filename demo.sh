#!/bin/bash
# Demo script for whiterose
# Usage: cd whiterose/ && bash demo.sh

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEMO_DIR=$(mktemp -d /tmp/whiterose-demo-XXXXXX)
trap "rm -rf $DEMO_DIR" EXIT

# Build whiterose if needed
WHITEROSE="$SCRIPT_DIR/whiterose"
if [ ! -f "$WHITEROSE" ]; then
    echo "Building whiterose..."
    cd "$SCRIPT_DIR"
    make -s 2>/dev/null || g++ -std=c++17 -Os -s main.cpp -o whiterose -lreadline
    cd "$DEMO_DIR"
fi

cd "$DEMO_DIR"
git init -q
git config user.email "demo@example.com"
git config user.name "Demo"
git commit --allow-empty -q -m "init"
cp "$WHITEROSE" .

echo ""
echo "╔════════════════════════════════════════════════╗"
echo "║                 WHITEROSE DEMO                ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# ─── 1. Boot + status ───
echo "━━━ 1. BOOT + STATUS ━━━"
echo 'status' | timeout 2 ./whiterose 2>/dev/null || true
echo ""

# ─── 2. Push auto-upstream ───
echo "━━━ 2. PUSH AUTO-UPSTREAM ━━━"
git checkout -q -b demo-feature
echo "ctx" > context.txt
git add context.txt
git commit -q -m "demo commit"
cd /tmp && git init --bare -q remote.git && cd - >/dev/null
git remote add origin /tmp/remote.git
echo 'push' | timeout 2 ./whiterose 2>/dev/null || true
echo ""

# ─── 3. Status reformat ───
echo "━━━ 3. STATUS REFORMAT ━━━"
echo "newfile" > new.txt
echo 'status' | timeout 2 ./whiterose 2>/dev/null || true
echo ""

# ─── 4. Force-push protection ───
echo "━━━ 4. FORCE-PUSH PROTECTION ━━━"
git push -u origin demo-feature -q 2>/dev/null || true
cd /tmp
rm -rf other-clone
git clone remote.git other-clone -q 2>/dev/null
cd /tmp/other-clone
git config user.email "o@o" && git config user.name "o"
echo "other" >> context.txt && git add context.txt && git commit -q -m "other"
git push -q 2>/dev/null
cd "$DEMO_DIR"
echo 'git push --force' | timeout 2 ./whiterose 2>/dev/null || true
echo ""

# ─── 5. Undo ───
echo "━━━ 5. BACKUP + UNDO ━━━"
cd /tmp
rm -rf other2-clone
git clone remote.git other2-clone -q 2>/dev/null
cd /tmp/other2-clone
git config user.email "x@x" && git config user.name "x"
echo "more" >> context.txt && git add context.txt && git commit -q -m "more"
git push -q 2>/dev/null
cd "$DEMO_DIR"
git fetch -q origin 2>/dev/null
echo -e 'git reset --hard HEAD~1\nundo' | timeout 3 ./whiterose 2>/dev/null || true
echo ""

# ─── 6. Smart pull ───
echo "━━━ 6. SMART PULL ━━━"
echo 'pull' | timeout 2 ./whiterose 2>/dev/null || true
echo ""

# ─── Cleanup ───
rm -rf /tmp/remote.git /tmp/other-clone /tmp/other2-clone

echo ""
echo "╔════════════════════════════════════════════════╗"
echo "║                 DEMO COMPLETE                 ║"
echo "╚════════════════════════════════════════════════╝"
echo ""
echo "Run whiterose yourself: cd $SCRIPT_DIR && ./whiterose"
