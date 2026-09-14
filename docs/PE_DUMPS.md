# PE Section Dumps

At boot the harness preloads each Xbox 360 module's data section (`imageBase + VirtualAddress`)
into guest RAM. This initialises `.data`, `.rdata`, vtables, and globals **before** any guest
code runs, so recompiled functions see the game's real static state from the first instruction.

## Location

`L4D_PreloadAllPEData(g_memory.base, peDumpsDir)` scans `peDumpsDir` for every file
with the `.pe` extension:

- Default: `<project>/analysis/pe_dumps/`
- Overridable at runtime: `l4d_boot_test.exe --pe-dumps <dir>`

Files are named `bin_<ModuleName>.dll.pe` (e.g. `bin_tier0_360.dll.pe`). Any `.pe` file in
the directory is loaded; naming only matters for your own bookkeeping.

## Source Format

The dumps are **plain Windows PE files** (an extracted inner PE from the Xbox 360 XEX,
converted to the host-valid PE container). Only the following fields are read:

| Field | Where it maps |
|---|---|
| DOS/NT headers | Skipped (validity check only) |
| `IMAGE_SECTION_HEADER[i].VirtualAddress` | Guest offset = `ImageBase + VirtualAddress` |
| `IMAGE_SECTION_HEADER[i].SizeOfRawData` | Number of bytes copied |
| Raw bytes | `memcpy(guestBase + ImageBase + VirtualAddress, fileBytes + PointerToRawData, SizeOfRawData)` |

ImageBase is taken from the PE's optional header and must match the module's guest base
edit from the co recompilation (e.g. launcher_360 ImageBase = `0x83210000`).

## Generating the Dumps

Extract the inner PE from each Xbox 360 XEX. `xex2pe` (or ReLaunchXe / `exiso` combined
with a `xextool` decompress) will yield a Windows-readable PE:

```bash
xex2pe tier0_360.dll analysis/pe_dumps/bin_tier0_360.dll.pe
```

Repeat for all 21 modules:

```
default.xex           -> bin_default.xex.pe
tier0_360.dll         -> bin_tier0_360.dll.pe
vstdlib_360.dll       -> bin_vstdlib_360.dll.pe
launcher_360.dll      -> bin_launcher_360.dll.pe
FileSystem_Stdio_360.dll  -> bin_FileSystem_Stdio_360.dll.pe
inputsystem_360.dll   -> bin_inputsystem_360.dll.pe
vphysics_360.dll      -> bin_vphysics_360.dll.pe
MaterialSystem_360.dll    -> bin_MaterialSystem_360.dll.pe
shaderapidx9_360.dll  -> bin_shaderapidx9_360.dll.pe
stdshader_dx9_360.dll -> bin_stdshader_dx9_360.dll.pe
StudioRender_360.dll  -> bin_StudioRender_360.dll.pe
datacache_360.dll     -> bin_datacache_360.dll.pe
SoundEmitterSystem_360.dll -> bin_SoundEmitterSystem_360.dll.pe
SceneFileCache_360.dll    -> bin_SceneFileCache_360.dll.pe
vgui2_360.dll         -> bin_vgui2_360.dll.pe
vguimatsurface_360.dll    -> bin_vguimatsurface_360.dll.pe
GameUI_360.dll        -> bin_GameUI_360.dll.pe
engine_360.dll        -> bin_engine_360.dll.pe
client_360.dll        -> bin_client_360.dll.pe
server_360.dll        -> bin_server_360.dll.pe
AppInstaller_360.dll  -> bin_AppInstaller_360.dll.pe
```

## Gotchas

- **ImageBase must match the recompile base.** If XenonRecomp emitted functions at a
  different guest base than the PE header, vtables and globals land at the wrong
  addresses and indirect calls misbehave.
- Dumps are large (~50 MB total). They are **git-ignored** (`analysis/pe_dumps/`) —
  pass your own path at runtime or copy them into an ignored local dir.
- If no `.pe` files are found the boot continues with a warning and all data sections
  stay zeroed, which makes init loops hang (see [DEBUG_METHODOLOGY.md](DEBUG_METHODOLOGY.md)).