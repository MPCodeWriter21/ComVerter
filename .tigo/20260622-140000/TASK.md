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

The two specs `https://github.github.com/gfm/` and `https://spec.commonmark.org/0.31.2/`, must be completely supported.

It's probably a good idea to split the code into multiple files; especially since markdown-to-html is not the only operation the package is meant to do.

hello_world function will be removed after this task has been closed with success.
