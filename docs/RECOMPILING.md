# Generating Recompiled Sources

The ~480 MB `ppc_recomp.*.cpp` files are **not included** in this repo. Generate them yourself.

## Prerequisites

1. Left 4 Dead Xbox 360 game files (legally obtained)
2. [XenonRecomp](https://github.com/hedge-dev/XenonRecomp)
3. `xex2pe` or equivalent XEX extraction tool
4. Python 3.10+

## Step 1 — Extract XEX Files

Use `xex2pe` or `exiso` to extract from the game disc:

```
default.xex  tier0_360.dll  vstdlib_360.dll  launcher_360.dll
filesystem_stdio_360.dll  inputsystem_360.dll  vphysics_360.dll
materialsystem_360.dll  shaderapidx9_360.dll  stdshader_dx9_360.dll
studiorender_360.dll  datacache_360.dll  soundemittersystem_360.dll
scenefilecache_360.dll  vgui2_360.dll  vguimatsurface_360.dll
gameui_360.dll  engine_360.dll  client_360.dll  server_360.dll
appinstaller_360.dll
```

## Step 2 — Run XenonRecomp

For each module, run XenonRecomp with the config from `recomp_configs/`:

```bash
XenonRecomp recomp_configs/launcher_360.toml
```

Output goes to `analysis/recompiled_code/<module>/`. Repeat for all 21 modules (~30-60 min total).

## Step 3 — Generate PE Dumps

Extract the inner PE from each XEX and save as `.pe`:

```bash
xex2pe tier0_360.dll analysis/pe_dumps/tier0_360.pe
```

Repeat for all 21 modules.

## Step 4 — Build

See [README](../README.md#building).
