# Architecture

L4D PC Recomp translates Left 4 Dead Xbox 360 PowerPC binaries into native x64 C++ via static recompilation.

## Dispatch Table

288 MB flat `PPCFunc**` array allocated at runtime.

- **Base**: `L4D_GLOBAL_BASE = 0x82000000`
- **Span**: `L4D_GLOBAL_SPAN = 0x09000000` (covers all 21 module images)
- **Index formula**: `(gva - 0x82000000) >> 2`
- **Direct calls**: `PPC_LOOKUP_FUNC(base, target_gva)` — static, resolved at recompile time
- **Indirect calls**: `PPC_CALL_INDIRECT_FUNC(ctr)` — bounds-checked; calls `PPCIndirectCallMissing` if slot is null

## Guest Memory Arena

4 GB VirtualAlloc (Windows) or mmap (Linux) at startup.

All guest virtual addresses are byte offsets into `g_memory.base`:

```cpp
void* host = g_memory.base + guest_va;
uint32_t gva = (uint8_t*)host - g_memory.base;
```

## PPCContext

```cpp
struct PPCContext {
    PPCRegister r[32];     // GPRs, union: u8/u16/u32/u64/s*/f*/d*
    PPCCRRegister cr[8];   // CR0-CR7
    PPCXERRegister xer;
    PPCRegister ctr, lr;
    PPCFPSCRRegister fpscr;
};
```

Registers hold values in **host (little-endian) byte order**. Byte-swapping occurs only at load/store boundaries:

```cpp
// Read big-endian Xbox 360 memory -> little-endian host register
ctx.r3.u32 = __builtin_bswap32(*(uint32_t*)(base + addr));
// Write little-endian register -> big-endian guest memory
*(uint32_t*)(base + addr) = __builtin_bswap32(ctx.r3.u32);
```

## PE Section Preloading

Each .xex was extracted as a Windows PE. At boot, the harness:
1. Reads each `.pe` file from `analysis/pe_dumps/`
2. Parses the PE section table
3. `memcpy`s each section into guest RAM at `imageBase + section.VirtualAddress`

This initializes `.data`, `.rdata`, vtables, and globals before any guest code runs.

## Mock Subsystems (`[0x8AE00000, 0x8AF00000)`)

### ICommandLine
- Instance GVA: `0x8AE00200`
- Vtable GVA: `0x8AE00000` (64 entries)
- Returned by `CommandLine()` hook
- Handlers: CreateCmdLine, CheckParm, FindParm registered via `InsertFunction`

### IMemAlloc
- Instance GVA: `0x8AE00410`
- Vtable GVA: `0x8AE00500`
- Global ptr `g_pMemAlloc` at `0x83200430`
- Alloc: bump allocator from `0x8AF00000`; Free: no-op

## Guest Thread Context

Thread block at `THREAD_BLOCK_GVA = 0x9FF00000`:

```
0x9FF00000  PCR   (0xAB0 bytes)
0x9FF00AB0  TLS   (0x100 bytes)
0x9FF00BB0  TEB   (0x2E0 bytes)
0x9FF00E90  Stack (0x40000 bytes, 256 KB, descending)
```

- `r1` = `0x9FF40890` (stack top)
- `r13` = `0x9FF00000` (PCR base)
