# Top-level Makefile
#
# Usage:
#   make              - Build with parallel jobs (default: nproc)
#   make JOBS=4       - Build with 4 parallel jobs
#   make test         - Run unit tests
#   make clean        - Clean build artifacts
#   make version      - Update version number
#   make reset        - Reset application settings

# Number of parallel build jobs (default: number of CPU cores)
JOBS ?= $(shell nproc)

# Default target
all: build/Makefile
	$(MAKE) -C build -j$(JOBS) && ln -sf build/ContestLogX ./clx

# Generate build system with CMake
build/Makefile:
	mkdir -p build
	cd build && cmake ..

# Clean build artifacts
clean:
	rm -rf build clx

# Run unit tests
test: build/Makefile
	@echo "Running unit tests..."
	cd build && ctest --output-on-failure

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
	sed -i 's/Version [0-9.]*\( (Alpha)\)/Version '"$$NEW_VERSION"'\1/' src/ui/mainwindow.cpp; \
	sed -i 's/APP_VERSION = "[0-9.]*"/APP_VERSION = "'"$$NEW_VERSION"'"/' src/main.cpp; \
	echo "Version updated to $$NEW_VERSION in:"; \
	echo "  - CMakeLists.txt"; \
	echo "  - README.md"; \
	echo "  - src/main.cpp"; \
	echo "  - src/ui/mainwindow.cpp"

# Reset application settings to initial state
reset:
	@echo "WARNING: This will delete all ContestLogX settings and return the application to its initial state."
	@echo "File: ~/.config/ContestLogX/ContestLogX.json"
	@echo "This includes:"
	@echo "  - Station information (callsign, name, grid, etc.)"
	@echo "  - Window geometry and layout"
	@echo "  - Radio connection settings"
	@echo "  - CW memories"
	@echo "  - All other preferences"
	@echo ""
	@read -p "Are you sure you want to reset ContestLogX? (yes/no): " CONFIRM; \
	if [ "$$CONFIRM" = "yes" ]; then \
		if [ -f ~/.config/ContestLogX/ContestLogX.json ]; then \
			rm -f ~/.config/ContestLogX/ContestLogX.json; \
			echo "Settings file deleted successfully."; \
			echo "ContestLogX will start with default settings on next launch."; \
		else \
			echo "Settings file not found. Nothing to reset."; \
		fi \
	else \
		echo "Reset cancelled."; \
	fi

.PHONY: all clean test version reset
