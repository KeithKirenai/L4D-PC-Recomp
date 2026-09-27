# Boot Progress Log

## Milestone 1 - Dispatch Table (DONE)

- 163,246 functions registered across 21 modules
- 288 MB dispatch table allocated at runtime
- All 21 PE images staged into guest RAM (26 sections)

## Milestone 2 - Kernel Stubs (DONE)

- 46 kernel stubs active at default.xex thunk GVAs
- XexLoadImage resolves all 21 modules by name suffix
- XexGetProcedureAddress returns LauncherMain @ 0x83214180 for ordinal 1

## Milestone 3 - _xstart to LauncherMain (DONE)

Verified boot trace:
```
_xstart (0x82011508)
  -> security bypass (sub_82011B38)   -> 1
  -> privilege check (sub_82011320)   -> 0  
  -> CRT initterm bypass (sub_82011A60)
  -> XexLoadImage(tier0_360.dll)      -> 0x82780000
  -> XexLoadImage(vstdlib_360.dll)    -> 0x82980000
  -> XexLoadImage(launcher_360.dll)   -> 0x83200000
  -> XexGetProcedureAddress(ordinal=1)-> LauncherMain @ 0x83214180
  -> LauncherMain ENTER
     -> ICommandLine::CreateCmdLine(" -game left4dead -novid")
     -> sub_832117C0 (basedir path setup) -> PASSED (hang resolved)
     -> Parameter parsing & subsystem group loop -> PASSED
     -> LauncherMain clean exit (r3 = 0)
```

## Milestone 4 - FileSystem_Stdio_360 & Dynamic Export Resolution (DONE)

- Real Xbox 360 PE ImageBase addresses aligned across `g_l4dModuleTable`.
- Full module export table parsed from all 20 Xbox 360 PE dumps and compiled into `L4D_GetModuleExport`.
- `XexGetProcedureAddress` wired to dynamically resolve exports for any loaded module and ordinal.
- `FileSystem_Stdio_360` entry point (`sub_82BCF5B8`) attached and executed during boot.
- CRT static initializers execute successfully, registering `InterfaceReg` factories:
  - `VFileSystem017`
  - `VBaseFileSystem011`
  - `QueuedLoaderVersion001`
  - `XboxInstallerVersion001`
- `CreateInterface("VFileSystem017")` returns `0x82BF3A90` with vtable `0x82B818DC`.
- `*g_pFileSystemModule` (`0x827CB460`) successfully linked to the active `IFileSystem` interface.

## Next Steps

1. Progress `CAppSystemGroup` subsystem instantiation (`MaterialSystem_360`, `inputsystem_360`, `vphysics_360`, `engine_360`).
2. Implement host window and render swapchain stub (SDL2 / host DXGI / Vulkan) for `shaderapidx9_360`.
3. Transition control from `LauncherMain` into `engine_360`'s main host execution loop (`CEngineAPI::Run`).

