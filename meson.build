project('comverter', 'c',
  version: '0.1.0',
  license: 'MIT',
)

py = import('python').find_installation(pure: false)

py.install_sources(
  'src/comverter/__init__.py',
  subdir: 'comverter',
)

py.extension_module(
  '_comverter',
  [
    'src/comverter/_comverter.c',
    'src/comverter/md_core.c',
    'src/comverter/md_html.c',
    'src/comverter/md_inline.c',
    'src/comverter/md_blocks.c',
  ],
  install: true,
  subdir: 'comverter',
)
