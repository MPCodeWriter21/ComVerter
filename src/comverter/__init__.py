# ComVerter - A conversion tool for common formats

# markdown_to_html(markdown, extensions=None) -> str
from comverter._comverter import markdown_to_html

ALL_MARKDOWN_EXTENSIONS = [
    "tagfilter",
    "autolink",
]

__all__ = ["markdown_to_html"]
