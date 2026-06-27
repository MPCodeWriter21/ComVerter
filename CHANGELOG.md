# Changelog

## [0.1.0] — 2026-06-27

### Added

- CommonMark 0.31.2 Markdown-to-HTML conversion via `markdown_to_html()`:
  ATX and setext headings, paragraphs, emphasis, strong emphasis, code
  spans, fenced code blocks, blockquotes, unordered and ordered lists,
  thematic breaks, hard line breaks, backslash escapes, HTML auto-escaping,
  links, and images.
- GFM extensions: autolink (`"autolink"`) and tagfilter (`"tagfilter"`).
- GFM pipe tables with optional alignment.
- Python C extension module `_comverter` exposing `markdown_to_html`.
- Comprehensive test suite covering all 731 CommonMark + GFM spec examples.
- Build system based on `meson-python` with Makefile convenience targets.
