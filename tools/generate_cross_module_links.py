import os
import re
import struct
import glob
import shutil

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", "..", "..", "Projects", "L4DRecomp"))
if not os.path.exists(os.path.join(REPO_ROOT, "extracted_game")):
    REPO_ROOT = "c:\\Users\\Carlos\\Projects\\L4DRecomp"

XEX_ROOT = os.path.join(REPO_ROOT, "extracted_game", "root")
PE_ROOT = os.path.join(REPO_ROOT, "analysis", "pe_dumps")
RECOMP_ROOT = os.path.join(REPO_ROOT, "analysis", "recompiled_code")
OUTPUT_DIR = os.path.join(REPO_ROOT, "L4DRecomp-PC", "L4DRecompLib")

MODULE_NAMES = [
    "launcher_360",
    "tier0_360",
    "vstdlib_360",
    "FileSystem_Stdio_360",
    "vphysics_360",
    "MaterialSystem_360",
    "shaderapidx9_360",
    "StudioRender_360",
    "datacache_360",
    "SoundEmitterSystem_360",
    "inputsystem_360",
    "SceneFileCache_360",
    "vgui2_360",
    "vguimatsurface_360",
    "stdshader_dx9_360",
    "GameUI_360",
    "engine_360",
    "Client_360",
    "Server_360",
    "AppInstaller_360",
    "default.xex",
]

