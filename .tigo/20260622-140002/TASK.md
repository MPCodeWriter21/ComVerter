# Add a command-line interface (CLI) entry point

- STATUS: OPEN
- PRIORITY: 60
- TAGS: CLI, user-facing

Create a CLI entry point using the `[project.scripts]` table in
`pyproject.toml` that wraps the conversion functionality for
command-line usage.

For example:
```bash
comverter to-html input.md output.html
```

This should:
- Accept input from files or stdin
- Write output to files or stdout
- Handle errors gracefully with meaningful exit codes
- Be tested with integration-style tests

Use `log21` for handling CLI arguments and logging.

This is significant enough to up the minor version.

The default for Markdown to HTML conversion should use themes (Task(20260623-103247)) and there should be a flag like `--raw` which will output unstyled HTML.

Keep in mind that this tool will have functionalities other than converting Markdown to HTML, so the CLI must not be based on only MD to HTML conversion and should probably have a sub-command for it.
