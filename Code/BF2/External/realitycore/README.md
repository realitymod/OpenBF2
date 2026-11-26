# _realitycore

Project Reality's native `_realitycore` Python module: distance, proximity and polygon helpers that
PR's game scripts import (`realitycore.py`, `realityzones.py`, `realitylaser.py`,
`realityutilities.py`, `gpm_gungame.py`).

`[CHOSEN, not recovered]` Retail Battlefield 2 has no such module. PR ships it as
`bin/amd-64/lib.linux-x86_64-2.7/_realitycore.so` and inside its Windows `dice_py.dll`, both built
for PR's patched interpreter ABI. OpenBF2 compiles it into the engine and registers it as a
built-in module in `PythonHost::initialize`, so it works on any interpreter the engine embeds and
takes precedence over PR's `.so` on `sys.path`.

## Provenance

`_realitycore.c` comes from yossizap/realitycore, `src/_realitycore.c` at commit `35848fc` (the
last commit before the file was removed for an unfinished pybind11 port). It is the source of PR's
shipped `.so`, whose debug info names `BF2-CPython/Modules/_realitycore.c` and matches its symbols
and format strings. Licensed under the GPLv3; see [LICENSE](LICENSE).

## Local changes

Each is marked `OpenBF2:` in the source.

- `M_PI` fallback: `Python.h` includes `math.h` before the file defines `_USE_MATH_DEFINES`.
- `Py_ssize_t` and `Py_RETURN_TRUE`/`Py_RETURN_FALSE` fallbacks for the Python 2.3.4 headers.
- The `fmin`/`fmax` shim is limited to MSVC versions before 2013, which lack them.
- `calcDistanceToPolygon`'s method-table docstring pointed at the function itself.
