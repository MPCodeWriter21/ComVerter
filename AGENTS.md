AGENTS.md — ComVerter Development Guide
=========================================

This file captures the development patterns and preferences. It is intended
for AI agents to maintain consistent behavior across sessions.

Project overview
----------------

ComVerter is a C Python extension for converting between formats (e.g. Markdown
to HTML). It uses `meson-python` as the build backend and `uv` as the release
build tool.

Instructions
------------

- Bump the version (minor or patch) in `pyproject.toml` when commits
  represent a meaningful change in functionality (new feature, breaking
  change, significant fix). Minor for features, patch for bug fixes.

- Task implementation and closing must be in the same commit. Only separate
  them in rare cases (e.g. when a hotfix is needed before the task is fully
  done).

- Never revert/override changes made by the user without asking for confirmation.
- Always ask for clarification if the user request is ambiguous.
- Do not commit anything without running the tests first.
- Do not commit empty files or files that do not provide any value to the project.
- Every new thing needs new tests to be added and run. If you add a new feature,
  add a test for it. If you fix a bug, add a test that reproduces the bug and
  passes after the fix.

Build system
------------

- **Release builds**: use `uv build --wheel`
- **Development builds**: use `make` targets (which use `uv` under the hood)
- **Build backend**: `meson-python` (not setuptools)
- **Python support**: 3.10+
- **Makefile targets**: `setup`, `reconfigure`, `build`, `test`, `wheel`, `clean`, `clean-all`

Commit convention
------------------

When a commit is related to a task, the task ID must appear in the commit
message: `type(Task YYYYMMDD-HHmmss): message`. Commits that are not
related to any task (e.g. chore-only or trivial fixes) do not need to
reference one.

Task management (Tigo)
----------------------

Tasks are managed using [Tigo](https://github.com/MPCodeWriter21/Tigo) and
stored as `TASK.md` files under `.tigo/YYYYMMDD-HHmmss/`. Each task has:

- A title (`# title`)
- STATUS, PRIORITY, TAGS, DUE metadata
- A description with checkpoints

Changes that are significant enough to warrant a new task should be added as
a new `TASK.md` file.

Code conventions
----------------

- C code uses the Python C API with no platform-specific `#ifdef` unless
  absolutely necessary
- Python code is minimal — the C extension does the work
- Tests use `pytest`
- CI runs on GitHub Actions across Python 3.10–3.14
- Docstrings use reStructuredText (Sphinx) format
- Avoid non-trivial characters (e.g. em-dash `—`, curly quotes, non-ASCII
  punctuation) in source code files — stick to ASCII
- Stage files one by one with explicit paths (`git add file1 file2`); do NOT
  use `git add -A` or `git add .` to avoid committing unintended files

Documentation
-------------

- All code changes include documentation (README.md updates, CHANGELOG.md, docstrings, etc.)
- README is the single source of truth for build/install/usage
- Development instructions are expressed as convenience features (Makefile
  targets, optional LSP hints) rather than prescriptive steps

Cross-platform
--------------

- Avoid Windows-specific paths, APIs, or idioms
- Makefile uses POSIX conventions with `VENV_BIN` variable for portability

Debugging with Python code
--------------------------

Do not directly use "python -c ...". Instead, write a temporary script and name
it `_debug_{REASON_FOR_CREATION}.py`, and run it using uv:
`uv run _debug_{REASON_FOR_CREATION}.py`

One general rule to keep in mind is to avoid directly using `python` and
instead running everything python related via uv.
