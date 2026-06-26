.PHONY: all setup reconfigure build install test test-all clean help

BUILDDIR ?= builddir

all: setup build

help:
	@echo "Targets:"
	@echo "  setup       - Create venv and configure meson"
	@echo "  reconfigure - Reconfigure meson"
	@echo "  build       - Build the C extension"
	@echo "  test        - Run tests until 3 fail"
	@echo "  test-all    - Run all the tests"
	@echo "  clean       - Remove build artifacts (keeps venv)"
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

install:
	uv pip install -e .

test: install
	uv pip install -q pytest
	uv run pytest -v --maxfail 3 tests/

test-all: install
	uv pip install -q pytest
	uv run pytest -q tests/

clean:
	-uv run meson compile -C $(BUILDDIR) --clean 2>/dev/null
	rm -rf $(BUILDDIR) compile_commands.json dist .venv
