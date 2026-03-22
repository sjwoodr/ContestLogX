# Top-level Makefile
#
# Usage:
#   make              - Build with parallel jobs (default: nproc)
#   make JOBS=4       - Build with 4 parallel jobs
#   make info         - Display project statistics
#   make test         - Run unit tests
#   make test-logs    - Run automated contest log tests (parallel, default 4 workers)
#   make test-logs-headless - Run automated contest log tests in headless mode (no display required)
#   make test-logs-headless WORKERS=8 - Run with 8 parallel workers
#   make clean        - Clean build artifacts
#   make version      - Update version number
#   make reset        - Reset application settings
#   make appimage     - Build portable AppImage via Docker (output: dist/)
#   make macos        - Build macOS app bundle (requires macOS)

# Number of parallel build jobs (default: number of CPU cores)
JOBS ?= $(shell nproc)

# Number of parallel test log runners (default: 4)
WORKERS ?= 4

# Local desktop integration paths
DESKTOP_DIR = $(HOME)/.local/share/applications
ICON_DIR = $(HOME)/.local/share/icons/hicolor/256x256/apps

# Default target
all: build/Makefile
	$(MAKE) -C build -j$(JOBS) && ln -sf build/ContestLogX ./clx
	@$(MAKE) --no-print-directory install-desktop

# Install desktop file and icon for local development
install-desktop: $(DESKTOP_DIR)/ContestLogX.desktop $(ICON_DIR)/contestlogx.png

$(DESKTOP_DIR)/ContestLogX.desktop: ContestLogX.desktop $(ICON_DIR)/contestlogx.png
	@mkdir -p $(DESKTOP_DIR)
	sed 's|Icon=contestlogx|Icon=$(ICON_DIR)/contestlogx.png|' $< > $@
	-update-desktop-database $(DESKTOP_DIR) 2>/dev/null

$(ICON_DIR)/contestlogx.png: resources/contestlogx.png
	@mkdir -p $(ICON_DIR)
	cp $< $@
	-gtk-update-icon-cache -f -t $(HOME)/.local/share/icons/hicolor 2>/dev/null

# Generate build system with CMake
build/Makefile:
	mkdir -p build
	cd build && cmake ..

# Clean build artifacts
clean:
	rm -rf build clx

# Display project information and statistics
info:
	@scripts/project_info.sh

# Run unit tests
test: build/Makefile
	@echo "Running unit tests..."
	cd build && ctest --output-on-failure
	@echo "Cleaning up test artifacts..."
	rm -rf build/Testing/

# Run automated contest log tests
test-logs: clx
	@echo "Running automated contest log tests..."
	@python3 -u scripts/run_log_tests.py --workers $(WORKERS)

# Run automated contest log tests in headless mode (no display required)
test-logs-headless: clx
	@echo "Running automated contest log tests in headless mode..."
	@QT_QPA_PLATFORM=offscreen python3 -u scripts/run_log_tests.py --workers $(WORKERS)

# Update version number across all files
version:
	@echo "Current version: $$(grep 'project(ContestLogX VERSION' CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')"
	@read -p "Enter new version number: " NEW_VERSION; \
	if [ -z "$$NEW_VERSION" ]; then \
		echo "Error: Version number cannot be empty"; \
		exit 1; \
	fi; \
	echo "Updating version to $$NEW_VERSION..."; \
	sed -i "s/project(ContestLogX VERSION [0-9.]*/project(ContestLogX VERSION $$NEW_VERSION/" CMakeLists.txt; \
	sed -i "s/ContestLogX v[0-9.]*/ContestLogX v$$NEW_VERSION/" README.md; \
	sed -i 's/Version [0-9.]*\( (Beta)\)/Version '"$$NEW_VERSION"'\1/' src/ui/mainWindow.cpp; \
	sed -i 's/APP_VERSION = "[0-9.]*"/APP_VERSION = "'"$$NEW_VERSION"'"/' src/main.cpp; \
	sed -i "s|releases/download/v[0-9.]*/ContestLogX\.[0-9.]*\.|releases/download/v$$NEW_VERSION/ContestLogX.$$NEW_VERSION.|g" web/src/pages/download.astro; \
	sed -i "s/Version [0-9.]* (Beta)/Version $$NEW_VERSION (Beta)/g" web/src/pages/download.astro; \
	sed -i '0,/^## \[/{s/^## \[/## ['"$$NEW_VERSION"']\n\n## [/}' CHANGELOG.md; \
	echo "Version updated to $$NEW_VERSION in:"; \
	echo "  - CMakeLists.txt"; \
	echo "  - README.md"; \
	echo "  - src/main.cpp"; \
	echo "  - src/ui/mainWindow.cpp"; \
	echo "  - web/src/pages/download.astro"; \
	echo "  - CHANGELOG.md"; \
	echo ""; \
	echo "Regenerating CMake cache..."; \
	rm -rf build/CMakeCache.txt build/CMakeFiles; \
	cd build && cmake .. > /dev/null 2>&1; \
	echo "CMake cache regenerated"; \
	echo ""; \
	echo "Updating copyright year..."; \
	cd - && ./scripts/update_copyright.sh; \
	echo ""; \
	echo "Rebuilding with new version..."; \
	$(MAKE) -j$(JOBS); \
	echo ""; \
	echo "✓ Version update and rebuild complete!"

# Reset application settings to initial state
reset:
	@echo "WARNING: This will delete all ContestLogX settings and return the application to its initial state."
	@echo "File: ~/.config/ContestLogX/ContestLogX.json"
	@echo "This includes:"
	@echo "  - Station information (callsign, name, grid, etc.)"
	@echo "  - Window geometry and layout"
	@echo "  - Radio connection settings"
	@echo "  - AD1C cty.dat files"
	@echo "  - CW memories"
	@echo "  - All other preferences"
	@echo ""
	@read -p "Are you sure you want to reset ContestLogX? (yes/no): " CONFIRM; \
	if [ "$$CONFIRM" = "yes" ]; then \
		if [ -f ~/.config/ContestLogX/ContestLogX.json ]; then \
			rm -f ~/.config/ContestLogX/ContestLogX.json; \
			rm -f data/cty.dat; \
			echo "Settings file deleted successfully."; \
			echo "ContestLogX will start with default settings on next launch."; \
		else \
			echo "Settings file not found. Nothing to reset."; \
		fi \
	else \
		echo "Reset cancelled."; \
	fi

# Build AppImage via Docker (Ubuntu 22.04), then zip with version in filename
appimage:
	docker build -f Dockerfile.appimage -t clx-appimage-builder .
	mkdir -p dist
	docker run --rm -v $(CURDIR)/dist:/output clx-appimage-builder
	@VERSION=$$(grep -m1 'project(ContestLogX VERSION' CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/'); \
	APPIMAGE=dist/ContestLogX-x86_64.AppImage; \
	ZIPFILE=dist/ContestLogX-$$VERSION-x86_64.AppImage.zip; \
	echo "Creating $$ZIPFILE ..."; \
	cd dist && zip -j ../$$ZIPFILE ContestLogX-x86_64.AppImage; \
	echo "Done: $$ZIPFILE"

# Build macOS app bundle (requires macOS)
macos:
	@scripts/build-macos-bundle.sh

.PHONY: all clean test test-logs test-logs-headless version reset appimage macos install-desktop
