import pytest

from comverter import markdown_to_html


def test_paragraph():
    result = markdown_to_html("Hello, world!")
    assert result == "<p>Hello, world!</p>\n"


def test_multiple_paragraphs():
    result = markdown_to_html("First paragraph.\n\nSecond paragraph.")
    assert result == "<p>First paragraph.</p>\n<p>Second paragraph.</p>\n"


def test_setext_heading_level1():
    result = markdown_to_html("Heading\n===")
    assert result == "<h1>Heading</h1>\n"


def test_setext_heading_level2():
    result = markdown_to_html("Heading\n---")
    assert result == "<h2>Heading</h2>\n"


def test_setext_heading_multiline_text():
    result = markdown_to_html("Multi\nline\ntext\n===")
    assert result == "<h1>Multi\nline\ntext</h1>\n"


def test_setext_heading_with_inline():
    result = markdown_to_html("**bold** text\n===")
    assert result == "<h1><strong>bold</strong> text</h1>\n"


def test_setext_heading_not_paragraph():
    result = markdown_to_html("Paragraph\n\n===")
    assert result == "<p>Paragraph</p>\n<p>===</p>\n"


def test_setext_vs_thematic_break():
    result = markdown_to_html("---\n\nNext")
    assert result == "<hr />\n<p>Next</p>\n"


def test_atx_headings():
    for level in range(1, 7):
        heading = "#" * level + " Heading " + str(level)
        result = markdown_to_html(heading)
        assert result == f"<h{level}>Heading {level}</h{level}>\n"


def test_emphasis():
    result = markdown_to_html("This is *italic* text.")
    assert result == "<p>This is <em>italic</em> text.</p>\n"


def test_strong_emphasis():
    result = markdown_to_html("This is **bold** text.")
    assert result == "<p>This is <strong>bold</strong> text.</p>\n"


def test_emphasis_underscore():
    result = markdown_to_html("This is _italic_ text.")
    assert result == "<p>This is <em>italic</em> text.</p>\n"


def test_strong_underscore():
    result = markdown_to_html("This is __bold__ text.")
    assert result == "<p>This is <strong>bold</strong> text.</p>\n"


def test_nested_emphasis():
    result = markdown_to_html("**bold *and italic* text**")
    assert result == "<p><strong>bold <em>and italic</em> text</strong></p>\n"


def test_code_span():
    result = markdown_to_html("Use `code` in text.")
    assert result == "<p>Use <code>code</code> in text.</p>\n"


def test_link():
    result = markdown_to_html("[Example](https://example.com)")
    assert result == '<p><a href="https://example.com">Example</a></p>\n'


def test_link_with_title():
    result = markdown_to_html('[Example](https://example.com "Title")')
    assert result == '<p><a href="https://example.com" title="Title">Example</a></p>\n'


def test_image():
    result = markdown_to_html("![Alt](image.png)")
    assert result == '<p><img src="image.png" alt="Alt" /></p>\n'


def test_image_with_title():
    result = markdown_to_html('![Alt](image.png "Title")')
    assert result == '<p><img src="image.png" alt="Alt" title="Title" /></p>\n'


def test_fenced_code_block():
    result = markdown_to_html("```\ncode block\n```")
    assert result == "<pre><code>code block\n</code></pre>\n"


def test_fenced_code_block_with_language():
    result = markdown_to_html("```python\nprint('hello')\n```")
    assert (
        result == "<pre><code class=\"language-python\">print('hello')\n</code></pre>\n"
    )


def test_unordered_list():
    result = markdown_to_html("- Item 1\n- Item 2")
    assert result == "<ul>\n<li>Item 1</li>\n<li>Item 2</li>\n</ul>\n"


def test_ordered_list():
    result = markdown_to_html("1. First\n2. Second")
    assert result == "<ol>\n<li>First</li>\n<li>Second</li>\n</ol>\n"


def test_blockquote():
    result = markdown_to_html("> A quote")
    assert result == "<blockquote>\n<p>A quote</p>\n</blockquote>\n"


def test_blockquote_multiple_lines():
    result = markdown_to_html("> Line 1\n> Line 2")
    assert result == "<blockquote>\n<p>Line 1\nLine 2</p>\n</blockquote>\n"


def test_thematic_break_dashes():
    result = markdown_to_html("---")
    assert result == "<hr />\n"


def test_thematic_break_asterisks():
    result = markdown_to_html("***")
    assert result == "<hr />\n"


def test_thematic_break_underscores():
    result = markdown_to_html("___")
    assert result == "<hr />\n"


def test_table_basic():
    result = markdown_to_html("| a | b |\n|---|---|\n| 1 | 2 |\n")
    assert "<table>" in result
    assert "<th>a</th>" in result
    assert "<th>b</th>" in result
    assert "<td>1</td>" in result
    assert "<td>2</td>" in result


def test_table_no_outer_pipes():
    result = markdown_to_html("a | b\n-|-\n1 | 2\n")
    assert "<table>" in result
    assert "<th>a</th>" in result
    assert "<td>1</td>" in result


def test_table_alignment():
    result = markdown_to_html("| a | b | c |\n|:--|:--:|---:|\n| l | c | r |\n")
    assert 'align="left"' in result
    assert 'align="center"' in result
    assert 'align="right"' in result


def test_table_with_paragraph():
    result = markdown_to_html("| x | y |\n|---|---|\n| 1 | 2 |\n\nParagraph\n")
    assert "<table>" in result
    assert "<p>Paragraph</p>" in result


def test_hard_line_break():
    result = markdown_to_html("Line 1  \nLine 2")
    assert result == "<p>Line 1<br />\nLine 2</p>\n"


def test_escape():
    result = markdown_to_html(r"\*not italic\*")
    assert result == "<p>*not italic*</p>\n"


def test_html_escaping():
    result = markdown_to_html("x < y && y > z")
    assert result == "<p>x &lt; y &amp;&amp; y &gt; z</p>\n"


def test_empty_string():
    result = markdown_to_html("")
    assert result == ""


def test_only_whitespace():
    result = markdown_to_html("   \n\n  ")
    assert result == ""


def test_complex_document():
    md = (
        "# Title\n\n"
        "This is a **paragraph** with *emphasis*.\n\n"
        "- Item 1\n"
        "- Item 2\n\n"
        "> A quote.\n\n"
        "```\n"
        "code\n"
        "```\n"
    )
    result = markdown_to_html(md)
    assert "<h1>Title</h1>" in result
    assert "<strong>paragraph</strong>" in result
    assert "<em>emphasis</em>" in result
    assert "<ul>" in result
    assert "<li>Item 1</li>" in result
    assert "<blockquote>" in result
    assert "<pre><code>" in result


def test_module_all():
    import comverter

    assert comverter.__all__ == ["markdown_to_html", "markdown_file_to_html"]


def test_invalid_type():
    with pytest.raises(TypeError):
        markdown_to_html(123)


def test_markdown_file_to_html():
    import os
    import tempfile

    from comverter import markdown_file_to_html

    with tempfile.NamedTemporaryFile(mode="w", suffix=".md", delete=False) as f:
        f.write("Hello, **world**!")
        inpath = f.name
    outpath = inpath + ".html"
    try:
        markdown_file_to_html(inpath, outpath)
        with open(outpath) as f:
            assert f.read() == "<p>Hello, <strong>world</strong>!</p>\n"
    finally:
        os.unlink(inpath)
        if os.path.exists(outpath):
            os.unlink(outpath)
