# Debug Methodology — The Stub/Bypass Loop

Bringing up a statically recompiled binary is an iterative process. This is the methodology used.

## The Loop

```
1. Run the binary
2. Find the last printf before the hang
3. Open the ppc_recomp.*.cpp function
4. Classify the hang:
   (a) Uninitialized vtable -> PPC_CALL_INDIRECT_FUNC(0) -> crash
   (b) Loop over uninitialized memory -> infinite strlen over zeroed .data
   (c) Cross-module call with no handler -> PPCIndirectCallMissing abort
   (d) Subsystem init requiring real hardware
5. Apply minimal fix (see patterns below)
6. ninja build -> run -> check output
7. Repeat
```

## Fix Patterns

### A — goto bypass (uninitialized vtable)

```cpp
// Before:
ctx.r10.u64 = PPC_LOAD_U32(ctx.r11.u32 + 16);
ctx.ctr.u64 = ctx.r10.u64;
ctx.lr = 0x83214720;
PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32);

// After:
goto loc_83214848;  // bypass uninitialized vtable
```

### B — Function stub (subsystem init)

```cpp
PPC_FUNC_IMPL(__imp__sub_8321A328) {
    PPC_FUNC_PROLOGUE();
    // CAppSystemGroup::InitStage stub -- mark stage 8, return 1
    PPC_STORE_U32(ctx.r3.u32 + 68, 8);
    ctx.r3.u64 = 1;
    return;
}
```

### C — InsertFunction hook (cross-module)

```cpp
g_memory.InsertFunction(0x8322DE38u, [](PPCContext& ctx, uint8_t* base) {
    ctx.r3.u64 = CMDLINE_INSTANCE_GVA;  // CommandLine() -> mock ptr
});
```

## Bypasses Applied

| Function | File | Fix |
|---|---|---|
| sub_8321A328 (CAppSystemGroup::InitStage) | ppc_recomp.0.cpp | Full stub: stage=8, return 1 |
| sub_832105D8 | ppc_recomp.0.cpp | Stub: r3=0, return |
| Vtable 0x83214700 in LauncherMain | ppc_recomp.0.cpp | goto loc_83214848 |
| Vtable 0x8321A484 in sub_8321A448 | ppc_recomp.0.cpp | goto loc_8321A49C |
| Vtable 0x83214160 in sub_83214130 | ppc_recomp.0.cpp | Block removed |
| sub_8322DE38 (CommandLine()) | ppc_recomp.3.cpp | Returns 0x8AE00200 |
| sub_8322DF78 (Plat_GetCommandLine) | l4d_boot.cpp | InsertFunction -> returns 1 |
| sub_8321A328 hook | l4d_boot.cpp | InsertFunction -> stage=8 |
| sub_83213A48 (post-init) | l4d_boot.cpp | InsertFunction -> returns 1 |
