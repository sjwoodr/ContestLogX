#!/bin/bash
# Update copyright year in all source files
# Updates "Copyright (c) 2025, ..." to "Copyright (c) 2025-<current_year>, ..."

CURRENT_YEAR=$(date +%Y)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Updating copyright year to include $CURRENT_YEAR..."

# Find all .h and .cpp files and update copyright lines
find "$SCRIPT_DIR" \
    -type f \
    \( -name "*.h" -o -name "*.cpp" -o -name "README.md" -o -name "DeveloperNotes.md" \) \
    ! -path "*/build/*" \
    ! -path "*/.git/*" \
    ! -path "*/CMakeFiles/*" \
    | while read -r file; do
    
    # Check if file contains copyright line
    if grep -q "Copyright (c)" "$file"; then
        # Update copyright: "2025, " -> "2025-<year>, " (if not already updated)
        # Also handles cases where it already has a year range
        sed -i "s/Copyright (c) 2025\(-[0-9]\+\)\?, by/Copyright (c) 2025-$CURRENT_YEAR, by/g" "$file"
    fi
done

echo "Copyright year updated in all source files."

# Count files updated
FILE_COUNT=$(find "$SCRIPT_DIR" -type f \( -name "*.h" -o -name "*.cpp" \) ! -path "*/build/*" ! -path "*/.git/*" -exec grep -l "Copyright (c) 2025-$CURRENT_YEAR" {} \; | wc -l)

echo "Files with updated copyright: $FILE_COUNT"
