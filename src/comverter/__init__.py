from comverter._comverter import markdown_to_html as _markdown_to_html


def markdown_to_html(markdown, extensions=None):
    """Convert Markdown text to HTML.

    Args:
        markdown: The Markdown string to convert.
        extensions: Optional list of GFM extensions (e.g. ["tagfilter"]).

    Returns:
        The resulting HTML string.
    """
    if extensions is None:
        extensions = []
    return _markdown_to_html(markdown, extensions)


__all__ = ["markdown_to_html"]
