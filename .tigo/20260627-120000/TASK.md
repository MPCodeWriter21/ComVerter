# Fix the code to pass 24 failing CommonMark Spec Tests

- STATUS: CLOSED
- PRIORITY: 80
- TAGS: bug, lists, spec-compliance

(done — all 731/731 pass)

24 of 707 CommonMark spec examples fail. All failures cluster in list-item interactions: indented code after blank lines, blockquote nesting, fenced code inside items, HTML comments between lists, sublist indentation, reference-definition consumption, and paragraph-interruption rules.

Source file: `src/comverter/md_blocks.c` (2046 lines)
