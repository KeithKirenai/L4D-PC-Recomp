# L4D PC Recomp

> Static binary recompilation of Left 4 Dead (Xbox 360) to native PC

[![Build](https://img.shields.io/badge/build-passing-brightgreen)](#building)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-blue)](#requirements)
[![License](https://img.shields.io/badge/license-GPL--3.0-orange)](LICENSE)
[![Status](https://img.shields.io/badge/status-WIP%20%E2%80%94%20boot%20sequence-yellow)](#current-status)

L4D PC Recomp converts the **Xbox 360 version of Left 4 Dead** into a fully native PC executable through **static binary recompilation** - no emulation, no Xbox 360 hardware required.

The approach follows [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp) (Sonic Unleashed) and uses [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) to lift Xbox 360 PowerPC machine code into C++, compiled for x64 Windows with a thin OS compatibility shim.

---

## How It Works

```
Xbox 360 .xex files
        |
        v
  XenonRecomp        <- Lifts PPC machine code -> C++ source (you run this yourself)
        |
        v
 ppc_recomp.*.cpp    <- 163,246 guest functions across 21 modules
        |
        v
 L4D Runtime Harness <- THIS REPO: boot, dispatch table, kernel stubs
        |
        v
 l4d_boot_test.exe   <- Native Windows x64 PE, zero emulation
```

Every guest PowerPC instruction becomes a C++ statement operating on a `PPCContext` register struct.
Indirect calls go through a flat function-pointer dispatch table indexed by guest virtual address.
Memory accesses are remapped through a 4 GB host VirtualAlloc arena.

---

## Module Map

| Module | Guest Base | Functions | Status |
|---|---|---|---|
| default.xex | 0x82010000 | 367 | OK - kernel thunks |
| tier0_360 | 0x82790000 | 2,121 | OK - loaded |
| vstdlib_360 | 0x82990000 | 1,082 | OK - loaded |
| launcher_360 | 0x83210000 | 1,028 | OK - LauncherMain reached |
| FileSystem_Stdio | 0x82B90000 | 2,547 | pending |
| inputsystem_360 | 0x83090000 | 672 | pending |
| vphysics_360 | 0x82220000 | 5,110 | pending |
| MaterialSystem_360 | 0x843A0000 | 5,142 | pending |
| shaderapidx9_360 | 0x84CB0000 | 8,326 | pending |
| stdshader_dx9_360 | 0x857A0000 | 4,410 | pending |
| StudioRender_360 | 0x85C10000 | 1,812 | pending |
| datacache_360 | 0x82E10000 | 1,841 | pending |
| SoundEmitterSystem_360 | 0x83410000 | 909 | pending |
| SceneFileCache_360 | 0x83610000 | 423 | pending |
| vgui2_360 | 0x83790000 | 1,553 | pending |
| vguimatsurface_360 | 0x848A0000 | 5,318 | pending |
| GameUI_360 | 0x83BE0000 | 10,981 | pending |
| engine_360 | 0x86410000 | 25,665 | pending |
| Client_360 | 0x87A90000 | 36,718 | pending |
| Server_360 | 0x89130000 | 37,932 | pending |
| AppInstaller_360 | 0x8A870000 | 9,289 | pending |
| **Total** | | **163,246** | |

---

## Current Boot Trace

```
_xstart (0x82011508)
  -> [Hook] sub_82011B38  security header bypass -> 1
  -> [Hook] sub_82011320  privilege check bypass -> 0
  -> [Hook] sub_82011A60  CRT initterm bypass
  -> XexLoadImage("tier0_360.dll")      -> 0x82790000
  -> XexLoadImage("vstdlib_360.dll")    -> 0x82990000
  -> XexLoadImage("launcher_360.dll")   -> 0x83210000
  -> XexGetProcedureAddress(ordinal=1)  -> LauncherMain @ 0x83214180
  -> LauncherMain ENTER (r3=1, r4=0, r5=argv)
     -> ICommandLine::CreateCmdLine(" -basedir  -game \left4dead")
     -> sub_832117C0  [basedir setup -- active debugging]
```

---

## Requirements

- Windows 10/11 x64
- LLVM/Clang 17+: `scoop install llvm`
- Ninja: `scoop install ninja`
- Python 3.10+
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) (to generate ppc_recomp sources)
- Left 4 Dead Xbox 360 game files (legally obtained)

> The recompiled C++ sources (~480 MB) are **not included**. You must generate them.
> See [docs/RECOMPILING.md](docs/RECOMPILING.md).

---

## Building

```powershell
# 1. Clone
git clone https://github.com/KeithKirenai/L4D-PC-Recomp.git
cd L4D-PC-Recomp

# 2. Add clang to PATH (adjust for your install)
$env:Path = "C:\your\llvm\bin;" + $env:Path

# 3. Configure and build
cmake -B build -G Ninja -S runtime
ninja -C build

# 4. Run
.\build\l4d_boot_test.exe
```

Full setup guide: [docs/RECOMPILING.md](docs/RECOMPILING.md)

---

## Project Layout

```
L4D-PC-Recomp/
+-- runtime/                     Boot harness, kernel stubs, dispatch table
|   +-- l4d_boot.cpp             Main boot sequence
|   +-- l4d_kernel_stubs.cpp     46 Xbox 360 kernel call stubs
|   +-- l4d_unified_dispatch.h   Dispatch table macros and address constants
|   +-- l4d_unified_dispatch.cpp Function registration (163K entries)
|   +-- l4d_autogen_stubs.cpp    Zero-return stubs for unimplemented imports
|   \-- CMakeLists.txt
+-- recomp_configs/              Per-module XenonRecomp .toml configs (43 files)
+-- docs/
|   +-- ARCHITECTURE.md          Deep-dive: dispatch table, memory model, context
|   +-- RECOMPILING.md           How to generate ppc_recomp.*.cpp
|   +-- PE_DUMPS.md              PE section dump format
|   +-- KERNEL_STUBS.md          Xbox 360 kernel call reference
|   +-- DEBUG_METHODOLOGY.md     Iterative stub/bypass debug loop
|   \-- PROGRESS.md              Boot progress log
+-- tools/                       (future helper scripts)
+-- .gitignore
+-- LICENSE
\-- README.md
```

---

## Architecture Summary

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for full detail.

**Dispatch Table**: 288 MB flat `PPCFunc**` array. Index = `(gva - 0x82000000) >> 2`.
Direct calls use `PPC_LOOKUP_FUNC`; indirect calls use `PPC_CALL_INDIRECT_FUNC` with bounds check.

**Guest Memory**: 4 GB `VirtualAlloc` arena. All guest addresses are offsets into this buffer.

**PPCContext**: Struct with 32 GPRs as `PPCRegister` unions (u8/u16/u32/u64/s*/f* views), CR0-CR7, XER, CTR, LR, FPSCR. Host byte order throughout; bswap only at load/store boundaries.

**PE Preloading**: Each .xex was extracted as a Windows PE. At boot, sections are `memcpy`d into guest RAM at `imageBase + virtualAddress`, initializing .data, .rdata, vtables, and globals.

---

## Contributing

Priority contribution areas:

1. Stub remaining `LauncherMain` boot path (see [docs/DEBUG_METHODOLOGY.md](docs/DEBUG_METHODOLOGY.md))
2. Implement `FileSystem_Stdio` with real PC path mapping
3. Rendering stub (SDL2/DXGI window for `shaderapidx9`)
4. Linux support (`mmap` path already conditionally compiled)
5. Audio stub (`SoundEmitterSystem_360`)

---

## Related Projects

| Project | Description |
|---|---|
| [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) | PPC -> C++ static recompiler |
| [XenosRecomp](https://github.com/hedge-dev/XenosRecomp) | Xbox 360 GPU shader recompiler |
| [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp) | Sonic Unleashed -- flagship reference |
| [rexglue-sdk](https://github.com/rexglue/rexglue-sdk) | Alternative Xbox 360 recompiler toolkit |

---

## Legal

No game assets, no proprietary code, no firmware included. Supply your own legally obtained L4D Xbox 360 copy.
Not affiliated with Valve or Microsoft.

## License

GPL-3.0 -- see [LICENSE](LICENSE).
