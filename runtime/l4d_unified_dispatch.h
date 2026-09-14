#pragma once

#include <cstdint>
#include <cstddef>
#include <cassert>

#define L4D_GLOBAL_BASE 0x82000000ull
#define L4D_GLOBAL_SPAN 0x09000000ull
#define L4D_TABLE_ENTRIES (L4D_GLOBAL_SPAN / 4)

struct PPCContext;
typedef void PPCFunc(struct PPCContext& __restrict__ ctx, uint8_t* base);

#ifndef PPC_FUNC_MAPPING_DECLARED
#define PPC_FUNC_MAPPING_DECLARED
struct PPCFuncMapping
{
    size_t guest;
    PPCFunc* host;
};
#endif

extern PPCFunc** g_l4dGlobalDispatchTable;
extern "C" void PPCIndirectCallMissing(PPCContext& ctx, uint8_t* base, uint32_t target);

// ── Boot API ────────────────────────────────────────────────────────────────
// Allocates and populates g_l4dGlobalDispatchTable with all 21 module functions.
void L4D_InitUnifiedDispatchTable();

// Reads all .pe dump files in peDumpsDir and memcpy's each section into guestBase.
// Returns true if at least one file was loaded.
#ifdef __cplusplus
#include <string>
bool L4D_PreloadAllPEData(uint8_t* guestBase, const std::string& peDumpsDir);
#endif

#ifndef PPC_LOOKUP_FUNC
#define PPC_LOOKUP_FUNC(base, target) \
    g_l4dGlobalDispatchTable[((uint32_t)(target) - (uint32_t)L4D_GLOBAL_BASE) >> 2]
#endif

#ifndef PPC_CALL_INDIRECT_FUNC
#define PPC_CALL_INDIRECT_FUNC(x) \
    do { \
        const uint32_t target_ = (uint32_t)(x); \
        if (target_ >= L4D_GLOBAL_BASE && (target_ - L4D_GLOBAL_BASE) < L4D_GLOBAL_SPAN) { \
            PPCFunc* fn_ = PPC_LOOKUP_FUNC(base, target_); \
            if (fn_) { \
                fn_(ctx, base); \
            } else { \
                PPCIndirectCallMissing(ctx, base, target_); \
            } \
        } else { \
            PPCIndirectCallMissing(ctx, base, target_); \
        } \
    } while (0)
#endif
