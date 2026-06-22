.PHONY: all setup reconfigure build test clean clean-all help

BUILDDIR ?= builddir

all: build

help:
	@echo "Targets:"
	@echo "  setup       - Create venv and configure meson"
	@echo "  reconfigure - Reconfigure meson"
	@echo "  build       - Build the C extension"
	@echo "  test        - Run tests"
	@echo "  wheel       - Build a pip-installable wheel"
	@echo "  clean       - Remove build artifacts (keeps venv)"
	@echo "  clean-all   - Remove build artifacts and venv"
	@echo ""
	@echo "Variables (with current values):"
	@echo "  BUILDDIR=$(BUILDDIR)"

setup:
	uv venv
	uv pip install -q meson-python ninja
	uv run meson setup $(BUILDDIR) --buildtype=release
	cp $(BUILDDIR)/compile_commands.json compile_commands.json

reconfigure:
	uv run meson setup $(BUILDDIR) --buildtype=release --reconfigure
	cp $(BUILDDIR)/compile_commands.json compile_commands.json

build:
	uv run meson compile -C $(BUILDDIR)
	uv build --wheel

test:
	uv pip install -q pytest
	uv pip install -e .
	uv run pytest -v tests/

clean:
	-uv run meson compile -C $(BUILDDIR) --clean 2>/dev/null
	rm -rf $(BUILDDIR) compile_commands.json dist .venv
