ComVerter - The Common Converter
================================

ComVerter is a simple C Python extension that I plan to use for converting some simple
formats to others. E.g. Markdown to HTML.

Build & Install (Release)
-------------------------

This project uses [uv](https://docs.astral.sh/uv/) as the release build tool.

```bash
# Build the wheel for the current Python version
uv build --wheel

# Install the wheel
uv pip install dist/comverter-*.whl
```

Usage
-----

```python
>>> from comverter import hello_world
>>> hello_world()
Hello World!
```

Development
----------

### Makefile targets

Run `make help` to see all available targets:

| Target       | Description                              |
|--------------|------------------------------------------|
| `make setup` | Create venv and configure meson          |
| `make build` | Build the C extension                    |
| `make test`  | Run tests                                |
| `make clean` | Remove build artifacts                   |

### LSP / clangd

If your editor supports clangd (Neovim, VS Code, etc.), there's a `.clangd`
config file in the project root. Once meson is configured (`make setup`),
`builddir/compile_commands.json` is generated automatically, giving you
completions, diagnostics, and go-to-definition for the Python C API.

Task Management
---------------

This project uses [Tigo](https://github.com/MPCodeWriter21/Tigo) for task
management. Tasks are stored as `TASK.md` files under `.tigo/YYYYMMDD-HHmmss/`.

Disclaimer
----------

This repository is being developed using AI assistance.

References
----------

- [Python C API](https://docs.python.org/3/c-api/index.html)
- [CommonMark](https://commonmark.org/)
- [Markdown on WikiPedia](https://en.wikipedia.org/wiki/Markdown)
- [RFC 2854](https://datatracker.ietf.org/doc/html/rfc2854)
- [RFC 7763](https://datatracker.ietf.org/doc/html/rfc7763)
- [RFC 7764](https://datatracker.ietf.org/doc/html/rfc7764)
- [RFC 8118](https://datatracker.ietf.org/doc/html/rfc8118)
