cmake_minimum_required(VERSION 3.20)
project(L4DRecompLib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(
    -fno-strict-aliasing
)

set(L4D_RECOMP_ROOT   "${CMAKE_CURRENT_SOURCE_DIR}/../../analysis/recompiled_code")
set(PPC_CONTEXT_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/XenonRecomp")
set(SIMDE_ROOT        "${PPC_CONTEXT_ROOT}/thirdparty/simde")

# ── Module list ─────────────────────────────────────────────────────────────
set(L4D_MODULES
    default.xex
    tier0_360
    vstdlib_360
    launcher_360
    FileSystem_Stdio_360
    inputsystem_360
    vphysics_360
    MaterialSystem_360
    shaderapidx9_360
    stdshader_dx9_360
    StudioRender_360
    datacache_360
    SoundEmitterSystem_360
    SceneFileCache_360
    vgui2_360
    vguimatsurface_360
    GameUI_360
    engine_360
    Client_360
    Server_360
    AppInstaller_360
)

# ── Collect all recompiled module sources ────────────────────────────────────
set(ALL_RECOMP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/l4d_unified_dispatch.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/l4d_unified_dispatch.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/l4d_kernel_stubs.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/l4d_kernel_stubs.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/l4d_cross_module_links.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/l4d_cross_module_links.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/l4d_boot.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/l4d_boot.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/l4d_autogen_stubs.cpp"
)

foreach(mod ${L4D_MODULES})
    file(GLOB MOD_SOURCES "${L4D_RECOMP_ROOT}/${mod}/*.cpp")
    list(APPEND ALL_RECOMP_SOURCES ${MOD_SOURCES})
endforeach()

# ── Static library (the full recompiled game) ────────────────────────────────
add_library(L4DRecompLib STATIC
    ${ALL_RECOMP_SOURCES}
)

# Each module needs its own directory on the include path for ppc_context.h etc.
# We pick the FIRST module dir as the authoritative ppc_context.h source (default.xex),
# since all 21 have identical correct_ppc_context.h deployed.
target_include_directories(L4DRecompLib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PPC_CONTEXT_ROOT}"
    "${SIMDE_ROOT}"
    "${L4D_RECOMP_ROOT}/default.xex"   # canonical ppc_context.h
)

foreach(mod ${L4D_MODULES})
    target_include_directories(L4DRecompLib PRIVATE
        "${L4D_RECOMP_ROOT}/${mod}"
    )
endforeach()

# ── Standalone boot-test executable ─────────────────────────────────────────
add_executable(l4d_boot_test
    "${CMAKE_CURRENT_SOURCE_DIR}/l4d_boot_main.cpp"
)

target_link_libraries(l4d_boot_test PRIVATE L4DRecompLib)
target_link_options(l4d_boot_test PRIVATE "-fuse-ld=lld" "-Wl,--gc-sections" "-Wl,--allow-multiple-definition")

target_include_directories(l4d_boot_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PPC_CONTEXT_ROOT}"
    "${SIMDE_ROOT}"
    "${L4D_RECOMP_ROOT}/default.xex"
)

if(WIN32)
    target_link_libraries(l4d_boot_test PRIVATE m)
else()
    target_link_libraries(l4d_boot_test PRIVATE pthread)
endif()
