# Windows Release Builds

This mini-guide summarizes the Windows release build variants produced by
`app\build_windows.ps1`.

Run these commands from the repository root in an x64 Visual Studio Developer
PowerShell. Replace `C:\dev\vcpkg` with your local vcpkg root, unless
`VCPKG_ROOT` is already set.

## Build Commands

Build the Microsoft Store/MSIX-targeted release payload:

```powershell
.\app\build_windows.ps1 -Configuration Release -VcpkgRoot C:\dev\vcpkg -Variants MsStore
```

Build the standalone release plus the installer/updater release payload:

```powershell
.\app\build_windows.ps1 -Configuration Release -VcpkgRoot C:\dev\vcpkg -Variants Standard,Standalone
```

If you omit `-Variants`, the script builds all three variants by default:

```powershell
.\app\build_windows.ps1 -Configuration Release -VcpkgRoot C:\dev\vcpkg
```

The default variant list is:

```text
Standard, MsStore, Standalone
```

## Variant Differences

### Standard

- Output directory: `app\build-windows\Release`
- Package kind: `STANDARD`
- Update mode: `AUTO_INSTALL`
- Intended for the normal installer/updater release channel.
- `aifilesorter.exe` is the Windows bootstrapper and launches
  `aifilesorter-bin.exe`.
- Use this build as the payload for the installer release.

### MsStore

- Output directory: `app\build-windows-store\Release`
- Package kind: `MSIX`
- Update mode: `DISABLED`
- Intended for Microsoft Store/MSIX packaging.
- Store builds do not perform app-managed update checks.
- `aifilesorter.exe` is the main application executable directly, not the
  bootstrapper.

After compiling this variant, package the MSIX separately from
`app\scripts`:

```powershell
Push-Location .\app\scripts
.\package_msix.ps1
Pop-Location
```

### Standalone

- Output directory: `app\build-windows-standalone\Release`
- Package kind: `STANDALONE`
- Update mode: `NOTIFY_ONLY`
- Intended for portable/manual distribution.
- The app can notify users about updates, but it does not auto-install them.
- `aifilesorter.exe` is the Windows bootstrapper and launches
  `aifilesorter-bin.exe`.

## Clearing the Compile Cache

To clear the selected build directories and the shared vcpkg installed tree,
pass `-Clean` to the build script.

Clean and rebuild all three default variants:

```powershell
.\app\build_windows.ps1 -Configuration Release -VcpkgRoot C:\dev\vcpkg -Clean
```

Clean and rebuild only the Store build cache:

```powershell
.\app\build_windows.ps1 -Configuration Release -VcpkgRoot C:\dev\vcpkg -Variants MsStore -Clean
```

Clean and rebuild only the installer/updater and standalone build caches:

```powershell
.\app\build_windows.ps1 -Configuration Release -VcpkgRoot C:\dev\vcpkg -Variants Standard,Standalone -Clean
```

`-Clean` removes the selected variant build directories, such as
`app\build-windows-store`, and also removes the shared dependency install tree
`app\build-windows-vcpkg_installed` when present. This forces CMake configure,
build outputs, and vcpkg-managed dependencies to be recreated.
