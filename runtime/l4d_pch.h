#pragma once
// l4d_pch.h
// Master include for L4DRecompLib .cpp files.
// Include order is mandatory — do not reorder.
//
// When compiled with -I<default.xex dir> on the path, both headers resolve
// to the module-local versions which have the correct guards and our
// PPC_LOOKUP_FUNC / PPC_CALL_INDIRECT_FUNC overrides.

// 1. ppc_config.h: defines PPC_IMAGE_BASE/SIZE/CODE_BASE, includes l4d_unified_dispatch.h
#include "ppc_config.h"

// 2. ppc_context.h (module-local "correct_ppc_context.h" version):
//    defines PPCContext struct, PPC_FUNC macros.
//    Uses #ifndef guards for PPC_LOOKUP_FUNC so our unified version wins.
#include "ppc_context.h"
