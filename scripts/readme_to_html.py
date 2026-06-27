from pathlib import Path

from comverter import ALL_MARKDOWN_EXTENSIONS, markdown_file_to_html

root = Path(__file__).parent.parent

markdown_file_to_html(
    str(root / "README.md"),
    str(root / "README.ComVerter.html"),
    extensions=ALL_MARKDOWN_EXTENSIONS,
)
