// l4d_kernel_stubs.cpp
// Xbox 360 kernel stubs for Left 4 Dead PC recompilation.
// Ported from Sonic Unleashed Recomp with L4D multi-module XexLoadImage.

// ppc_context.h is the canonical include that defines PPCContext and PPC_FUNC.
// We pull the one from XenonUtils (which is on the include path via -I).
#include "l4d_pch.h"
#include "l4d_unified_dispatch.h"
#include "l4d_kernel_stubs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <mutex>
#include <vector>
#include <string>
#include <atomic>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

// ---------------------------------------------------------------------------
// Helper: read/write big-endian 32-bit values in guest memory
// ---------------------------------------------------------------------------
static inline uint32_t GuestU32(const uint8_t* base, uint32_t gva)
{
    uint32_t v;
    memcpy(&v, base + gva, 4);
    return __builtin_bswap32(v);
}

static inline void WriteGuestU32(uint8_t* base, uint32_t gva, uint32_t val)
{
    val = __builtin_bswap32(val);
    memcpy(base + gva, &val, 4);
}

static inline const char* GuestStr(const uint8_t* base, uint32_t gva)
{
    if (!gva) return "(null)";
    return reinterpret_cast<const char*>(base + gva);
}

// ---------------------------------------------------------------------------
// Module handle table  (name suffix -> guest imageBase)
// ---------------------------------------------------------------------------
struct L4DModule {
    const char*     name;
    uint32_t        imageBase;
    PPCFuncMapping* mappings;
};

extern PPCFuncMapping default_xex_PPCFuncMappings[];
extern PPCFuncMapping tier0_360_PPCFuncMappings[];
extern PPCFuncMapping vstdlib_360_PPCFuncMappings[];
extern PPCFuncMapping launcher_360_PPCFuncMappings[];
extern PPCFuncMapping FileSystem_Stdio_360_PPCFuncMappings[];
extern PPCFuncMapping inputsystem_360_PPCFuncMappings[];
extern PPCFuncMapping vphysics_360_PPCFuncMappings[];
extern PPCFuncMapping MaterialSystem_360_PPCFuncMappings[];
extern PPCFuncMapping shaderapidx9_360_PPCFuncMappings[];
extern PPCFuncMapping stdshader_dx9_360_PPCFuncMappings[];
extern PPCFuncMapping StudioRender_360_PPCFuncMappings[];
extern PPCFuncMapping datacache_360_PPCFuncMappings[];
extern PPCFuncMapping SoundEmitterSystem_360_PPCFuncMappings[];
extern PPCFuncMapping SceneFileCache_360_PPCFuncMappings[];
extern PPCFuncMapping vgui2_360_PPCFuncMappings[];
extern PPCFuncMapping vguimatsurface_360_PPCFuncMappings[];
extern PPCFuncMapping GameUI_360_PPCFuncMappings[];
extern PPCFuncMapping engine_360_PPCFuncMappings[];
extern PPCFuncMapping Client_360_PPCFuncMappings[];
extern PPCFuncMapping Server_360_PPCFuncMappings[];
extern PPCFuncMapping AppInstaller_360_PPCFuncMappings[];

static L4DModule g_l4dModuleTable[] = {
    { "default.xex",              0x82010000, default_xex_PPCFuncMappings          },
    { "tier0_360.dll",            0x82790000, tier0_360_PPCFuncMappings             },
    { "vstdlib_360.dll",          0x82990000, vstdlib_360_PPCFuncMappings           },
    { "launcher_360.dll",         0x83210000, launcher_360_PPCFuncMappings          },
    { "filesystem_stdio_360.dll", 0x82B90000, FileSystem_Stdio_360_PPCFuncMappings  },
    { "inputsystem_360.dll",      0x83090000, inputsystem_360_PPCFuncMappings       },
    { "vphysics_360.dll",         0x82220000, vphysics_360_PPCFuncMappings          },
    { "materialsystem_360.dll",   0x843A0000, MaterialSystem_360_PPCFuncMappings    },
    { "shaderapidx9_360.dll",     0x84CB0000, shaderapidx9_360_PPCFuncMappings      },
    { "stdshader_dx9_360.dll",    0x857A0000, stdshader_dx9_360_PPCFuncMappings     },
    { "studiorender_360.dll",     0x85C10000, StudioRender_360_PPCFuncMappings      },
    { "datacache_360.dll",        0x82E10000, datacache_360_PPCFuncMappings         },
    { "soundemittersystem_360.dll",0x83410000,SoundEmitterSystem_360_PPCFuncMappings},
    { "scenefilecache_360.dll",   0x83610000, SceneFileCache_360_PPCFuncMappings    },
    { "vgui2_360.dll",            0x83790000, vgui2_360_PPCFuncMappings             },
    { "vguimatsurface_360.dll",   0x848A0000, vguimatsurface_360_PPCFuncMappings    },
    { "gameui_360.dll",           0x83BE0000, GameUI_360_PPCFuncMappings            },
    { "engine_360.dll",           0x86410000, engine_360_PPCFuncMappings            },
    { "client_360.dll",           0x87A90000, Client_360_PPCFuncMappings            },
    { "server_360.dll",           0x89130000, Server_360_PPCFuncMappings            },
    { "appinstaller_360.dll",     0x8A870000, AppInstaller_360_PPCFuncMappings      },
    { nullptr, 0, nullptr }
};

