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
    :param extensions: Extensions to enable.
        Supported values:

        - ``"tagfilter"`` - filter out certain HTML tags
        - ``"autolink"`` - auto-link URLs and email addresses
    :returns: The resulting HTML string.
    :rtype: str
    :raises TypeError: If the input is not a string.
    :raises ValueError: If the input is invalid.
    """

def markdown_file_to_html(
    input_file: str,
    output_file: str,
    extensions: list[_MarkdownExtension] | None = None,
) -> None:
    """Convert a Markdown file to HTML and write to an output file.

    :param input_file: Path to the input Markdown file.
    :param output_file: Path where the HTML output is written.
    :param extensions: Extensions to enable.
    :raises FileNotFoundError: If the input file does not exist.
    :raises OSError: If the output file cannot be written.
    """

ALL_MARKDOWN_EXTENSIONS: list[_MarkdownExtension]
