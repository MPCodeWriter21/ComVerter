"""ComVerter - The Common Converter.

Provides Markdown to HTML conversion following the CommonMark 0.31.2 spec
and GitHub Flavored Markdown.
"""

from typing import Literal

_MarkdownExtension = Literal["tagfilter", "autolink"]

def markdown_to_html(
    markdown: str,
    extensions: list[_MarkdownExtension] | None = None,
) -> str:
    """Convert Markdown text to HTML.

    Implements the CommonMark 0.31.2 spec and GitHub Flavored Markdown.

    :param markdown: The Markdown text to convert.
    :param extensions: GFM extensions to enable.
        Supported values:

        - ``"tagfilter"`` - filter out certain HTML tags
        - ``"autolink"`` - auto-link URLs and email addresses
    :returns: The resulting HTML string.
    :rtype: str
    :raises TypeError: If the input is not a string.
    :raises ValueError: If the input is invalid.
    """

ALL_MARKDOWN_EXTENSIONS: list[_MarkdownExtension]
