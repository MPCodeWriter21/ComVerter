# Add documentation and verify package

- STATUS: CLOSED
- PRIORITY: 70
- TAGS: documentation, verification

Update the README.md with build and install instructions for the package.
Verify that:
1. `uv build` produces a working wheel
2. The installed package can be imported
3. Calling `comverter.hello_world()` prints "Hello World!" to stdout
