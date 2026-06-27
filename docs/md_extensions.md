Extensions
==========

ComVerter supports the following custom extensions, passed via the
``extensions`` parameter of ``markdown_to_html`` and
``markdown_file_to_html``. Enable all of them at once with
``ALL_MARKDOWN_EXTENSIONS``.

``"tagfilter"``
---------------

Filters out specific HTML tags by replacing the opening ``<`` with
``&lt;``. This prevents execution of certain HTML elements in rendered
output.

The following tags are filtered per the GFM spec:

- ``title``
- ``textarea``
- ``style``
- ``xmp``
- ``iframe``
- ``noembed``
- ``noframes``
- ``script``
- ``plaintext``

**Example:**

```pycon
>>> markdown_to_html("<script>alert(1)</script>", extensions=["tagfilter"])
'&lt;script>alert(1)&lt;/script>\n'
```

``"autolink"``
--------------

Auto-links URLs and email addresses that appear in text without explicit
Markdown link syntax.

**Example:**

```pycon
>>> markdown_to_html("Visit https://example.com", extensions=["autolink"])
'<p>Visit <a href="https://example.com">https://example.com</a></p>\n'
```
