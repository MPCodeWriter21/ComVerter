project('comverter', 'c',
  version: '0.1.0',
  license: 'MIT',
)

py = import('python').find_installation(pure: false)

# Android requires explicit -lpython3.x for extension modules
# because (unlike Linux) it does not resolve symbols from the
# hosting process. See PEP 738 and meson-python#824.
if host_machine.system() == 'android'
  py_dep = py.dependency(embed: true)
else
  py_dep = py.dependency()
endif

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
  dependencies: py_dep,
  install: true,
  subdir: 'comverter',
)
