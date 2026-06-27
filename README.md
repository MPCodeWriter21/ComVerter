ComVerter - The Common Converter
================================

ComVerter is a simple C Python extension that I plan to use for converting some simple
formats to others. Like Markdown to HTML, HTML to PDF, and many others.

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
>>> from comverter import markdown_to_html

>>> markdown_to_html("Hello, **world**!")
'<p>Hello, <strong>world</strong>!</p>\n'

# Enable GFM extensions
>>> markdown_to_html("http://example.com", extensions=["autolink"])
'<p><a href=\"http://example.com\">http://example.com</a></p>\n'

>>> markdown_to_html("<x>", extensions=["tagfilter"])
'<p>&lt;x&gt;</p>\n'
```

### Supported Markdown features

- **ATX headings** — `# H1` through `###### H6`
- **Paragraphs** — consecutive lines of text
- **Emphasis** — `*italic*` and `_italic_`
- **Strong emphasis** — `**bold**` and `__bold__`
- **Code spans** — `` `code` ``
- **Links** — `[text](url)` with optional `"title"`
- **Images** — `![alt](src)` with optional `"title"`
- **Fenced code blocks** — `` ``` `` and `` ~~~ ``
- **Blockquotes** — `> quote`
- **Unordered lists** — `- item`, `* item`, `+ item`
- **Ordered lists** — `1. item`
- **Thematic breaks** — `---`, `***`, `___`
- **Hard line breaks** — two trailing spaces
- **Backslash escapes** — `\*`, `\[`, etc.
- **Automatic HTML escaping** — `<`, `>`, `&`, `"`
- **Setext headings** — `Heading\n===` (level 1) and `Heading\n---` (level 2)
- **GFM pipe tables** — `\| a \| b \|\n\|---\|---\|\n\| 1 \| 2 \|` with optional alignment

### GFM Extensions

Pass the `extensions` parameter with a list of extension names:

| Extension      | Description                                                |
|----------------|------------------------------------------------------------|
| `"autolink"`   | Auto-link URLs and email addresses                         |
| `"tagfilter"`  | Filter out certain HTML tags for security                  |

Development
----------

### Makefile targets

Run `make help` to see all available targets:

| Target            | Description                              |
|-------------------|------------------------------------------|
| `make setup`      | Create venv and configure meson          |
| `make reconfigure`| Reconfigure meson                        |
| `make build`      | Build the C extension and wheel          |
| `make install`    | Install the package in editable mode     |
| `make test`       | Run tests (stop after 3 failures)        |
| `make test-all`   | Run all tests                            |
| `make clean`      | Remove all build artifacts               |

### LSP

Once meson is configured (`make setup`), `compile_commands.json` is
generated automatically, giving you completions, diagnostics, and
go-to-definition for the Python C API.

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
- [GitHub Flavored Markdown](https://github.github.com/gfm/)
- [CommonMark](https://spec.commonmark.org/0.31.2/)
- [Markdown on WikiPedia](https://en.wikipedia.org/wiki/Markdown)
- [HTML Spec](https://html.spec.whatwg.org/)
- [RFC 2854](https://datatracker.ietf.org/doc/html/rfc2854)
- [RFC 7763](https://datatracker.ietf.org/doc/html/rfc7763)
- [RFC 7764](https://datatracker.ietf.org/doc/html/rfc7764)
- [RFC 8118](https://datatracker.ietf.org/doc/html/rfc8118)
