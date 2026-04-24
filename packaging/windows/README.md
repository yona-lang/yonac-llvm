# Windows Installer (WiX v4 Draft)

This directory contains a first-draft MSI installer definition for Yona.

## Layout

- `YonaInstaller.wxs`: WiX v4 package definition.
- `build-msi.ps1`: stages files from a build tree and invokes `wix build`.

## Prerequisites

- WiX Toolset v4 (`wix.exe` on `PATH`)
- A built Yona tree (default: `out/build/x64-release`)

## Build MSI

```powershell
pwsh ./packaging/windows/build-msi.ps1 -BuildDir out/build/x64-release -Version 0.1.0
```

The script creates:

- staging tree: `out/installer/windows/stage`
- msi: `out/installer/windows/yona-<version>-windows-x64.msi`

## Notes

- The draft currently installs into `ProgramFiles64Folder\Yona`.
- `bin` is appended to system `PATH` via MSI environment table.
- The file payload is taken from the staged layout:
  - `bin/` (`yonac.exe`, `yona.exe`)
  - `lib/Std/`
  - `runtime/`
  - `src/runtime/` (for advanced fallback workflows)
  - `include/yona/runtime/`
  - top-level docs/license files