static L4DModule* L4D_FindModuleByName(const char* name)
{
    for (L4DModule* m = g_l4dModuleTable; m->name; ++m) {
        size_t nameLen = strlen(name);
        size_t keyLen  = strlen(m->name);
        if (nameLen >= keyLen) {
            const char* suffix = name + (nameLen - keyLen);
            bool match = true;
            for (size_t i = 0; i < keyLen; ++i) {
                if (tolower((unsigned char)suffix[i]) !=
                    tolower((unsigned char)m->name[i])) {
                    match = false; break;
                }
            }
            if (match) return m;
        }
    }
    return nullptr;
}

static L4DModule* L4D_FindModuleByHandle(uint32_t handle)
{
    for (L4DModule* m = g_l4dModuleTable; m->name; ++m)
        if (m->imageBase == handle) return m;
    return nullptr;
}

// ---------------------------------------------------------------------------
// NTSTATUS constants
// ---------------------------------------------------------------------------
static constexpr uint32_t STATUS_SUCCESS            = 0x00000000u;
static constexpr uint32_t STATUS_NOT_IMPLEMENTED    = 0xC0000002u;
static constexpr uint32_t STATUS_OBJECT_NOT_FOUND   = 0xC0000034u;
static constexpr uint32_t STATUS_NOT_FOUND          = 0xC0000225u;

#ifdef STATUS_DLL_NOT_FOUND
#undef STATUS_DLL_NOT_FOUND
#endif
#ifdef STATUS_NO_SUCH_FILE
#undef STATUS_NO_SUCH_FILE
#endif
static constexpr uint32_t STATUS_NO_SUCH_FILE       = 0xC000000Fu;
static constexpr uint32_t STATUS_DLL_NOT_FOUND      = 0xC0000135u;

