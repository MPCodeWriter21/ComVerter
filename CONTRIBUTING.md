# Contributing

## Getting started

```bash
make setup   # create venv + configure meson
make build   # compile the C extension
make test    # run tests
```

## Code style

- C code follows the existing patterns in `src/comverter/`. Use
  `.clang-format` for automatic formatting.
- Python code is minimal — the C extension does the work.
- Every new feature or bug fix must include or update tests under
  `tests/`. Run `make test-all` to verify nothing is broken.

## Testing

The project uses `pytest`. The test suite covers all 731 examples from
the CommonMark 0.31.2 spec and the GFM spec. To run all tests:

```bash
make test-all
```

To stop after 3 failures during development:

```bash
make test
```

## Pull requests

1. Make sure all tests pass.
2. Add tests for new functionality.
3. Keep commits focused and messages concise.
4. Update README.md and CHANGELOG.md if the change affects the public
   API or notable behaviour.

## License

By contributing, you agree that your contributions will be licensed
under the MIT License (see `pyproject.toml`).
