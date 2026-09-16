// l4d_boot.cpp
// Left 4 Dead PC Recompilation — Boot Harness
//
// Sequence:
//   1. Allocate 4 GB guest memory arena
//   2. Init unified dispatch table (~288 MB function-pointer array)
//   3. Preload all 21 PE images into guest RAM
//   4. Register Xbox 360 kernel stubs
//   5. Set up guest thread context (PCR / TLS / TEB / stack)
//   6. Jump into _xstart @ 0x82011508

// ppc_context.h must come before any use of PPCContext / PPC_FUNC
#include "l4d_pch.h"
#include "l4d_unified_dispatch.h"
#include "l4d_kernel_stubs.h"
#include "l4d_cross_module_links.h"
#include "l4d_boot.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>

// MinGW / MSVCRT compatibility: roundevenf is defined in C23 / POSIX 2024 but missing in MSVCRT
extern "C" float roundevenf(float x) {
    float r = std::round(x);
    if (std::fabs(x - r) == 0.5f) {
        if (std::fmod(r, 2.0f) != 0.0f) {
            r += (x > r) ? 1.0f : -1.0f;
        }
    }
    return r;
}


#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

// ---------------------------------------------------------------------------
// Guest memory arena
// ---------------------------------------------------------------------------
static constexpr size_t GUEST_MEM_SIZE = 0x100000000ULL;  // 4 GB

// Global Memory struct expected by ppc_context.h / function.h helpers.
// For the standalone harness we define it here; when integrated into the
// full Unleashed-derived runtime it will come from kernel/memory.cpp.
struct Memory {
    uint8_t* base{};
    void*    Translate(size_t offset)      const noexcept { return base + offset; }
    uint32_t MapVirtual(const void* host)  const noexcept {
        return (uint32_t)(static_cast<const uint8_t*>(host) - base);
    }
    PPCFunc* FindFunction(uint32_t gva)    const noexcept {
        if (gva < L4D_GLOBAL_BASE || (gva - L4D_GLOBAL_BASE) >= L4D_GLOBAL_SPAN) return nullptr;
        return PPC_LOOKUP_FUNC(base, gva);
    }
    void     InsertFunction(uint32_t gva, PPCFunc* fn) noexcept {
        if (gva >= L4D_GLOBAL_BASE && (gva - L4D_GLOBAL_BASE) < L4D_GLOBAL_SPAN) {
            PPC_LOOKUP_FUNC(base, gva) = fn;
        } else {
            fprintf(stderr, "[Memory::InsertFunction] OUT OF BOUNDS: 0x%08X\n", gva);
        }
    }
};

Memory g_memory;

extern "C" void* MmGetHostAddress(uint32_t ptr) { return g_memory.base + ptr; }

// ---------------------------------------------------------------------------
// Guest thread block layout  (mirrors Unleashed recomp's GuestThreadContext)
// ---------------------------------------------------------------------------
static constexpr size_t   PCR_SIZE          = 0xAB0;
static constexpr size_t   TLS_SIZE          = 0x100;
static constexpr size_t   TEB_SIZE          = 0x2E0;
static constexpr size_t   STACK_SIZE        = 0x40000;   // 256 KB

// Place the thread block at a stable guest address that won't conflict with
// any of the 21 module images (all are in 0x82000000-0x8B000000).
static constexpr uint32_t THREAD_BLOCK_GVA  = 0x9FF00000u;

static inline void WriteGuestU32(uint8_t* base, uint32_t gva, uint32_t val)
{
    val = __builtin_bswap32(val);
    memcpy(base + gva, &val, 4);
}

