# Xbox 360 Kernel Stubs

46 Xbox 360 kernel calls stubbed in `runtime/l4d_kernel_stubs.cpp`,
registered at the import thunk GVAs from `default.xex`.

## XEX Loader

| Stub | Behavior |
|---|---|
| XexLoadImage | Matches name suffix against 21-module table; returns imageBase |
| XexGetProcedureAddress | ordinal 1 of launcher_360 -> LauncherMain @ 0x83214180 |

## Debug / Crash

| Stub | Behavior |
|---|---|
| DbgPrint | Reads guest format string, prints to stderr |
| KeBugCheck | Prints code, calls exit(1) |
| HalReturnToFirmware | Logs and returns |

## Thread / Synchronization

| Stub | Behavior |
|---|---|
| KeTlsAlloc | Returns slot from static counter |
| KeTlsGetValue / KeTlsSetValue / KeTlsFree | No-op |
| RtlInitializeCriticalSection | No-op |
| RtlEnterCriticalSection / RtlLeaveCriticalSection | No-op |

## Memory

| Stub | Behavior |
|---|---|
| NtAllocateVirtualMemory | Guest bump allocator |
| NtFreeVirtualMemory | No-op |

## File I/O

| Stub | Behavior |
|---|---|
| NtCreateFile | Logs path, returns STATUS_NO_SUCH_FILE |
| NtReadFile / NtWriteFile | No-op |
| NtQueryInformationFile | Returns 0 |
| NtClose | No-op |

## Events

| Stub | Behavior |
|---|---|
| NtCreateEvent | Returns dummy handle |
| NtWaitForSingleObjectEx | Returns immediately |