// ---------------------------------------------------------------------------
// XexLoadImage
// r3=path_gva  r4=flags  r5=0  r6=out_handle_gva
// Returns NTSTATUS in r3
// ---------------------------------------------------------------------------
PPC_FUNC_IMPL(__imp__XexLoadImage)
{
    uint32_t pathGva = ctx.r3.u32;
    uint32_t outGva  = ctx.r6.u32;
    const char* path = GuestStr(base, pathGva);
    printf("[XexLoadImage] Requested: \"%s\"\n", path);

    // Optional debug monitor DLL: Xbox 360 titles check for this to enable dev features.
    // If not present, returning STATUS_NO_SUCH_FILE (0xC000000F) allows the title to cleanly
    // know vxbdm is absent without erroring out.
    if (strstr(path, "vxbdm")) {
        printf("[XexLoadImage] Optional debug monitor \"%s\" not present -> STATUS_NO_SUCH_FILE\n", path);
        if (outGva) WriteGuestU32(base, outGva, 0);
        ctx.r3.u64 = STATUS_NO_SUCH_FILE;
        return;
    }

    L4DModule* mod = L4D_FindModuleByName(path);
    if (!mod) {
        fprintf(stderr, "[XexLoadImage] Unknown module \"%s\"\n", path);
        if (outGva) WriteGuestU32(base, outGva, 0);
        ctx.r3.u64 = STATUS_NO_SUCH_FILE;
        return;
    }
    printf("[XexLoadImage] -> handle 0x%08X  (%s)\n", mod->imageBase, mod->name);
    if (outGva) WriteGuestU32(base, outGva, mod->imageBase);
    ctx.r3.u64 = STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// XexGetProcedureAddress
// r3=handle  r4=ordinal  r5=out_addr_gva
// ---------------------------------------------------------------------------
PPC_FUNC_IMPL(__imp__XexGetProcedureAddress)
{
    uint32_t handle  = ctx.r3.u32;
    uint32_t ordinal = ctx.r4.u32;
    uint32_t outGva  = ctx.r5.u32;

    L4DModule* mod = L4D_FindModuleByHandle(handle);
    if (!mod) {
        fprintf(stderr, "[XexGetProcedureAddress] Unknown handle 0x%08X\n", handle);
        if (outGva) WriteGuestU32(base, outGva, 0);
        ctx.r3.u64 = STATUS_OBJECT_NOT_FOUND;
        return;
    }

    // Handle well-known exports
    // launcher_360.dll: Ordinal 1 is LauncherMain @ 0x83214180
    if (mod->imageBase == 0x83210000 && (ordinal == 1 || ordinal == 0x83214180)) {
        printf("[XexGetProcedureAddress] mod=%s ordinal=%u -> LauncherMain @ 0x83214180\n",
               mod->name, ordinal);
        if (outGva) WriteGuestU32(base, outGva, 0x83214180u);
        ctx.r3.u64 = STATUS_SUCCESS;
        return;
    }

    // All cross-module calls are already dispatch-table-resolved.
    // Return 0 (not found) for unknown ordinals — callers check before use.
    printf("[XexGetProcedureAddress] mod=%s ordinal=0x%08X -> returning 0\n",
           mod->name, ordinal);
    if (outGva) WriteGuestU32(base, outGva, 0);
    ctx.r3.u64 = STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// DbgPrint  r3=fmt_gva  r4..r7=args
// ---------------------------------------------------------------------------
PPC_FUNC_IMPL(__imp__DbgPrint)
{
    const char* fmt = GuestStr(base, ctx.r3.u32);
    printf("[DbgPrint] ");
    printf(fmt, (unsigned)ctx.r4.u32, (unsigned)ctx.r5.u32,
                (unsigned)ctx.r6.u32, (unsigned)ctx.r7.u32);
    size_t len = strlen(fmt);
    if (len == 0 || fmt[len-1] != '\n') printf("\n");
    ctx.r3.u64 = 0;
}

PPC_FUNC_IMPL(__imp__XexCheckExecutablePrivilege) { ctx.r3.u64 = 1; }

// ---------------------------------------------------------------------------
// XAM stubs
// ---------------------------------------------------------------------------
PPC_FUNC_IMPL(__imp__XamLoaderTerminateTitle)
{
    printf("[XamLoaderTerminateTitle] Exiting.\n");
    exit(0);
}
PPC_FUNC_IMPL(__imp__XamLoaderSetLaunchData)     { ctx.r3.u64 = STATUS_SUCCESS; }
PPC_FUNC_IMPL(__imp__XamLoaderGetLaunchData)     { ctx.r3.u64 = STATUS_NOT_FOUND; }
PPC_FUNC_IMPL(__imp__XamLoaderGetLaunchDataSize) { ctx.r3.u64 = 0; }
PPC_FUNC_IMPL(__imp__XamShowMessageBoxUIEx)      { ctx.r3.u64 = STATUS_SUCCESS; }
PPC_FUNC_IMPL(__imp__XGetLanguage)               { ctx.r3.u64 = 1; }  // XLANGAGE_ENGLISH
PPC_FUNC_IMPL(__imp__XGetAVPack)                 { ctx.r3.u64 = 6; }  // XAVPACK_HDMI

// ---------------------------------------------------------------------------
// NT File I/O stubs
// ---------------------------------------------------------------------------
PPC_FUNC_IMPL(__imp__NtCreateFile)           { ctx.r3.u64 = STATUS_NOT_IMPLEMENTED; }
PPC_FUNC_IMPL(__imp__NtOpenFile)             { ctx.r3.u64 = STATUS_NOT_IMPLEMENTED; }
PPC_FUNC_IMPL(__imp__NtClose)                { ctx.r3.u64 = STATUS_SUCCESS; }
PPC_FUNC_IMPL(__imp__NtReadFile)             { ctx.r3.u64 = STATUS_NOT_IMPLEMENTED; }
PPC_FUNC_IMPL(__imp__NtWriteFile)            { ctx.r3.u64 = STATUS_NOT_IMPLEMENTED; }
PPC_FUNC_IMPL(__imp__NtFlushBuffersFile)     { ctx.r3.u64 = STATUS_SUCCESS; }
PPC_FUNC_IMPL(__imp__NtSetInformationFile)   { ctx.r3.u64 = STATUS_SUCCESS; }
PPC_FUNC_IMPL(__imp__NtQueryInformationFile) { ctx.r3.u64 = STATUS_NOT_IMPLEMENTED; }
PPC_FUNC_IMPL(__imp__NtQueryVolumeInformationFile) { ctx.r3.u64 = STATUS_NOT_IMPLEMENTED; }
PPC_FUNC_IMPL(__imp__NtQueryDirectoryFile)   { ctx.r3.u64 = STATUS_NOT_IMPLEMENTED; }
PPC_FUNC_IMPL(__imp__NtReadFileScatter)      { ctx.r3.u64 = STATUS_NOT_IMPLEMENTED; }
PPC_FUNC_IMPL(__imp__NtDuplicateObject)      { ctx.r3.u64 = STATUS_NOT_IMPLEMENTED; }

PPC_FUNC_IMPL(__imp__NtCreateEvent)
{
    uint32_t outGva = ctx.r3.u32;
    if (outGva) WriteGuestU32(base, outGva, 0xDEAD0001u);
    ctx.r3.u64 = STATUS_SUCCESS;
}

PPC_FUNC_IMPL(__imp__NtWaitForSingleObjectEx) { ctx.r3.u64 = STATUS_SUCCESS; }

// ---------------------------------------------------------------------------
// NtAllocate/FreeVirtualMemory — bump allocator above 0xA0100000
// ---------------------------------------------------------------------------
static std::atomic<uint32_t> s_vmNextAlloc{0xA0100000u};

PPC_FUNC_IMPL(__imp__NtAllocateVirtualMemory)
{
    uint32_t baseAddrPtrGva  = ctx.r4.u32;
    uint32_t regionSizePtrGva = ctx.r6.u32;

    uint32_t requestedSize = regionSizePtrGva ? GuestU32(base, regionSizePtrGva) : 0;
    if (requestedSize == 0) requestedSize = 0x10000;
    uint32_t aligned = (requestedSize + 0xFFFFu) & ~0xFFFFu;
    uint32_t gva = s_vmNextAlloc.fetch_add(aligned);

    if (baseAddrPtrGva)  WriteGuestU32(base, baseAddrPtrGva, gva);
    if (regionSizePtrGva) WriteGuestU32(base, regionSizePtrGva, aligned);
    ctx.r3.u64 = STATUS_SUCCESS;
}

PPC_FUNC_IMPL(__imp__NtFreeVirtualMemory)    { ctx.r3.u64 = STATUS_SUCCESS; }
PPC_FUNC_IMPL(__imp__NtQueryVirtualMemory)   { ctx.r3.u64 = STATUS_NOT_IMPLEMENTED; }

// ---------------------------------------------------------------------------
// Rtl* stubs
// ---------------------------------------------------------------------------
PPC_FUNC_IMPL(__imp__RtlInitAnsiString)
{
    uint32_t strPtr = ctx.r3.u32;
    uint32_t srcGva = ctx.r4.u32;
    if (!strPtr) { ctx.r3.u64 = 0; return; }

    uint16_t len = srcGva ? (uint16_t)strlen(GuestStr(base, srcGva)) : 0;
    uint16_t lenBE = __builtin_bswap16(len);
    uint16_t maxBE = __builtin_bswap16((uint16_t)(len + 1));
    uint32_t ptrBE = __builtin_bswap32(srcGva);
    memcpy(base + strPtr,     &lenBE, 2);
    memcpy(base + strPtr + 2, &maxBE, 2);
    memcpy(base + strPtr + 4, &ptrBE, 4);
    ctx.r3.u64 = STATUS_SUCCESS;
}

static std::vector<std::pair<uint32_t, std::mutex*>> g_critSections;
static std::mutex g_critMapLock;

static std::mutex* GetOrCreateMutex(uint32_t gva)
{
    std::lock_guard<std::mutex> lg(g_critMapLock);
    for (auto& p : g_critSections)
        if (p.first == gva) return p.second;
    auto* m = new std::mutex();
    g_critSections.push_back({gva, m});
    return m;
}

PPC_FUNC_IMPL(__imp__RtlInitializeCriticalSection)
{
    GetOrCreateMutex(ctx.r3.u32);
    ctx.r3.u64 = STATUS_SUCCESS;
}
PPC_FUNC_IMPL(__imp__RtlEnterCriticalSection) { GetOrCreateMutex(ctx.r3.u32)->lock(); }
PPC_FUNC_IMPL(__imp__RtlLeaveCriticalSection) { GetOrCreateMutex(ctx.r3.u32)->unlock(); }

PPC_FUNC_IMPL(__imp__RtlImageXexHeaderField)   { ctx.r3.u64 = 0; }
PPC_FUNC_IMPL(__imp__RtlNtStatusToDosError)
{
    ctx.r3.u64 = (ctx.r3.u32 == 0) ? 0u : 0x7FFFFFFFu;
}
PPC_FUNC_IMPL(__imp__RtlCompareMemoryUlong)    { ctx.r3.u64 = 0; }
PPC_FUNC_IMPL(__imp__RtlRaiseException)
{
    fprintf(stderr, "[RtlRaiseException] Guest raised exception @ lr=0x%08X\n",
            (uint32_t)ctx.lr);
    abort();
}

// ---------------------------------------------------------------------------
// KE stubs
// ---------------------------------------------------------------------------
PPC_FUNC_IMPL(__imp__KeBugCheck)
{
    fprintf(stderr, "[KeBugCheck] code=0x%08X\n", ctx.r3.u32);
    abort();
}
PPC_FUNC_IMPL(__imp__KeBugCheckEx)
{
    fprintf(stderr, "[KeBugCheckEx] code=0x%08X\n", ctx.r3.u32);
    abort();
}
PPC_FUNC_IMPL(__imp__KeGetCurrentProcessType) { ctx.r3.u64 = 2; }  // ProcessTypeTitle

static constexpr uint32_t TLS_TABLE_GVA = 0xA00F0000u;
static constexpr uint32_t TLS_SLOTS     = 256u;
static std::atomic<uint32_t> s_nextTlsSlot{0};

PPC_FUNC_IMPL(__imp__KeTlsAlloc)
{
    uint32_t slot = s_nextTlsSlot.fetch_add(1);
    ctx.r3.u64 = (slot < TLS_SLOTS) ? slot : 0xFFFFFFFFu;
}
PPC_FUNC_IMPL(__imp__KeTlsGetValue)
{
    uint32_t slot = ctx.r3.u32;
    ctx.r3.u64 = (slot < TLS_SLOTS) ? GuestU32(base, TLS_TABLE_GVA + slot * 4) : 0;
}
PPC_FUNC_IMPL(__imp__KeTlsSetValue)
{
    uint32_t slot = ctx.r3.u32;
    if (slot < TLS_SLOTS) WriteGuestU32(base, TLS_TABLE_GVA + slot * 4, ctx.r4.u32);
    ctx.r3.u64 = 1;
}
PPC_FUNC_IMPL(__imp__KeTlsFree) { ctx.r3.u64 = 1; }

// ---------------------------------------------------------------------------
// Misc
// ---------------------------------------------------------------------------
PPC_FUNC_IMPL(__imp__ExGetXConfigSetting)     { ctx.r3.u64 = STATUS_NOT_IMPLEMENTED; }
PPC_FUNC_IMPL(__imp__HalReturnToFirmware) { printf("[HalReturnToFirmware] r3=0x%08X lr=0x%08X\n", (uint32_t)ctx.r3.u32, (uint32_t)ctx.lr); }
PPC_FUNC_IMPL(__imp____C_specific_handler)    { ctx.r3.u64 = 0; }

// ============================================================================
// PPCIndirectCallMissing
// ============================================================================
extern "C" void PPCIndirectCallMissing(PPCContext& ctx, uint8_t* base, uint32_t target)
{
    fprintf(stderr,
        "[PPCIndirectCallMissing] Unmapped call 0x%08X  (lr=0x%08X)\n",
        target, (uint32_t)ctx.lr);
}

// ============================================================================
// L4D_RegisterKernelStubs
// ============================================================================
void L4D_RegisterKernelStubs()
{
    if (!g_l4dGlobalDispatchTable) {
        fprintf(stderr, "[L4D_RegisterKernelStubs] Dispatch table not initialized!\n");
        return;
    }

    auto reg = [](uint32_t gva, PPCFunc* fn) {
        if (gva >= (uint32_t)L4D_GLOBAL_BASE &&
            (gva - (uint32_t)L4D_GLOBAL_BASE) < (uint32_t)L4D_GLOBAL_SPAN)
        {
            uint32_t idx = (gva - (uint32_t)L4D_GLOBAL_BASE) >> 2;
            g_l4dGlobalDispatchTable[idx] = fn;
        }
    };

    // default.xex kernel thunk addresses (ppc_func_mapping.cpp L325-L370)
    reg(0x82018688u, __imp__XamLoaderSetLaunchData);
    reg(0x82018698u, __imp__XamLoaderGetLaunchData);
    reg(0x820186A8u, __imp__XamLoaderGetLaunchDataSize);
    reg(0x820186B8u, __imp__XamShowMessageBoxUIEx);
    reg(0x820186C8u, __imp__XGetLanguage);
    reg(0x820186D8u, __imp__XGetAVPack);
    reg(0x820186E8u, __imp__XamLoaderTerminateTitle);
    reg(0x820186F8u, __imp__NtWaitForSingleObjectEx);
    reg(0x82018708u, __imp__RtlInitAnsiString);
    reg(0x82018718u, __imp__XexGetProcedureAddress);
    reg(0x82018728u, __imp__XexLoadImage);
    reg(0x82018738u, __imp__NtClose);
    reg(0x82018748u, __imp__NtCreateEvent);
    reg(0x82018758u, __imp__ExGetXConfigSetting);
    reg(0x82018768u, __imp__XexCheckExecutablePrivilege);
    reg(0x82018778u, __imp__DbgPrint);
    reg(0x82018788u, __imp____C_specific_handler);
    reg(0x82018798u, __imp__RtlNtStatusToDosError);
    reg(0x820187A8u, __imp__NtOpenFile);
    reg(0x820187B8u, __imp__NtSetInformationFile);
    reg(0x820187C8u, __imp__NtQueryInformationFile);
    reg(0x820187D8u, __imp__NtQueryVolumeInformationFile);
    reg(0x820187E8u, __imp__NtQueryDirectoryFile);
    reg(0x820187F8u, __imp__NtReadFileScatter);
    reg(0x82018808u, __imp__NtReadFile);
    reg(0x82018818u, __imp__NtCreateFile);
    reg(0x82018828u, __imp__NtDuplicateObject);
    reg(0x82018838u, __imp__RtlLeaveCriticalSection);
    reg(0x82018848u, __imp__RtlEnterCriticalSection);
    reg(0x82018858u, __imp__RtlImageXexHeaderField);
    reg(0x82018868u, __imp__HalReturnToFirmware);
    reg(0x82018878u, __imp__NtAllocateVirtualMemory);
    reg(0x82018888u, __imp__KeBugCheckEx);
    reg(0x82018898u, __imp__KeGetCurrentProcessType);
    reg(0x820188A8u, __imp__NtFreeVirtualMemory);
    reg(0x820188B8u, __imp__RtlCompareMemoryUlong);
    reg(0x820188C8u, __imp__RtlInitializeCriticalSection);
    reg(0x820188D8u, __imp__NtQueryVirtualMemory);
    reg(0x820188E8u, __imp__RtlRaiseException);
    reg(0x820188F8u, __imp__KeBugCheck);
    reg(0x82018908u, __imp__KeTlsAlloc);
    reg(0x82018918u, __imp__KeTlsGetValue);
    reg(0x82018928u, __imp__KeTlsSetValue);
    reg(0x82018938u, __imp__KeTlsFree);
    reg(0x82018948u, __imp__NtWriteFile);
    reg(0x82018958u, __imp__NtFlushBuffersFile);

    printf("[L4D_RegisterKernelStubs] 46 stubs registered.\n");
}



