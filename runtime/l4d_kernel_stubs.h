#pragma once
// l4d_kernel_stubs.h
// Forward-declarations for all Xbox 360 kernel stubs implemented in
// l4d_kernel_stubs.cpp.  Uses the raw PPC function signature so that this
// header can be included without pulling in a module-specific ppc_context.h.

#include "l4d_unified_dispatch.h"
// PPCFunc is already typedef''d in l4d_unified_dispatch.h as:
//   typedef void PPCFunc(struct PPCContext& __restrict ctx, uint8_t* base);
// So we can forward-declare each stub as just PPCFunc-typed extern functions.

// ── Bootstrap API ─────────────────────────────────────────────────────────
// Call this after L4D_InitUnifiedDispatchTable() to wire all stubs.
void L4D_RegisterKernelStubs();

// ── Individual stub forward declarations ─────────────────────────────────
// Raw signature: void name(PPCContext&, uint8_t*)
// These must match the PPC_FUNC expansion in ppc_context.h exactly.
struct PPCContext;
#define L4D_STUB_DECL(name) extern "C" void name(PPCContext& __restrict ctx, uint8_t* base)

L4D_STUB_DECL(__imp__XexLoadImage);
L4D_STUB_DECL(__imp__XexGetProcedureAddress);
L4D_STUB_DECL(__imp__XexCheckExecutablePrivilege);
L4D_STUB_DECL(__imp__DbgPrint);

L4D_STUB_DECL(__imp__XamLoaderTerminateTitle);
L4D_STUB_DECL(__imp__XamLoaderSetLaunchData);
L4D_STUB_DECL(__imp__XamLoaderGetLaunchData);
L4D_STUB_DECL(__imp__XamLoaderGetLaunchDataSize);
L4D_STUB_DECL(__imp__XamShowMessageBoxUIEx);
L4D_STUB_DECL(__imp__XGetLanguage);
L4D_STUB_DECL(__imp__XGetAVPack);

L4D_STUB_DECL(__imp__NtCreateFile);
L4D_STUB_DECL(__imp__NtOpenFile);
L4D_STUB_DECL(__imp__NtClose);
L4D_STUB_DECL(__imp__NtReadFile);
L4D_STUB_DECL(__imp__NtWriteFile);
L4D_STUB_DECL(__imp__NtFlushBuffersFile);
L4D_STUB_DECL(__imp__NtSetInformationFile);
L4D_STUB_DECL(__imp__NtQueryInformationFile);
L4D_STUB_DECL(__imp__NtQueryVolumeInformationFile);
L4D_STUB_DECL(__imp__NtQueryDirectoryFile);
L4D_STUB_DECL(__imp__NtReadFileScatter);
L4D_STUB_DECL(__imp__NtDuplicateObject);
L4D_STUB_DECL(__imp__NtCreateEvent);
L4D_STUB_DECL(__imp__NtWaitForSingleObjectEx);
L4D_STUB_DECL(__imp__NtAllocateVirtualMemory);
L4D_STUB_DECL(__imp__NtFreeVirtualMemory);
L4D_STUB_DECL(__imp__NtQueryVirtualMemory);

L4D_STUB_DECL(__imp__RtlInitAnsiString);
L4D_STUB_DECL(__imp__RtlInitializeCriticalSection);
L4D_STUB_DECL(__imp__RtlEnterCriticalSection);
L4D_STUB_DECL(__imp__RtlLeaveCriticalSection);
L4D_STUB_DECL(__imp__RtlImageXexHeaderField);
L4D_STUB_DECL(__imp__RtlNtStatusToDosError);
L4D_STUB_DECL(__imp__RtlCompareMemoryUlong);
L4D_STUB_DECL(__imp__RtlRaiseException);

L4D_STUB_DECL(__imp__KeBugCheck);
L4D_STUB_DECL(__imp__KeBugCheckEx);
L4D_STUB_DECL(__imp__KeGetCurrentProcessType);
L4D_STUB_DECL(__imp__KeTlsAlloc);
L4D_STUB_DECL(__imp__KeTlsGetValue);
L4D_STUB_DECL(__imp__KeTlsSetValue);
L4D_STUB_DECL(__imp__KeTlsFree);

L4D_STUB_DECL(__imp__ExGetXConfigSetting);
L4D_STUB_DECL(__imp__HalReturnToFirmware);
L4D_STUB_DECL(__imp____C_specific_handler);