def find_xex_path(mod_name):
    candidates = [
        os.path.join(XEX_ROOT, "bin", mod_name),
        os.path.join(XEX_ROOT, "bin", mod_name + ".dll"),
        os.path.join(XEX_ROOT, mod_name),
        os.path.join(XEX_ROOT, mod_name + ".xex"),
        os.path.join(XEX_ROOT, "left4dead", "bin", mod_name),
        os.path.join(XEX_ROOT, "left4dead", "bin", mod_name + ".dll"),
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    return None

def find_pe_path(mod_name):
    candidates = [
        os.path.join(PE_ROOT, f"{mod_name}.pe"),
        os.path.join(PE_ROOT, f"bin_{mod_name}.pe"),
        os.path.join(PE_ROOT, f"bin_{mod_name}.dll.pe"),
        os.path.join(PE_ROOT, f"{mod_name}.dll.pe"),
        os.path.join(PE_ROOT, f"left4dead_bin_{mod_name}.pe"),
        os.path.join(PE_ROOT, f"left4dead_bin_{mod_name}.dll.pe"),
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    return None

def clean_mod_name(name):
    n = name.lower()
    for ext in [".dll", ".xex", ".exe"]:
        if n.endswith(ext):
            n = n[:-len(ext)]
    return n

def main():
    print("=== L4D Automated Cross-Module Linker Generator ===")
    
    # 1. Parse symbol mappings from ppc_func_mapping.cpp
    print("[1/5] Loading symbol mappings from recompiled modules...")
    mod_symbols = {}
    for mod in MODULE_NAMES:
        clean = clean_mod_name(mod)
        mapping_file = os.path.join(RECOMP_ROOT, mod, "ppc_func_mapping.cpp")
        if not os.path.exists(mapping_file):
            mapping_file = os.path.join(RECOMP_ROOT, clean, "ppc_func_mapping.cpp")
        syms = {}
        if os.path.exists(mapping_file):
            with open(mapping_file, "r", errors="ignore") as f:
                for line in f:
                    m = re.search(r'\{\s*(0x[0-9A-Fa-f]+)\s*,\s*([^\s,\}]+)\s*\}', line)
                    if m:
                        gva = int(m.group(1), 16)
                        sym = m.group(2)
                        syms[gva] = sym
        mod_symbols[clean] = syms
        print(f"  {clean:24s}: {len(syms)} symbols")

    # 2. Extract export tables
    print("\n[2/5] Extracting export tables...")
    module_exports = {}
    module_load_addrs = {}

    for mod in MODULE_NAMES:
        clean = clean_mod_name(mod)
        xex_path = find_xex_path(mod)
        pe_path = find_pe_path(mod)
        if not xex_path or not pe_path:
            continue

        with open(xex_path, "rb") as f:
            xex_data = f.read()
        sec_off = struct.unpack_from(">I", xex_data, 0x10)[0]
        load_addr = struct.unpack_from(">I", xex_data, sec_off + 0x110)[0]
        exp_tbl_gva = struct.unpack_from(">I", xex_data, sec_off + 0x160)[0]
        module_load_addrs[clean] = load_addr

        if exp_tbl_gva == 0:
            continue

        with open(pe_path, "rb") as f:
            pe_data = f.read()

        exp_rva = exp_tbl_gva - load_addr
        if exp_rva + 0x2C > len(pe_data):
            continue

        imgbase, count, base = struct.unpack_from(">III", pe_data, exp_rva + 0x20)
        exports = {}
        for i in range(count):
            off = struct.unpack_from(">I", pe_data, exp_rva + 0x2C + i * 4)[0]
            if off != 0:
                fn_gva = (imgbase << 16) + off
                exports[base + i] = fn_gva

        module_exports[clean] = exports
        print(f"  {clean:24s}: load_addr=0x{load_addr:08X}, base_ord={base}, exports={len(exports)}")

    # 3. Resolve imports across all modules
    print("\n[3/5] Resolving cross-module imports...")
    links = []

    for mod in MODULE_NAMES:
        clean = clean_mod_name(mod)
        xex_path = find_xex_path(mod)
        pe_path = find_pe_path(mod)
        if not xex_path or not pe_path:
            continue

        with open(xex_path, "rb") as f:
            data = f.read()
        with open(pe_path, "rb") as f:
            pe_data = f.read()

        num_opt_headers = struct.unpack_from(">I", data, 0x14)[0]
        imp_offset = None
        off = 0x18
        for i in range(num_opt_headers):
            k, v = struct.unpack_from(">II", data, off)
            if k == 0x000103FF:
                imp_offset = v
                break
            off += 8

        if not imp_offset:
            continue

        total_size, str_table_size, lib_count = struct.unpack_from(">III", data, imp_offset)
        str_start = imp_offset + 12
        str_data = data[str_start : str_start + str_table_size]

        # Parse string table as list of strings
        strs = []
        pos = 0
        while pos < len(str_data):
            end = str_data.find(b"\x00", pos)
            if end == -1: break
            s = str_data[pos:end].decode("ascii", errors="ignore")
            if s: strs.append(s)
            pos = end + 1
            while pos < len(str_data) and pos % 4 != 0:
                pos += 1

        lib_off = str_start + str_table_size
        mod_load_addr = module_load_addrs[clean]

        for j in range(lib_count):
            lib_size = struct.unpack_from(">I", data, lib_off)[0]
            name_idx = struct.unpack_from(">H", data, lib_off + 36)[0] & 0xFF
            num_records = struct.unpack_from(">H", data, lib_off + 38)[0]
            lib_name = strs[name_idx] if name_idx < len(strs) else f"lib_{name_idx}"
            clean_target = clean_mod_name(lib_name)

            if clean_target not in module_exports:
                lib_off += lib_size
                continue

            target_exports = module_exports[clean_target]
            target_syms = mod_symbols.get(clean_target, {})
            importing_syms = mod_symbols.get(clean, {})

            # Parse imports per XEX2 spec
            # record_type == 0: variable / IAT slot
            # record_type == 1: function thunk for previous import
            lib_imports = []
            for i in range(num_records):
                record_addr = struct.unpack_from(">I", data, lib_off + 40 + i * 4)[0]
                pe_off = record_addr - mod_load_addr
                if 0 <= pe_off + 4 <= len(pe_data):
                    record_val = struct.unpack_from(">I", pe_data, pe_off)[0]
                    rec_type = (record_val >> 24) & 0xFF
                    ord_val = record_val & 0xFFFF
                    if rec_type == 0:
                        lib_imports.append({
                            "ordinal": ord_val,
                            "iat_gva": record_addr,
                            "thunk_gva": 0
                        })
                    elif rec_type == 1:
                        if lib_imports:
                            lib_imports[-1]["thunk_gva"] = record_addr

            # Now resolve each import
            for imp in lib_imports:
                ord_val = imp["ordinal"]
                iat_gva = imp["iat_gva"]
                thunk_gva = imp["thunk_gva"]

                if ord_val in target_exports:
                    target_gva = target_exports[ord_val]
                    target_sym_raw = target_syms.get(target_gva, None)
                    
                    if target_sym_raw:
                        # It is an exported FUNCTION
                        is_func = True
                        if target_sym_raw.startswith("__imp__"):
                            target_sym = target_sym_raw[7:]
                        else:
                            target_sym = target_sym_raw

                        thunk_sym = None
                        if thunk_gva:
                            thunk_sym_raw = importing_syms.get(thunk_gva, None)
                            if thunk_sym_raw:
                                thunk_sym = thunk_sym_raw
                            else:
                                thunk_sym = f"__imp__sub_{thunk_gva:08X}"
                    else:
                        # It is an exported DATA VARIABLE (e.g. g_pMemAlloc in .data)
                        is_func = False
                        target_sym = None
                        thunk_sym = None

                    links.append({
                        "importing_mod": clean,
                        "target_mod": clean_target,
                        "iat_gva": iat_gva,
                        "thunk_gva": thunk_gva,
                        "target_gva": target_gva,
                        "target_sym": target_sym,
                        "thunk_sym": thunk_sym,
                        "is_func": is_func,
                        "ordinal": ord_val,
                    })

            lib_off += lib_size

    func_links = [l for l in links if l["is_func"]]
    var_links = [l for l in links if not l["is_func"]]
    print(f"  Total resolved: {len(links)} ({len(func_links)} functions, {len(var_links)} variables)")

    # 4. Generate C++ source and header files
    print("\n[4/5] Generating l4d_cross_module_links.h and l4d_cross_module_links.cpp...")
    header_path = os.path.join(OUTPUT_DIR, "l4d_cross_module_links.h")
    cpp_path = os.path.join(OUTPUT_DIR, "l4d_cross_module_links.cpp")

    unique_target_syms = sorted(list({link["target_sym"] for link in func_links}))

    thunk_links = [l for l in func_links if l["thunk_sym"]]
    seen_thunks = set()
    unique_thunk_links = []
    for l in thunk_links:
        if l["thunk_sym"] not in seen_thunks:
            seen_thunks.add(l["thunk_sym"])
            unique_thunk_links.append(l)

    with open(header_path, "w", encoding="utf-8") as f:
        f.write("""// l4d_cross_module_links.h
// Automatically generated by tools/generate_cross_module_links.py
// DO NOT EDIT MANUALLY!
#pragma once
#include <cstdint>

// Resolves all cross-module imports across all 21 Xbox 360 modules,
// populates guest memory IAT entries, and updates g_l4dGlobalDispatchTable.
void L4D_SetupCrossModuleLinks(uint8_t* guestBase);
""")

    with open(cpp_path, "w", encoding="utf-8") as f:
        f.write("""// l4d_cross_module_links.cpp
// Automatically generated by tools/generate_cross_module_links.py
// DO NOT EDIT MANUALLY!

#include "l4d_cross_module_links.h"
#include "l4d_unified_dispatch.h"
#include "l4d_pch.h"
#include <cstdio>
#include <cstring>

// ── Target Function Forward Declarations ────────────────────────────────────
""")
        for sym in unique_target_syms:
            f.write(f"PPC_EXTERN_FUNC({sym});\n")

        f.write("\n// ── Cross-Module Thunk Overrides (Direct Native Calls) ─────────────────────\n")
        for l in unique_thunk_links:
            thunk = l["thunk_sym"]
            target = l["target_sym"]
            f.write(f'// {l["importing_mod"]} -> {l["target_mod"]} ord={l["ordinal"]} (thunk=0x{l["thunk_gva"]:08X})\n')
            f.write(f'extern "C" void {thunk}(PPCContext& ctx, uint8_t* base) {{\n')
            f.write(f'    {target}(ctx, base);\n')
            f.write("}\n\n")

        f.write("""// ── Cross-Module Linker Setup ──────────────────────────────────────────────
struct CrossModuleLink {
    uint32_t iat_gva;
    uint32_t thunk_gva;
    uint32_t target_gva;
    PPCFunc* target_fn;
};

static const CrossModuleLink g_crossModuleLinks[] = {
""")
        for l in links:
            thunk_str = f"0x{l['thunk_gva']:08X}u" if l["thunk_gva"] else "0"
            fn_str = l["target_sym"] if l["is_func"] else "nullptr"
            f.write(f"    {{ 0x{l['iat_gva']:08X}u, {thunk_str}, 0x{l['target_gva']:08X}u, {fn_str} }},\n")

        f.write(f"""}};

static inline void WriteGuestU32(uint8_t* base, uint32_t gva, uint32_t val) {{
    val = __builtin_bswap32(val);
    memcpy(base + gva, &val, 4);
}}

void L4D_SetupCrossModuleLinks(uint8_t* guestBase) {{
    printf("[L4D Cross-Module Linker] Linking {len(links)} imports across game modules...\\n");
    size_t iatPatched = 0;
    size_t dispatchPatched = 0;

    for (const auto& link : g_crossModuleLinks) {{
        // 1. Patch IAT entry in guest memory
        if (link.iat_gva != 0) {{
            WriteGuestU32(guestBase, link.iat_gva, link.target_gva);
            iatPatched++;
        }}

        // 2. Patch thunk address in global dispatch table so indirect calls hit target
        if (link.thunk_gva != 0) {{
            if (link.thunk_gva >= L4D_GLOBAL_BASE && (link.thunk_gva - L4D_GLOBAL_BASE) < L4D_GLOBAL_SPAN) {{
                uint32_t idx = (link.thunk_gva - L4D_GLOBAL_BASE) >> 2;
                g_l4dGlobalDispatchTable[idx] = link.target_fn;
                dispatchPatched++;
            }}
        }}
    }}

    printf("[L4D Cross-Module Linker] Successfully linked %zu IAT entries, %zu dispatch thunks.\\n",
           iatPatched, dispatchPatched);
}}
""")

    print(f"[5/5] Done! Generated:")
    print(f"  {header_path}")
    print(f"  {cpp_path}")

    # Copy this script to tools/generate_cross_module_links.py
    dest = os.path.join(REPO_ROOT, "tools", "generate_cross_module_links.py")
    shutil.copy(__file__, dest)
    print(f"Copied generator to: {dest}")

if __name__ == "__main__":
    main()
