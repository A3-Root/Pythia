# Using Pythia

Pythia lets you write Arma 3 extension logic in **Python 3** instead of C++. Your
SQF calls a single entry point, and Pythia routes the call to a Python function
you ship inside a mod folder. Values convert automatically in both directions.

> Pythia uses the **system Python** — it does not bundle an interpreter. Every
> machine that runs a Pythia-powered mod must have the matching Python version
> installed (see [Requirements](#requirements)).

---

## Contents

- [Requirements](#requirements)
- [Installing Pythia](#installing-pythia)
- [Quick start](#quick-start)
- [Anatomy of a Pythia mod](#anatomy-of-a-pythia-mod)
- [The SQF API](#the-sqf-api)
- [Writing Python bridges](#writing-python-bridges)
- [Type conversion](#type-conversion)
- [Return values, errors and large responses](#return-values-errors-and-large-responses)
- [Long-running work: threads](#long-running-work-threads)
- [Module discovery and `PYTHIA_PATH`](#module-discovery-and-pythia_path)
- [Built-in functions](#built-in-functions)
- [Python dependencies (pip)](#python-dependencies-pip)
- [Compiled modules (Cython / numpy)](#compiled-modules-cython--numpy)
- [Development workflow](#development-workflow)
- [Troubleshooting](#troubleshooting)
- [Building Pythia from source](#building-pythia-from-source)
- [Bundled examples](#bundled-examples)

---

## Requirements

- **Arma 3** (64-bit `Arma3_x64.exe` is recommended; Linux 32-bit is not supported).
- **Python**, matching the version Pythia was built against — `major.minor` must
  match exactly (e.g. `3.12`). The patch level does not matter.
  - **Windows:** install the 64-bit Python from [python.org](https://www.python.org/)
    (include the development headers). Pythia finds it via `PYTHIA_PYTHON_HOME`,
    `PYTHONHOME`, the registry, or `PATH`.
  - **Linux:** install your distro's `python3.<minor>` package. The system
    `libpython` is loaded by the dynamic loader.
- **BattlEye must be disabled** — it blocks the extension from loading.

Pythia logs the interpreter it resolved to `pythia.log` / `PythiaSetPythonPath.log`
in the Arma 3 directory.

---

## Installing Pythia

1. Get `@Pythia` — subscribe on the Steam Workshop, download a release, or build
   it yourself (see [Building Pythia from source](#building-pythia-from-source)).
2. Copy the `@Pythia` folder into your Arma 3 directory.
3. Install the matching Python version (above).
4. In the Arma 3 Launcher, enable `@Pythia` and **disable BattlEye**.

Verify it loads from the debug console (in the editor) with:

```sqf
hint str (["pythia.ping", [1, 2, 3]] call py3_fnc_callExtension);
// -> [1,2,3]
```

---

## Quick start

A minimal Python extension is a folder with a `$PYTHIA$` marker and a Python
package. Put it in any enabled mod that Pythia can see (or point `PYTHIA_PATH`
at it — see [Module discovery](#module-discovery-and-pythia_path)).

```
@MyMod/
    addons/                 <- your normal PBOs (so Arma loads the mod)
    mymod/
        $PYTHIA$            <- text file, single line: mymod
        __init__.py         <- your Python code
```

`$PYTHIA$` contains exactly the importable package name:

```
mymod
```

`mymod/__init__.py`:

```python
def greet(name):
    return f"Hello, {name}!"
```

Call it from SQF:

```sqf
private _msg = ["mymod.greet", ["world"]] call py3_fnc_callExtension;
hint _msg;   // Hello, world!
```

---

## Anatomy of a Pythia mod

- **`$PYTHIA$` marker** — a plain text file whose only content is the Python
  **package name** Pythia should import (letters, digits, underscores). The
  directory that contains `$PYTHIA$` is added so that name becomes importable.
- The package directory name usually matches the marker, but it does not have to
  (the marker is authoritative).
- You can ship Python code inside an existing mod (next to its `addons/`), or as
  a separate Python-only mod. Either way the mod must be loaded by Arma (or the
  directory listed in `PYTHIA_PATH`) for Pythia to discover it.

A package (`__init__.py`) exposes functions as `package.function`. Sub-modules
and sub-packages nest with dots: `package.submodule.function`.

---

## The SQF API

Always call through the helper function — it handles serialization, the response
protocol, large responses, and error reporting:

```sqf
[<function path>, <arguments array>] call py3_fnc_callExtension
```

- **`<function path>`** — a string: `"package.function"` or
  `"package.sub.module.function"`.
- **`<arguments array>`** — an array of arguments passed positionally to the
  Python function. Use `[]` for none.

Examples:

```sqf
// Function in a package's __init__.py
["basic.hello", []] call py3_fnc_callExtension;

// Function inside a sub-module
["basic.module.ping_from_module", ["str", 1, 2.3, true]] call py3_fnc_callExtension;

// Multiple arguments
["mymod.add", [2, 40]] call py3_fnc_callExtension;   // -> 42
```

The call is **synchronous**: Arma waits for the Python function to return, then
gives you the converted return value. Keep handlers fast (see
[threads](#long-running-work-threads) for slow work).

> Under the hood this wraps `"Pythia" callExtension (str [...])`. You normally
> never call `callExtension` directly — `py3_fnc_callExtension` is the supported
> interface.

---

## Writing Python bridges

A "bridge" is just a normal Python function. Anything importable under your
package can be called.

```python
# mymod/__init__.py

def add(a, b):
    return a + b

def stats(numbers):
    # numbers arrives as a Python list
    return [min(numbers), max(numbers), sum(numbers) / len(numbers)]
```

```python
# mymod/world.py  -> called as "mymod.world.spawn"

def spawn(kind, position):
    x, y, z = position
    return ["spawned", kind, [x, y, z]]
```

Rules of thumb:

- Arguments are passed **positionally**, in array order.
- A function called with no second array (`["mymod.func"]`) receives no
  arguments — equivalent to `["mymod.func", []]`.
- Return one value (any [convertible type](#type-conversion)). Returning `None`
  yields `nil` in SQF.
- Imports are lazy and cached: the first call imports your module; later calls
  reuse it.

---

## Type conversion

| SQF                 | Python                | Notes                                            |
|---------------------|-----------------------|--------------------------------------------------|
| Number              | `float`               | SQF has a single numeric type; integers arrive as `float` (e.g. `3.0`). |
| `true` / `false`    | `bool`                |                                                  |
| String              | `str`                 | UTF-8.                                            |
| Array               | `list`                | Nest freely.                                      |
| `nil`               | `None`                |                                                  |

Returning from Python:

| Python                  | SQF                  | Notes                                          |
|-------------------------|----------------------|------------------------------------------------|
| `int` / `float`         | Number               |                                                |
| `bool`                  | `true` / `false`     |                                                |
| `str`                   | String               |                                                |
| `list` / `tuple`        | Array                |                                                |
| `None`                  | `nil`                |                                                |

- If you need integers on the SQF side, remember they come **in** as floats;
  convert with `int(...)` in Python when indexing or doing integer math.
- **Dictionaries are not converted automatically.** Return structured data as
  nested arrays, e.g. `[["id", 7], ["name", "Bob"]]`, and read it in SQF.
- **Coroutines / `async` return values are rejected.** Use a background thread
  instead (next section).

---

## Return values, errors and large responses

`py3_fnc_callExtension` returns the converted Python return value directly. On
the wire Pythia tags responses (`"r"` result, `"e"` error, `"m"` multipart); the
helper hides all of that:

- **Result** — you get the value.
- **Error** — if the Python function raises, the helper shows the Python
  traceback via `PY3_fnc_showMessage` and the call returns `[]`. Check `pythia.log`
  for the full traceback.
- **Large responses** — Arma caps a single `callExtension` return at ~10k
  characters. Pythia automatically splits bigger payloads into multiple parts and
  the helper stitches them back together. You don't do anything special; just be
  aware very large returns cost extra round-trips.

---

## Long-running work: threads

A synchronous call blocks the whole game until Python returns. For anything slow,
start a background thread and poll it from SQF. (Pythia makes new threads daemon
by default so Arma can still exit.)

A common pattern — start work, get a ticket, poll, fetch the result:

```python
# slowmod/__init__.py
import threading

_TASKS = {}
_NEXT_ID = 0

def _heavy(n):
    # pretend this takes a while
    return sum(i * i for i in range(n))

def start(n):
    global _NEXT_ID
    _NEXT_ID += 1
    tid = _NEXT_ID
    t = threading.Thread(target=lambda: _TASKS.__setitem__(tid, _heavy(n)))
    t.start()
    return tid                      # hand the ticket back to SQF

def done(tid):
    return tid in _TASKS

def result(tid):
    return _TASKS.pop(tid)
```

```sqf
private _tid = ["slowmod.start", [10000000]] call py3_fnc_callExtension;

// poll without freezing the game (e.g. in a loop / per-frame handler)
waitUntil { ["slowmod.done", [_tid]] call py3_fnc_callExtension };

private _value = ["slowmod.result", [_tid]] call py3_fnc_callExtension;
```

See `examples/@PythiaThread` for a fuller, exception-safe version.

---

## Module discovery and `PYTHIA_PATH`

Pythia finds your Python packages by:

1. **Loaded Arma mods** — it scans the folders of mods Arma has loaded
   (recursively, bounded depth) for `$PYTHIA$` markers.
2. **`PYTHIA_PATH` environment variable** — a list of directories that each
   contain a `$PYTHIA$` marker. Separator is `;` on Windows, `:` on Linux. These
   take precedence over discovered modules and are handy during development:

   ```bat
   set PYTHIA_PATH=D:\dev\mymod
   ```

   ```bash
   export PYTHIA_PATH=/home/me/dev/mymod
   ```

3. **Runtime rescan** — after adding or moving modules you can re-run discovery
   without restarting Arma:

   ```sqf
   ["pythia.rescan", []] call py3_fnc_callExtension;   // -> number of modules found
   ```

---

## Built-in functions

Pythia ships internal helpers under the `pythia.` namespace:

| Call                                             | Purpose                                             |
|--------------------------------------------------|-----------------------------------------------------|
| `["pythia.ping", [a, b, ...]]`                   | Echoes its arguments back. Connectivity check.      |
| `["pythia.test", []]`                            | Returns `"OK"`.                                      |
| `["pythia.version", []]`                         | Returns the Pythia version string.                  |
| `["pythia.rescan", []]`                          | Re-discovers modules; returns the module count.     |
| `["pythia.enable_reloader", [true]]`             | Enables the dev code reloader (see below).          |

---

## Python dependencies (pip)

If your module needs third-party packages, list them in a `requirements.txt` and
install them **into the same system Python** Pythia uses:

```bash
python -m pip install -r requirements.txt
```

`@Pythia` also ships convenience installers (`install_requirements64.bat`,
`install_requirements64.sh`) that target the correct system Python — drag a
`requirements.txt` onto them. See `examples/@PythiaUsePip`.

---

## Compiled modules (Cython / numpy)

Cython and other C-extension modules (numpy, pydantic-core, …) work. Pythia loads
the system `libpython` with global symbol visibility so native extensions can
resolve Python's C symbols. Make sure any wheels you install match your Python
`major.minor` and architecture (64-bit). See `examples/@PythiaNumpy`.

---

## Development workflow

- **Iterate without restarting Arma** — point `PYTHIA_PATH` at your working copy
  and enable the reloader:

  ```sqf
  ["pythia.enable_reloader", [true]] call py3_fnc_callExtension;
  ```

  Pythia then reloads changed `.py` files on subsequent calls. Modules may define
  `__pre_reload__()` / `__post_reload__(state)` to carry state across a reload.
  The reloader adds per-call overhead and only reloads pure Python (not compiled
  `.pyd`/`.so`) — **never ship it enabled**.

- You can run and test your package as ordinary Python outside Arma (most
  examples have an `if __name__ == "__main__":` block you can execute directly).

---

## Troubleshooting

- **`Extension output is empty` / nothing happens** — BattlEye is almost always
  the cause. Disable it. It can also mean the extension failed to load; check
  `pythia.log` and `PythiaSetPythonPath.log`.
- **`python3X.dll` not found / extension won't load (Windows)** — the matching
  Python isn't installed or isn't discoverable. Install Python `major.minor`
  (64-bit), or set `PYTHIA_PYTHON_HOME` to its directory. The resolved path is
  logged in `PythiaSetPythonPath.log`.
- **Wrong Python version** — the installed `major.minor` must match what Pythia
  was built against (printed in `pythia.log`). A mismatched minor version will
  fail to load.
- **`Pythia module "..." not recognized`** — Pythia can't find your package.
  Confirm the mod is enabled (or in `PYTHIA_PATH`), the `$PYTHIA$` marker exists
  and names the package, and try `pythia.rescan`.
- **Linux 32-bit** — unsupported. Use the 64-bit server/client executable.
- **Import errors from your module** — read the traceback in `pythia.log`; it is
  trimmed to your code's frames.

---

## Building Pythia from source

You only need this if you are changing Pythia's C++/tooling — writing Python
extensions just needs the published `@Pythia`.

- **Windows:** run `build.bat` (x64 binaries + addon PBOs). Add `x86` to also
  build 32-bit, or `nopbo` to skip PBOs. Requires the target Python with dev
  headers, CMake, Visual Studio (Desktop C++), and Mikero's DePboTools for the
  PBOs.
- **Linux:** run `./build.sh` (x64 `.so`). Requires `python3.<minor>-dev`,
  `build-essential`, `cmake`, `ninja-build`.

Both produce/extend the `@Pythia` folder. The PBOs are platform-independent, so a
full cross-platform release is the Windows binaries + Linux `.so` + one shared set
of PBOs. See `README.md` and `DEVELOPMENT.md` for details.

---

## Bundled examples

In `examples/`:

| Example            | Shows                                             |
|--------------------|---------------------------------------------------|
| `@PythiaBasic`     | Packages, sub-modules, arguments, return values.  |
| `@PythiaClasses`   | Structuring code with classes.                    |
| `@PythiaLogging`   | Logging from Python.                              |
| `@PythiaNumpy`     | Using numpy / compiled dependencies.              |
| `@PythiaThread`    | Background threads and the poll pattern.          |
| `@PythiaUsePip`    | Shipping and installing `requirements.txt`.       |
| `pythia_pbo_sample`| A minimal addon PBO that wires up a Python mod.   |

Enable an example as a local mod in the Launcher (alongside `@Pythia`, with
BattlEye disabled) to try it.
