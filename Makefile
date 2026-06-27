.PHONY: all setup reconfigure build install test test-all clean format help

MODE ?= release
BUILDDIR ?= builddir$(if $(filter debug,$(MODE)),_debug)

all: setup build

help:
	@echo "Targets:"
	@echo "  setup               - Create venv and configure meson (MODE=release)"
	@echo "  reconfigure         - Reconfigure meson"
	@echo "  build               - Build the C extension"
	@echo "  format              - Run all source formatters"
	@echo "  test                - Run tests until 3 fail"
	@echo "  test-all            - Run all the tests"
	@echo "  clean               - Remove build artifacts (keeps venv)"
	@echo ""
	@echo "Variables (with current values):"
	@echo "  MODE=$(MODE)  BUILDDIR=$(BUILDDIR)"
	@echo ""
	@echo "Use MODE=debug for debug builds: make setup MODE=debug"

setup:
	uv venv
	uv pip install -q meson-python ninja
	uv run meson setup $(BUILDDIR) --buildtype=$(MODE)
	cp $(BUILDDIR)/compile_commands.json compile_commands.json

reconfigure:
	uv run meson setup $(BUILDDIR) --buildtype=$(MODE) --reconfigure
	cp $(BUILDDIR)/compile_commands.json compile_commands.json

build:
	uv run meson compile -C $(BUILDDIR)
	uv build --wheel

format:
	uv run pre-commit run --all-files

install:
	uv pip install -e .

test: install
	uv pip install -q pytest
	uv run pytest -v --maxfail 3 tests/

test-all: install
	uv pip install -q pytest
	uv run pytest -q tests/

clean:
	-uv run meson compile -C builddir --clean 2>/dev/null
	-uv run meson compile -C builddir_debug --clean 2>/dev/null
	rm -rf builddir builddir_debug compile_commands.json dist .venv
