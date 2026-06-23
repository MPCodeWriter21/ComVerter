# Implement CommonMark/Markdown to HTML conversion

- STATUS: OPEN
- PRIORITY: 90
- TAGS: core, commonmark, implementation

Implement the core conversion functionality from scratch in pure C with no external libraries and expose it through the Python C extension.

This includes:
- Implementing the latest CommonMark standard
- Implementing a `markdown_to_html(markdown_text: str) -> str` function
- Handling encoding/decoding between Python `str` and C strings
- Adding corresponding Python-level tests for various Markdown inputs
- Updating the `__init__.py` to export the new function
- Adding proper error handling for invalid inputs