static void InitGuestThreadContext(PPCContext& ctx)
{
    uint8_t* thr = g_memory.base + THREAD_BLOCK_GVA;
    memset(thr, 0, PCR_SIZE + TLS_SIZE + TEB_SIZE + STACK_SIZE);

    // PCR[0]    = big-endian ptr to TLS block
    uint32_t tlsGva = THREAD_BLOCK_GVA + (uint32_t)PCR_SIZE;
    WriteGuestU32(g_memory.base, THREAD_BLOCK_GVA, tlsGva);

    // PCR[0x100] = big-endian ptr to TEB block
    uint32_t tebGva = THREAD_BLOCK_GVA + (uint32_t)PCR_SIZE + (uint32_t)TLS_SIZE;
    WriteGuestU32(g_memory.base, THREAD_BLOCK_GVA + 0x100, tebGva);

    // PCR[0x10C] = CPU number (0)
    thr[0x10C] = 0;

    // TLS[0x10] = 0xFFFFFFFF  (Unleashed recomp quirk — some TLS check)
    uint32_t sentinel = 0xFFFFFFFFu;
    memcpy(thr + PCR_SIZE + 0x10, &sentinel, 4);

    // r1  = stack pointer (top of the stack region, descending)
    ctx.r1.u64  = THREAD_BLOCK_GVA + (uint32_t)(PCR_SIZE + TLS_SIZE + TEB_SIZE + STACK_SIZE);
    // r13 = PCR base (thread-local storage root on Xbox 360)
    ctx.r13.u64 = THREAD_BLOCK_GVA;

    ctx.fpscr.loadFromHost();
}

// Strong function overrides for default.xex direct C++ calls
extern "C" void __imp__sub_82011B38(PPCContext& ctx, uint8_t* base) {
    printf("[Hook:__imp__sub_82011B38] Direct call Security Header -> BYPASSED (returning 1)\n");
    ctx.r3.u64 = 1;
}

extern "C" void __imp__sub_82011320(PPCContext& ctx, uint8_t* base) {
    printf("[Hook:__imp__sub_82011320] Direct call Console Privilege -> BYPASSED (returning 0)\n");
    ctx.r3.u64 = 0;
}

extern "C" void __imp__sub_82011A60(PPCContext& ctx, uint8_t* base) {
    // Empty CRT _initterm
}

