# Implement hello_world C extension function

- STATUS: CLOSED
- PRIORITY: 80
- TAGS: implementation, C

Implement the C extension module `_comverter` with a `hello_world()`
function that prints "Hello World!" to stdout using printf.

The module should:
- Define a proper Python C extension module
- Implement the hello_world function
- Handle module initialization correctly
- Export the function in the module's method table

Also create the Python package `__init__.py` that imports and exposes
the C extension's functions.
