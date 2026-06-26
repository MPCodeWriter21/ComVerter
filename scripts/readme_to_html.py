from pathlib import Path

from comverter import markdown_to_html

root = Path(__file__).parent

with open(root / "README.md", "r") as file:
    markdown_data = file.read()

html_data = markdown_to_html(markdown_data)

with open(root / "README.ComVerter.html", "w") as file:
    file.write(html_data)