// ---------------------------------------------------------------------------
// L4D_Boot
// ---------------------------------------------------------------------------
bool L4D_Boot(const std::string& peDumpsDir)
{
    printf("=== L4D PC Recomp Boot Harness ===\n\n");

    // ── 1. Allocate 4 GB guest memory ──────────────────────────────────────
    printf("[Boot] Allocating 4 GB guest memory arena...\n");
#ifdef _WIN32
    g_memory.base = (uint8_t*)VirtualAlloc(
        nullptr, GUEST_MEM_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    void* p = mmap(nullptr, GUEST_MEM_SIZE,
                   PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    g_memory.base = (p == MAP_FAILED) ? nullptr : (uint8_t*)p;
#endif
    if (!g_memory.base) {
        fprintf(stderr, "[Boot] FATAL: Could not allocate 4 GB guest memory!\n");
        return false;
    }
    printf("[Boot] Guest base: %p\n", (void*)g_memory.base);

    // ── 2. Unified dispatch table ───────────────────────────────────────────
    printf("[Boot] Initializing unified dispatch table...\n");
    L4D_InitUnifiedDispatchTable();

    // ── 3. Preload PE images ─────────────────────────────────────────────────
    printf("[Boot] Preloading PE images from: %s\n", peDumpsDir.c_str());
    if (!L4D_PreloadAllPEData(g_memory.base, peDumpsDir))
        printf("[Boot] WARN: No .pe files found — data sections zeroed.\n");

    // ── 4. Register kernel stubs ─────────────────────────────────────────────
    printf("[Boot] Registering kernel stubs...\n");
    L4D_RegisterKernelStubs();

    // ── 4a. Setup automated cross-module links ────────────────────────────────
    printf("[Boot] Setting up automated cross-module links across 21 modules...\n");
    L4D_SetupCrossModuleLinks(g_memory.base);

    // ── 4b. Boot Security & Title Hooks ──────────────────────────────────────
    // sub_82011B38: Xbox 360 security header validation check. Must return 1 (success) to avoid HalReturnToFirmware.
    g_memory.InsertFunction(0x82011B38u, [](PPCContext& ctx, uint8_t* base) {
        printf("[Hook:sub_82011B38] Xbox 360 Security Header check -> BYPASSED (returning 1)\n");
        ctx.r3.u64 = 1;
    });

    // sub_82011320: Check for debug / development console privileges. Must return 0 to prevent XamLoaderTerminateTitle.
    g_memory.InsertFunction(0x82011320u, [](PPCContext& ctx, uint8_t* base) {
        printf("[Hook:sub_82011320] Console Privilege check -> BYPASSED (returning 0)\n");
        ctx.r3.u64 = 0;
    });

    // sub_82011A60: CRT static initializer loop (_initterm). In static PE dumps, uninitialized list pointers are null,
    // causing an infinite loop calling 0x0. Bypass empty CRT static initializers.
    g_memory.InsertFunction(0x82011A60u, [](PPCContext& ctx, uint8_t* base) {
        printf("[Hook:sub_82011A60] CRT static initializers -> BYPASSED\n");
    });

    // CAppSystemGroup / Subsystem initialization hooks:
    // sub_8321A328: Initializes subsystems and checks their vtable methods.
    // In our standalone recomp harness without full dynamic COM interface modules yet,
    // returning 1 (success) allows LauncherMain to cleanly pass the stage checks!
    g_memory.InsertFunction(0x8321A328u, [](PPCContext& ctx, uint8_t* base) {
        printf("[Hook:sub_8321A328 (CAppSystemGroup::InitStage)] -> SUCCESS (1)\n");
        // stw r10, 68(r31) where r10 = 8 (stage 8 = success)
        uint32_t thisPtr = ctx.r3.u32;
        WriteGuestU32(base, thisPtr + 68, 8);
        ctx.r3.u64 = 1;
    });

    // sub_83213A48: Post-Init subsystem shutdown/cleanup validation.
    g_memory.InsertFunction(0x83213A48u, [](PPCContext& ctx, uint8_t* base) {
        printf("[Hook:sub_83213A48 (Subsystem Post-Init)] -> BYPASSED\n");
        ctx.r3.u64 = 1;
    });

    // ── 5. Guest thread context ──────────────────────────────────────────────
    printf("[Boot] Setting up guest thread context...\n");
    PPCContext ctx{};
    InitGuestThreadContext(ctx);

    // ── 5a. Module static initializers / DllMain ────────────────────────────
    printf("[Boot] Initializing tier0_360 via DllMain (0x8279F888)...\n");
    PPCFunc* tier0DllMain = g_memory.FindFunction(0x8279F888u);
    if (tier0DllMain) {
        ctx.r3.u64 = 0x82780000u; // hinstDLL
        ctx.r4.u64 = 1;          // DLL_PROCESS_ATTACH
        ctx.r5.u64 = 0;          // lpReserved
        tier0DllMain(ctx, g_memory.base);
        printf("[Boot] tier0_360 DllMain returned r3 = 0x%08X\n", ctx.r3.u32);
        uint32_t cmdlineVtable = __builtin_bswap32(*(uint32_t*)(g_memory.base + 0x827C00D8));
        uint32_t crtHeap = __builtin_bswap32(*(uint32_t*)(g_memory.base + 0x827CB9F8));
        printf("[Boot] CommandLine singleton @ 0x827C00D8 vtable = 0x%08X, crtHeap = 0x%08X\n", cmdlineVtable, crtHeap);
    } else {
        printf("[Boot] WARN: tier0 DllMain (0x8279F888) not found in dispatch table!\n");
    }

    // ── 6. Verify and Jump into LauncherMain ───────────────────────────────
    static constexpr uint32_t LAUNCHER_MAIN_GVA = 0x83214180u;
    PPCFunc* launcherMain = g_memory.FindFunction(LAUNCHER_MAIN_GVA);
    if (!launcherMain) {
        fprintf(stderr,
            "[Boot] FATAL: LauncherMain (0x%08X) missing from dispatch table!\n"
            "       Check that L4D_InitUnifiedDispatchTable() ran successfully.\n",
            LAUNCHER_MAIN_GVA);
        return false;
    }
    printf("[Boot] LauncherMain @ 0x%08X -> host fn %p\n\n", LAUNCHER_MAIN_GVA, (void*)launcherMain);
    fflush(stdout);

    // Setup LauncherMain args: r3 = hInstance, r4 = hPrevInstance, r5 = lpCmdLine, r6 = nCmdShow
    ctx.r3.u64 = 0x83200000u;
    ctx.r4.u64 = 0;
    static const uint32_t CMDLINE_STR_GVA = 0x8AE02000u;
    const char* defaultCmdline = "-game left4dead -novid";
    strcpy((char*)(g_memory.base + CMDLINE_STR_GVA), defaultCmdline);
    ctx.r5.u64 = CMDLINE_STR_GVA;
    ctx.r6.u64 = 1;

    // ── 7. Jump! ─────────────────────────────────────────────────────────────
    launcherMain(ctx, g_memory.base);

    printf("\n[Boot] LauncherMain returned (r3 = 0x%08X)\n", ctx.r3.u32);
    return true;
}
