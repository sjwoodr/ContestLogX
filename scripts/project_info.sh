#!/bin/bash
# Project statistics script for ContestLogX

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║          ContestLogX Project Statistics                        ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Source code statistics
echo "📊 SOURCE CODE"
echo "─────────────────────────────────────────────────────────────────"
src_lines=$(find src include -type f \( -name "*.cpp" -o -name "*.h" \) 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}')
cpp_files=$(find src include -type f -name "*.cpp" 2>/dev/null | wc -l)
h_files=$(find src include -type f -name "*.h" 2>/dev/null | wc -l)
echo "  Lines of Code:        $src_lines"
echo "  C++ Source Files:     $cpp_files"
echo "  Header Files:         $h_files"
echo ""

# Documentation
echo "📖 DOCUMENTATION"
echo "─────────────────────────────────────────────────────────────────"
doc_lines=$(find docs -type f -name "*.md" 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}')
readme_lines=$(wc -l < README.md 2>/dev/null || echo 0)
echo "  Lines in README.md:   $readme_lines"
echo "  Lines in docs/:       $doc_lines"
echo ""

# Test Statistics
echo "🧪 TESTS"
echo "─────────────────────────────────────────────────────────────────"
test_files=$(find tests -type f -name "test_*.cpp" 2>/dev/null | wc -l)
test_lines=$(find tests -type f -name "test_*.cpp" 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}')
test_cases=$(find build/tests -type f -executable 2>/dev/null | while read exe; do "$exe" 2>&1 | grep "Totals:" | grep -oE "[0-9]+ passed" | grep -oE "[0-9]+"; done | awk '{sum+=$1} END {print sum}')
test_cases=${test_cases:-0}
echo "  Test Files:           $test_files"
echo "  Lines of Test Code:   $test_lines"
echo "  Test Cases:           $test_cases"
echo ""

# Contest Definitions
echo "🏆 CONTEST DEFINITIONS"
echo "─────────────────────────────────────────────────────────────────"
contest_files=$(find contests -type f -name "*.json" 2>/dev/null | wc -l)
contest_lines=$(find contests -type f -name "*.json" 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}')
contest_names=$(find contests -type f -name "*.json" 2>/dev/null | sed 's|.*/||; s|\.json||' | tr '\n' ',' | sed 's/,$//')
echo "  Contest Definitions:  $contest_files"
echo "  Lines in Definitions: $contest_lines"
echo "  Contests:"
echo "    $contest_names" | fold -w 65 -s | sed 's/^/    /'
echo ""

# Test Logs
echo "📝 TEST LOGS"
echo "─────────────────────────────────────────────────────────────────"
log_files=$(find test_logs -type f -name "*.clx" 2>/dev/null | wc -l)
log_lines=$(find test_logs -type f -name "*.clx" 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}')
log_qsos=$(find test_logs -type f -name "*.clx" 2>/dev/null | while read f; do grep -c '"id":' "$f" 2>/dev/null || true; done | awk '{sum+=$1} END {print sum}')
echo "  Test Log Files:       $log_files"
echo "  Lines in Test Logs:   $log_lines"
echo "  Total QSOs in Logs:   $log_qsos"
echo ""

# Build Statistics
echo "🔨 BUILD"
echo "─────────────────────────────────────────────────────────────────"
cmake_version=$(cmake --version 2>/dev/null | head -1)
qt_version=$(qmake --version 2>/dev/null | grep -oE "Qt version [0-9.]*" || echo "Not found")
build_time=$(stat -c %y build/Makefile 2>/dev/null | cut -d' ' -f1-2 || echo "Not built")
echo "  CMake:                $cmake_version"
echo "  Qt:                   $qt_version"
echo "  Build Generated:      $build_time"
echo ""

# Repository
echo "📦 REPOSITORY"
echo "─────────────────────────────────────────────────────────────────"
git_branch=$(git branch --show-current 2>/dev/null || echo "Not a git repo")
git_commit=$(git rev-parse --short HEAD 2>/dev/null || echo "N/A")
git_status=$(git status --short 2>/dev/null | wc -l)
echo "  Branch:               $git_branch"
echo "  Commit:               $git_commit"
echo "  Uncommitted Changes:  $git_status files"
echo ""

echo "═════════════════════════════════════════════════════════════════"
