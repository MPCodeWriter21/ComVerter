# Add "autodir" custom extension

- STATUS: OPEN
- PRIORITY: 50
- TAGS: bidi, autodir, extension

Add a new custom extension `"autodir"` that adds `dir="auto"` to
block-level HTML elements, matching GitHub's approach for bidirectional
text support.

## Behaviour

1. Append ` dir="auto"` to all block-level tags in the rendered HTML:
   `<p>`, `<h1>`–`<h6>`, `<ul>`, `<ol>`, `<li>`, `<blockquote>`,
   `<pre>`, `<table>`, `<thead>`, `<tbody>`, `<tr>`, `<td>`, `<th>`.
2. The browser's Unicode Bidi Algorithm reads the first strong
   character in the element and sets the direction automatically.

## Examples

Input:
```
Hello

سلام

World
```

With `extensions=["autodir"]`, expected output:
```html
<p dir="auto">Hello</p>
<p dir="auto">سلام</p>
<p dir="auto">World</p>
```

Input:
```
> سلام
> Hello
```

Expected output:
```html
<blockquote dir="auto">
<p dir="auto">سلام</p>
<p dir="auto">Hello</p>
</blockquote>
```

## Scope

- Works with all block-level elements.
- Does NOT set `dir` on inline elements.
- Only appended to tags emitted by ComVerter, not to raw HTML
  passed through in the input.

## Implementation notes

- The extension identifier string is `"autodir"`.
- The simplest implementation is to add ` dir="auto"` to every
  opening block tag right in the output functions of `md_blocks.c`
  (and any other file that emits block-level HTML tags).
- No Unicode detection is needed — the browser handles it.

This is a significant enough to upgrade to minor version of the package.
