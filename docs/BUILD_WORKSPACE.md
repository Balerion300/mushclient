# Building the workspace preview

Use VS2022 C++ Build Tools, Windows SDK and x86 MFC. The existing project links
its static runtime and bundled libraries; the new editor uses the independently
built official Scintilla source under `third_party/scintilla`.

Run `tools/build_workspace.ps1 -Configuration Release` from PowerShell. Use
`-Configuration Debug` for the diagnostic client. The script locates Visual
Studio and the installed MFC libraries, builds the solution, Scintilla and native
stdio companion, then copies the editor DLL beside the selected executable.
It creates temporary build properties, not registry preferences or user worlds.

For focused tests, install Python 3.11, Pillow and psutil in a development virtual
environment. Run `python tests/run_native_tests.py`,
`python tests/run_agent_tests.py`, `python -m unittest discover -s agent_companion`,
and `tests/workspace/run_native_probe.ps1`. Editor fixtures are documented in
`docs/PENN_IDE.md`. Run `python tests/workspace/persistence_probe.py` for the SQLite
contract. The native frame and editor are Windows-only.

The optional provider runner is independent Python source in
`agent_companion/runner.py`. A standalone Windows x64 runner can be produced with
Python 3.11 and PyInstaller 6.22.3:

```
python -m PyInstaller --onefile --name MUSHclientAgentRunner --distpath WinRel --workpath .runner-build --specpath .runner-build agent_companion/runner.py
```

Then run `tests/workspace/integration_runner_package.py` against that executable.
Include `PYTHON-LICENSE.txt` and `PYINSTALLER-COPYING.txt` when distributing it.
The main client and stdio companion do not require an installed Python runtime.
No provider runs until its explicit argument list is configured.

`tests/package_workspace.py` combines these binaries with a fresh extracted
official MUSHclient distribution, notices, help and documentation, and creates a
SHA256 manifest. It refuses an existing destination or ZIP; it never copies the
installed client's preferences, saved worlds, grants or credentials.
