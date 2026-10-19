#!/usr/bin/env python3
"""Normalize the ARM64 Hex-Rays export into a compilable-source candidate.

The original Hex-Rays listing is kept under docs/lib as an immutable reference.
This script rewrites writable `.data` and `.bss` aliases into one contiguous
memory arena at their original ELF virtual addresses. The decompiler indexes
through byte_33008 across the section boundary, so separate C++ globals would
not preserve the original address layout.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT / "docs/lib/arm64-v8a/libsand.so.c"
ELF = ROOT / "docs/lib/arm64-v8a/libsand.so"
OUTPUT = ROOT / "app/src/main/cpp/arm64/libsand_decompiled.cpp"
DATA_START = 0x33000
DATA_FILE_OFFSET = 0x23000
BSS_START = 0x33510
BSS_END = 0xE50EF8  # readelf -SW: .bss NOBITS size 0xE1D9E8
DATA_SIZE = BSS_START - DATA_START
BSS_SIZE = BSS_END - BSS_START
MEMORY_SIZE = BSS_END - DATA_START

# Names inferred from concrete function behavior in the frame scheduler and
# particle-list implementation, applied identically on every generation pass.
FUNCTION_NAMES = {
    "chain_update": "synchronize_particle_state_list",
    "chain_append": "append_particle_state_list",
    "calc_state_all": "reconcile_particle_states",
    "init_group": "initialize_material_group_lookup",
    "return_particle": "lookup_particle_at_cell",
}

# Convert address-derived Hex-Rays globals into stable role-based names.
# Keep this table explicit: each name is tied to an observed use.
SEMANTIC_NAMES = {
    "byte_33008": "g_shared_engine_data",
    "dword_3300C": "g_framebuffer_stride",
    "byte_33010": "g_engine_mode",
    "dword_33014": "g_current_particle_index",
    "byte_33018": "g_material_properties",
    "byte_334E4": "g_zoom_factor",
    "byte_334E5": "g_selected_material",
    "byte_334E6": "g_selected_material_state",
    "qword_334D8": "g_random_state",
    "_bss_start__": "g_view_width",
    "word_33512": "g_view_height",
    "word_33514": "g_background_color",
    "byte_33516": "g_framebuffer_bank_index",
    "qword_33518": "g_framebuffer_state_banks",
    "word_4C7380": "g_particle_records",
    "byte_4C7410": "g_particle_record_bytes",
    "byte_1C6A0": "g_material_properties_lut",
    "word_1C858": "g_direction_lut",
    "byte_1C960": "g_particle_state_lut",
    "byte_E50A18": "g_direction_index",
    "byte_E50A30": "g_search_direction",
    "byte_E50A31": "g_search_remaining",
    "byte_E50A32": "g_search_step",
    "word_E50A38": "g_found_x",
    "word_E50A3A": "g_found_y",
    "qword_E50A08": "g_framebuffer_pixels",
    "qword_E50A10": "g_cell_state_grid",
    "word_E50A72": "g_surface_height",
    "qword_E50A78": "g_jni_env",
    "qword_E50A80": "g_surface_global_ref",
    "qword_E50A88": "g_native_window",
    "word_E50A90": "g_surface_width",
    "byte_E50A92": "g_cell_state_allocated",
    "algn_E50AA4": "g_native_window_buffer_storage",
    "dword_E50AB0": "g_native_window_stride",
    "qword_E50AB8": "g_native_window_pixels",
    "qword_E50AE0": "g_engine_class_global_ref",
    "qword_E50AE8": "g_init_callback_method",
    "byte_E50B00": "g_command_queue_read_index",
    "byte_E50B01": "g_command_queue_write_index",
    "byte_E50B08": "g_command_queue",
}
assert DATA_SIZE == 0x510
assert 0x33518 + 600012 * 8 == 0x4C7378
assert 0x4C7410 + 9996544 == 0xE4FD10
assert BSS_SIZE == 0xE1D9E8
assert MEMORY_SIZE == 0xE1DEF8


def safe_id(s: str) -> str:
    return re.sub(r"[^A-Za-z0-9_]", "_", s)


def bss_array_size(name: str, old_size: str) -> str:
    if old_size:
        return old_size
    if name == "_bss_start__":
        return str(BSS_SIZE // 2)
    if name == "byte_33008":
        return "1"
    if name == "qword_33518":
        # The four particle banks use a 150003-slot stride; the final bank
        # reaches exactly to the next named global at 0x4C7374.
        return "600012"
    if name == "byte_E50CB0":
        # Bytes up to the next named global at 0xE50CCF.
        return "31"
    raise ValueError(f"Unbounded BSS array needs a verified size: {name}")


def main() -> None:
    src = INPUT.read_text()
    src = src.replace("#include <defs.h>", '#include "defs.h"')
    # The PLT resolver and ELF entrypoint are linker/runtime machinery, not
    # application engine code. The Android NDK supplies their responsibilities.
    src = re.sub(
        r"//----- \(0000000000005880\).*?(?=//----- \(0000000000005AC0\))",
        "",
        src,
        count=1,
        flags=re.S,
    )
    src = re.sub(
        r"//----- \(0000000000005AC0\).*?//----- \(0000000000005C4C\)",
        "// ELF entrypoint and private JNIEnv dispatch shim are supplied by Android.\n\n//----- (0000000000005C4C)",
        src,
        count=1,
        flags=re.S,
    )
    runtime_start = "//----- (0000000000016A24)"
    if runtime_start not in src:
        raise SystemExit("Could not locate the ARM64 bundled libc++abi tail")
    src = src[:src.index(runtime_start)]
    # Remove dead Hex-Rays prototypes for omitted PLT/libc++abi functions.
    src = re.sub(r"(?m)^.*\b(?:sub|nullsub)_[0-9A-Fa-f]+\([^;\n]*\);[^\n]*\n", "", src)
    src = re.sub(r"(?m)^.*\b(?:start|_gxx_personality_v0|_cxa_call_unexpected)\([^;\n]*\);[^\n]*\n", "", src)
    src = re.sub(r"(?m)^.*\boff_32[0-3][0-9A-Fa-f]+\b.*// weak.*\n", "", src)
    src = src.replace(
        "__int64 _JNIEnv::CallStaticVoidMethod(__int64 a1, __int64 a2, __int64 a3, ...);",
        "",
    )
    src = src.replace(
        "_JNIEnv::CallStaticVoidMethod(qword_E50A78, qword_E50AE0, qword_E50AE8);",
        "reinterpret_cast<JNIEnv *>(qword_E50A78)->CallStaticVoidMethod("
        "reinterpret_cast<jclass>(qword_E50AE0), reinterpret_cast<jmethodID>(qword_E50AE8));",
    )
    # JNI name-based lookup requires C linkage; Hex-Rays loses that source-level
    # property when it emits these exported native entry points.
    src = re.sub(
        r"(?m)^__int64 __fastcall (Java_smellymoo_sand_Engine_(?:init|command)\s*\()",
        r'extern "C" JNIEXPORT __int64 JNICALL \1',
        src,
    )
    src = src.replace("`typeinfo for'decltype(nullptr)", "typeinfo_decltype_nullptr")
    src = re.sub(r"`typeinfo name for'([A-Za-z0-9_:]+)", lambda m: "typeinfo_name_" + safe_id(m.group(1)), src)
    src = re.sub(r"`typeinfo for'([A-Za-z0-9_:]+)", lambda m: "typeinfo_" + safe_id(m.group(1)), src)
    src = re.sub(r"(?m)^.*typeinfo_.*// weak.*\n", "", src)
    def normalize_i128(match):
        value = int(match.group(1), 16)
        high, low = value >> 64, value & ((1 << 64) - 1)
        return f"hx_i128(0x{high:016X}ULL, 0x{low:016X}ULL)"
    src = re.sub(r"0x([0-9A-Fa-f]{17,32})LL\b", normalize_i128, src)
    src = src.replace("if ( _bss_start__ > 0 )", "if ( _bss_start__[0] > 0 )")
    # On first startup main_loop() locks the native window and stores its
    # non-null pixels pointer before calling set_frame(). The decompiled
    # mode-2 branch used that pointer to skip the one-time initialization,
    # leaving g_engine_mode stuck at 3 forever; main_loop then skips both the
    # command queue and simulation (it only runs those for modes > 4). Always
    # take the mode-2 initialization path, preserving the window pointer for
    # framebuffer setup below.
    stale_framebuffer_shortcut = "    if ( qword_E50A08 != 0 )\n      goto LABEL_4;\n"
    if src.count(stale_framebuffer_shortcut) != 1:
        raise SystemExit("Expected one stale-framebuffer shortcut in set_frame")
    src = src.replace(
        stale_framebuffer_shortcut,
        "    // Initialize a newly started/resumed engine even when the window\n"
        "    // already supplied a valid framebuffer pointer.\n",
    )
    bad_record_read = "(_WORD *)((char *)&dword_0 + (_QWORD)v3 + 2)"
    if src.count(bad_record_read) != 1:
        raise SystemExit("Expected one draw_particle base-relative record access")
    # v3 is already reconstructed as a native pointer into the shared arena;
    # adding it to the unrelated ELF-header dword_0 symbol forms a wild address.
    src = src.replace(bad_record_read, "(_WORD *)((char *)v3 + 2)")
    src = re.sub(r"\(unsigned __int8\)byte_33008(?!\[)", "(unsigned __int8)byte_33008[0]", src)
    src = re.sub(r"\bbyte_33008\s*(==|!=)", r"byte_33008[0] \1", src)
    src = src.replace("(_bss_start__ - 1)", "(_bss_start__[0] - 1)")
    # Preserve the low-register conversions Hex-Rays expresses as pointer-to-int
    # C-style casts, while making those conversions legal C++ on LP64.
    cast_type = r"(?:unsigned\s+__int(?:8|16|32|64)|unsigned\s+int|_DWORD|_WORD|_BYTE|__int(?:8|16|32|64)|int)"
    src = re.sub(
        r"\((" + cast_type + r")\s*\)\s*(result|v[0-9]+|a[0-9]+|[ijk])\b(\[[^\]\n]+\]|\+\+|--)?",
        r"hx_cast<\1>(\2\3)",
        src,
    )
    # Hex-Rays uses incompatible signedness guesses for pointers into this table.
    # Keep the ELF table's unsigned storage and cast only the four signed views.
    signed_v2_seen = False
    def signed_table_view(match):
        nonlocal signed_v2_seen
        if match.group(1) == "v2":
            if signed_v2_seen:
                return match.group(0)
            signed_v2_seen = True
        return f"{match.group(1)} = reinterpret_cast<__int16 *>(word_1C858 + ({match.group(2)}));"
    src = re.sub(
        r"\b(v2|v28|v211|v29)\s*=\s*&word_1C858\[([^\]]+)\];",
        signed_table_view,
        src,
    )
    src = src.replace(
        "v211 = &word_1C858[16 * (v161[6] & 7)];",
        "v211 = reinterpret_cast<__int16 *>(word_1C858 + (16 * (v161[6] & 7)));",
    )
    src = src.replace("return (_BYTE *)(&dword_0 + 1);", "return (char *)(&dword_0 + 1);")
    # Hex-Rays emits negative constants for unsigned lookup bytes/words. Preserve
    # their exact bit patterns while making C++ list initialization legal.
    first_func = src.index("//----- (0000000000005C4C)")
    prefix, rest = src[:first_func], src[first_func:]
    def unsigned_init(match):
        width = 8 if match.group(1) == "_BYTE" else 16
        modulus = 1 << width
        body = re.sub(r"-(\d+)", lambda n: str((-int(n.group(1))) % modulus), match.group(4))
        return match.group(1) + " " + match.group(2) + "[" + match.group(3) + "] = {" + body + "};"
    prefix = re.sub(r"(_BYTE|_WORD)\s+(\w+)\[([^\]]+)\]\s*=\s*\{(.*?)\};", unsigned_init, prefix, flags=re.S)
    # libc++abi teardown tables are supplied by the NDK, not engine state.
    prefix = re.sub(r"(?m)^.*\b(?:off_334F8|off_33500|qword_33508)\b.*// weak.*\n", "", prefix)
    src = prefix + rest
    if "#error" in src:
        raise SystemExit("Unrecovered Hex-Rays #error remains in the ARM64 input")

    data_image = bytearray(ELF.read_bytes()[DATA_FILE_OFFSET:DATA_FILE_OFFSET + DATA_SIZE])
    if len(data_image) != DATA_SIZE:
        raise SystemExit("Could not read the complete ELF .data image")
    # These .data relocations belonged to the omitted libc++abi teardown shim.
    for address in (0x334E8, 0x334F0, 0x334F8, 0x33500):
        offset = address - DATA_START
        if data_image[offset:offset + 8] != bytes(8):
            raise SystemExit(f"Unexpected nonzero relocation slot at {address:#x}")
    data_rows = [
        "  " + ", ".join(f"0x{byte:02X}" for byte in data_image[i:i + 16]) + ","
        for i in range(0, DATA_SIZE, 16)
    ]

    lines = src.splitlines()
    out = []
    injected = False
    bss_decl = re.compile(r"^(.*?)\s+([A-Za-z_]\w*)(?:\[([^\]]*)\])?(?:\s*=.*)?;\s*// weak\s*$")
    addr_name = re.compile(r"^(?:byte|word|dword|qword|xmmword|unk|algn|stru)_([0-9A-Fa-f]+)$")

    for line in lines:
        m = bss_decl.match(line)
        if m:
            typ, name, old_size = m.groups()
            addr = BSS_START if name == "_bss_start__" else None
            am = addr_name.match(name)
            if am:
                addr = int(am.group(1), 16)
            if addr is not None and DATA_START <= addr < BSS_END:
                if not injected:
                    out.append(f"alignas(16) static unsigned char g_native_memory[{MEMORY_SIZE:#x}] = {{}};")
                    out.append(f"static const unsigned char g_native_data_image[{DATA_SIZE:#x}] = {{")
                    out.extend(data_rows)
                    out.append("};")
                    out.append("__attribute__((constructor)) static void initialize_native_memory() {")
                    out.append("  std::memcpy(g_native_memory, g_native_data_image, sizeof(g_native_data_image));")
                    out.append("}")
                    out.append("#define unk_1C858 word_1C858")
                    out.append("#define unk_33018 byte_33018")
                    out.append("#define unk_E50B08 byte_E50B08")
                    injected = True
                offset = addr - DATA_START
                if name == "_bss_start__":
                    out.append(f"#define _bss_start__ reinterpret_cast<__int16 *>(g_native_memory + {offset:#x})")
                    continue
                suffix = ""
                if old_size is not None:
                    if name in {"byte_33008", "byte_33018", "algn_E50AA4"}:
                        # These are views into larger contiguous records/buffers;
                        # their Hex-Rays declarator lengths only describe the first field.
                        length = BSS_END - addr
                    elif name == "word_4C7380":
                        # Particle records are indexed at 10 shorts per particle.
                        length = (BSS_END - addr) // 2
                    else:
                        length = bss_array_size(name, old_size)
                    suffix = f" (*)[{length}]"
                    expr = f"(*reinterpret_cast<{typ}{suffix}>(g_native_memory + {offset:#x}))"
                else:
                    expr = f"(*reinterpret_cast<{typ} *>(g_native_memory + {offset:#x}))"
                out.append(f"#define {name} {expr}")
                continue
        out.append(line)

    if not injected:
        raise SystemExit("Could not locate the ARM64 BSS declarations")
    generated = "\n".join(out).rstrip() + "\n"
    for old_name, semantic_name in SEMANTIC_NAMES.items():
        generated = re.sub(r"\b" + re.escape(old_name) + r"\b", semantic_name, generated)

    # The byte global at 0x4C7410 is a shifted view into the same 20-byte
    # records as g_particle_records (0x4C7380). Recover record-relative field
    # accessors instead of preserving Hex-Rays' negative-base byte arithmetic.
    link_access = re.compile(
        r"\*\(_DWORD \*\)&g_particle_record_bytes\[20 \* (.*?) - 140\]"
    )
    generated, link_count = link_access.subn(r"particle_next_link(\1)", generated)
    if link_count != 6:
        raise SystemExit(f"Expected six particle-list link accesses, found {link_count}")
    byte_accessors = {
        "136": "particle_material_id",
        "135": "particle_flags",
        "133": "particle_status_flag",
        "132": "particle_packed_state",
        "128": "particle_counter",
    }
    byte_access = re.compile(
        r"g_particle_record_bytes\[20 \* (.*?) - (136|135|133|132|128)\]"
    )
    def particle_field_access(match):
        accessor = byte_accessors[match.group(2)]
        return f"{accessor}({match.group(1)})"
    generated, byte_count = byte_access.subn(particle_field_access, generated)
    if byte_count != 36:
        raise SystemExit(f"Expected 36 particle-byte field accesses, found {byte_count}")
    counter_pointer = (
        "result = (__int64)&g_particle_record_bytes[20 * dword_E50A00];\n"
        "      *(_BYTE *)(result - 128) = 0;"
    )
    if generated.count(counter_pointer) != 1:
        raise SystemExit("Expected one shifted-base particle-counter access")
    generated = generated.replace(
        counter_pointer,
        "result = (__int64)&particle_counter(dword_E50A00);\n"
        "      *(_BYTE *)result = 0;",
    )
    accessors = (
        "\nstatic inline _BYTE &particle_byte_field(__int64 index, unsigned offset) {\n"
        "  return reinterpret_cast<_BYTE *>(g_particle_records + 10LL * index)[offset];\n"
        "}\n"
        "static inline _DWORD &particle_next_link(__int64 index) {\n"
        "  return *reinterpret_cast<_DWORD *>(reinterpret_cast<_BYTE *>(g_particle_records + 10LL * index) + 4);\n"
        "}\n"
        "static inline _DWORD &particle_state_list_head(unsigned list_index) {\n"
        "  return *reinterpret_cast<_DWORD *>(g_native_memory + 0xe1da40 + 12 * list_index);\n"
        "}\n"
        "static inline _BYTE &particle_material_id(__int64 index) { return particle_byte_field(index, 8); }\n"
        "static inline _BYTE &particle_flags(__int64 index) { return particle_byte_field(index, 9); }\n"
        "static inline _BYTE &particle_status_flag(__int64 index) { return particle_byte_field(index, 11); }\n"
        "static inline _BYTE &particle_packed_state(__int64 index) { return particle_byte_field(index, 12); }\n"
        "static inline _BYTE &particle_counter(__int64 index) { return particle_byte_field(index, 16); }\n"
    )
    record_alias = re.search(r"(?m)^#define g_particle_record_bytes .*\n", generated)
    if not record_alias:
        raise SystemExit("Could not locate particle byte-view alias for accessors")
    generated = generated[:record_alias.end()] + accessors + generated[record_alias.end():]

    # Make the gas-particle update readable without changing its control flow:
    # distinguish RNG state, search results, record pointers, and grid addresses.
    start_marker = "//----- (000000000000FFF8)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    seek_gas = generated[start:end]
    for old_name, semantic_name in {
        "v0": "rng_first_pass",
        "v1": "rng_direction_parity",
        "v2": "rng_next_state",
        "v3": "rng_direction_sample",
        "v4": "scan_step",
        "v5": "destination_x",
        "v6": "destination_y",
        "v7": "grid_width",
        "v8": "particle_index",
        "v9": "source_particle_record",
        "v10": "destination_particle_record",
        "v11": "destination_cell_offset",
        "v12": "cell_state_grid_base",
        "v13": "current_particle_index",
        "v14": "found_x",
        "v15": "found_y",
        "v16": "current_particle_record",
        "v17": "destination_record_slot",
        "v18": "destination_cell",
    }.items():
        seek_gas = re.sub(r"\b" + old_name + r"\b", semantic_name, seek_gas)
    generated = generated[:start] + seek_gas + generated[end:]

    # Annotate the principal variables in the particle movement scheduler.
    # Names follow direct uses: record coordinates, direction deltas, density
    # checks, scan progress, and the grid slot being moved into.
    start_marker = "//----- (000000000000CDFC)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    seek_particle = generated[start:end]
    for old_name, semantic_name in {
        "v0": "active_particle_index",
        "v1": "direction_index",
        "v2": "direction_entry",
        "v4": "direction_delta_y",
        "v5": "particle_x",
        "v6": "direction_delta_x",
        "v7": "particle_y",
        "v8": "adjacent_x",
        "v9": "adjacent_y",
        "v10": "particle_density",
        "v11": "scan_counter",
        "v12": "lower_y_bound",
        "v13": "scan_x",
        "v14": "scan_distance",
        "v15": "scan_iteration",
        "v17": "working_particle_index",
        "v18": "working_index_copy",
        "v19": "working_particle_record",
        "v20": "particle_material_id_value",
        "v21": "slide_variant",
        "v22": "material_motion_type",
        "v23": "minimum_powder_progress",
        "v24": "liquid_move_succeeded",
        "v25": "powder_move_result",
        "v26": "transition_particle_record",
        "v27": "random_state_snapshot",
        "v28": "particle_motion_class",
        "v29": "state_particle_index",
        "v30": "next_particle_state",
        "v31": "target_particle_found",
        "v32": "retry_particle_record",
        "v33": "retry_particle_x",
        "v34": "retry_particle_y",
        "v35": "displacement_x",
        "v36": "displacement_y",
        "v37": "moving_particle_record",
        "v38": "moving_state_record",
        "v39": "packed_state_byte",
        "v40": "previous_x",
        "v41": "previous_y",
        "v42": "next_x",
        "v43": "next_y",
        "v44": "target_particle_found_again",
        "v45": "target_x",
        "v46": "target_y",
        "v47": "target_particle_index",
        "v48": "target_particle_record",
        "v49": "grid_width",
        "v50": "target_record_slot",
        "v51": "cell_grid_base",
        "v52": "target_cell",
        "v53": "direction_entry_offset",
        "v54": "saved_density_for_liquid",
        "v55": "saved_density_for_powder",
        "v56": "saved_density_for_retry",
        "v57": "saved_density_for_scan",
        "v58": "saved_density_for_fallback",
        "v59": "original_cell_material",
    }.items():
        seek_particle = re.sub(r"\b" + old_name + r"\b", semantic_name, seek_particle)
    generated = generated[:start] + seek_particle + generated[end:]

    # Recover the inputs and candidate-cell roles in the powder slide helper.
    start_marker = "//----- (000000000000C3C4)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    slide_powder = generated[start:end]
    for old_name, semantic_name in {
        "a1": "source_x",
        "a2": "source_y",
        "a3": "particle_index",
        "a4": "side_variant",
        "a5": "particle_density_threshold",
        "result": "slide_succeeded",
        "v6": "side_variant_value",
        "v7": "particle_index_value",
        "v8": "directional_table_base",
        "v9": "candidate_offset_x",
        "v10": "candidate_offset_y",
        "v11": "direction_vector",
        "v12": "gravity_delta_y",
        "v13": "preferred_delta_x",
        "v14": "preferred_x",
        "v15": "preferred_y",
        "v17": "fallback_x",
        "v18": "fallback_y",
        "v19": "fallback_cell_offset",
        "v20": "preferred_material_id",
    }.items():
        slide_powder = re.sub(r"\b" + old_name + r"\b", semantic_name, slide_powder)
    powder_table_base = (
        "  unsigned __int8 *directional_table_base; // x3\n",
        "  directional_table_base = &g_material_properties_lut[32 * (unsigned __int8)g_direction_index];",
    )
    for old_text in powder_table_base:
        if slide_powder.count(old_text) != 1:
            raise SystemExit("Expected one powder movement-table alias")
        slide_powder = slide_powder.replace(old_text, "")
    for old_text, new_text in {
        "*((_WORD *)directional_table_base + 234)": "g_direction_lut[16 * (unsigned __int8)g_direction_index + 14]",
        "*((_WORD *)directional_table_base + 235)": "g_direction_lut[16 * (unsigned __int8)g_direction_index + 15]",
        "*((_WORD *)directional_table_base + 232)": "g_direction_lut[16 * (unsigned __int8)g_direction_index + 12]",
        "*((_WORD *)directional_table_base + 233)": "g_direction_lut[16 * (unsigned __int8)g_direction_index + 13]",
    }.items():
        if slide_powder.count(old_text) != 1:
            raise SystemExit(f"Expected one powder direction vector access: {old_text}")
        slide_powder = slide_powder.replace(old_text, new_text)
    for state_value in (1, 2):
        old_text = (
            "LOBYTE(g_particle_records[10 * particle_index_value + 6]) = "
            f"g_particle_records[10 * particle_index_value + 6] & 0xF8 | {state_value};"
        )
        new_text = (
            "particle_packed_state(particle_index_value) = "
            f"particle_packed_state(particle_index_value) & 0xF8 | {state_value};"
        )
        if slide_powder.count(old_text) != 1:
            raise SystemExit(f"Expected powder packed-state write {state_value}")
        slide_powder = slide_powder.replace(old_text, new_text)
    generated = generated[:start] + slide_powder + generated[end:]

    # The liquid helper tries a side-dependent direction, then the primary
    # flow vector and (when necessary) the surface-pull fallback.
    start_marker = "//----- (000000000000C5EC)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    slide_liquid = generated[start:end]
    for old_name, semantic_name in {
        "a1": "source_x",
        "a2": "source_y",
        "a3": "particle_index",
        "a4": "side_variant",
        "a5": "particle_density_threshold",
        "result": "slide_succeeded",
        "v5": "particle_index_value",
        "v8": "side_variant_value",
        "v9": "particle_density_threshold_value",
        "v10": "initial_direction_entry",
        "v11": "source_x_value",
        "v12": "side_direction_delta_y",
        "v13": "source_y_value",
        "v14": "side_direction_x",
        "v15": "direction_index",
        "v16": "direction_vector",
        "v17": "flow_delta_x",
        "v18": "flow_delta_y",
        "v19": "directional_table_base",
        "v20": "candidate_offset_x",
        "v21": "candidate_offset_y",
        "v22": "selected_offset_x",
        "v23": "selected_offset_y",
        "v24": "preferred_delta_x",
        "v25": "preferred_x",
        "v26": "preferred_y",
        "v27": "preferred_material_id",
        "v29": "side_candidate_y",
        "v30": "lower_y_limit",
        "v31": "right_x_limit",
        "v32": "side_candidate_material_id",
        "v33": "primary_direction_entry",
        "v34": "primary_candidate_x",
        "v35": "primary_candidate_y",
        "v36": "fallback_x",
        "v37": "fallback_y",
        "v38": "fallback_material_id",
        "v39": "fallback_direction_entry",
    }.items():
        slide_liquid = re.sub(r"\b" + old_name + r"\b", semantic_name, slide_liquid)
    liquid_table_base = (
        "  unsigned __int8 *directional_table_base; // x5\n",
        "  directional_table_base = &g_material_properties_lut[32 * direction_index];",
    )
    for old_text in liquid_table_base:
        if slide_liquid.count(old_text) != 1:
            raise SystemExit("Expected one liquid movement-table alias")
        slide_liquid = slide_liquid.replace(old_text, "")
    for old_text, new_text in {
        "*((_WORD *)directional_table_base + 234)": "g_direction_lut[16 * direction_index + 14]",
        "*((_WORD *)directional_table_base + 235)": "g_direction_lut[16 * direction_index + 15]",
        "*((_WORD *)directional_table_base + 232)": "g_direction_lut[16 * direction_index + 12]",
        "*((_WORD *)directional_table_base + 233)": "g_direction_lut[16 * direction_index + 13]",
    }.items():
        if slide_liquid.count(old_text) != 1:
            raise SystemExit(f"Expected one liquid direction vector access: {old_text}")
        slide_liquid = slide_liquid.replace(old_text, new_text)
    for state_value in (1, 2):
        old_text = (
            "LOBYTE(g_particle_records[10 * particle_index_value + 6]) = "
            f"g_particle_records[10 * particle_index_value + 6] & 0xF8 | {state_value};"
        )
        new_text = (
            "particle_packed_state(particle_index_value) = "
            f"particle_packed_state(particle_index_value) & 0xF8 | {state_value};"
        )
        if slide_liquid.count(old_text) != 1:
            raise SystemExit(f"Expected liquid packed-state write {state_value}")
        slide_liquid = slide_liquid.replace(old_text, new_text)
    generated = generated[:start] + slide_liquid + generated[end:]

    # The Hex-Rays listing also indexed direction vectors by walking beyond
    # the 440-byte material-property table. The ELF places g_direction_lut
    # immediately after it; offsets 230/231 words in that alias are vector
    # components 10/11. Use the in-bounds direction-table view instead.
    start_marker = "//----- (0000000000007D30)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    can_sleep = generated[start:end]
    for old_text in (
        "  unsigned __int8 *v11; // x0\n",
        "  v11 = &g_material_properties_lut[32 * (unsigned __int8)g_direction_index];",
    ):
        if can_sleep.count(old_text) != 1:
            raise SystemExit("Expected one aliased direction-table base in can_sleep")
        can_sleep = can_sleep.replace(old_text, "")
    for old_text, new_text in {
        "*((_WORD *)v11 + 230)": "g_direction_lut[16 * (unsigned __int8)g_direction_index + 10]",
        "*((_WORD *)v11 + 231)": "g_direction_lut[16 * (unsigned __int8)g_direction_index + 11]",
    }.items():
        if can_sleep.count(old_text) != 1:
            raise SystemExit(f"Unexpected direction-table alias count in can_sleep: {old_text}")
        can_sleep = can_sleep.replace(old_text, new_text)
    generated = generated[:start] + can_sleep + generated[end:]

    start_marker = "//----- (000000000000E740)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    flame = generated[start:end]
    for old_text in (
        "  unsigned __int8 *v15; // x3\n",
        "  v15 = &g_material_properties_lut[32 * v0];",
    ):
        if flame.count(old_text) != 1:
            raise SystemExit("Expected one aliased direction-table base in flame")
        flame = flame.replace(old_text, "")
    for old_text, new_text in {
        "*((_WORD *)v15 + 230)": "g_direction_lut[16 * v0 + 10]",
        "*((_WORD *)v15 + 231)": "g_direction_lut[16 * v0 + 11]",
    }.items():
        if flame.count(old_text) != 1:
            raise SystemExit(f"Expected one direction-table alias in flame: {old_text}")
        flame = flame.replace(old_text, new_text)
    generated = generated[:start] + flame + generated[end:]

    # Name the key locals in the particle renderer instead of carrying the
    # decompiler's opaque vN labels through the repaired memory access.
    start_marker = "//----- (00000000000062B8)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    draw_particle = generated[start:end]
    for old_name, semantic_name in {
        "v0": "particle_index",
        "v3": "particle_record",
        "v6": "buffer_slot",
        "v8": "particle_y",
    }.items():
        draw_particle = re.sub(r"\b" + old_name + r"\b", semantic_name, draw_particle)
    generated = generated[:start] + draw_particle + generated[end:]

    # Name the interpolation, brush footprint, and wake coordinates in the
    # pointer-stroke renderer. This is a naming-only pass: arithmetic and
    # control flow remain the Hex-Rays translation of the ARM64 routine.
    start_marker = "//----- (0000000000014F98)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    draw_to = generated[start:end]
    for old_name, semantic_name in {
        "a1": "target_x",
        "a2": "target_y",
        "result": "x_step_offset",
        "v5": "x_delta",
        "v7": "abs_x_delta",
        "v8": "line_step_count",
        "v9": "line_step_count_float",
        "v10": "brush_radius",
        "v11": "brush_edge_x",
        "v12": "brush_edge_y",
        "v13": "x_increment_per_step",
        "v14": "y_increment_per_step",
        "v15": "interpolation_step",
        "v16": "y_step_offset",
        "v17": "step_index",
        "v18": "brush_far_y",
        "v19": "x_offset",
        "v20": "brush_column_x",
        "v21": "brush_row_y",
        "v22": "brush_step",
        "v23": "brush_radius_unsigned",
        "v24": "wake_x",
        "v25": "gravity_direction_entry",
        "v26": "gravity_direction_for_wake",
        "v27": "interpolated_x",
        "v28": "interpolated_y",
        "v29": "top_brush_step_index",
        "v30": "brush_edge_offset",
        "v31": "candidate_y",
        "v32": "candidate_x",
        "v33": "interior_brush_step_index",
        "v34": "side_candidate_y",
        "v35": "candidate_y_limit",
        "v36": "left_brush_step_index",
        "v37": "left_brush_offset",
        "v38": "left_brush_x",
        "v39": "brush_top_y",
        "v40": "brush_left_x",
    }.items():
        draw_to = re.sub(r"\b" + old_name + r"\b", semantic_name, draw_to)
    generated = generated[:start] + draw_to + generated[end:]

    # Name the brush material mapping and variant-selection state. This helper
    # is called for each sampled point by draw_to(), so preserve its lookup,
    # random-number, and placement logic exactly.
    start_marker = "//----- (000000000000E2E4)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    add_group_type = generated[start:end]
    for old_name, semantic_name in {
        "a1": "x",
        "a2": "y",
        "a3": "tool_id",
        "a4": "force_place",
        "v4": "x_position",
        "v5": "y_position",
        "v6": "bottom_boundary",
        "v7": "right_boundary",
        "v8": "paired_material_x",
        "v9": "paired_material_y",
        "v11": "material_group_entry",
        "v12": "base_material_id",
        "v13": "placement_pattern",
        "v14": "variant_count",
        "v15": "random_state_snapshot",
        "v16": "variant_offset",
        "v17": "material_id",
        "v18": "existing_material_id",
        "v19": "existing_material_flags",
        "v20": "random_state_value",
        "v21": "diagonal_pattern_match",
        "v22": "diagonal_distance",
        "v23": "grid_pattern_match",
    }.items():
        add_group_type = re.sub(r"\b" + old_name + r"\b", semantic_name, add_group_type)
    generated = generated[:start] + add_group_type + generated[end:]

    # Name the rectangular brush bounds, material mapping state, and perimeter
    # wake-up scan in draw_square(). The traversal order and all clipping tests
    # are intentionally left untouched.
    start_marker = "//----- (0000000000014768)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    draw_square = generated[start:end]
    for old_name, semantic_name in {
        "a1": "left_x",
        "a2": "top_y",
        "a3": "right_x",
        "a4": "bottom_y",
        "a5": "tool_id",
        "v5": "first_row_y",
        "v7": "last_row_y",
        "v8": "right_column_x",
        "v9": "selected_tool_id",
        "v10": "column_x",
        "v11": "material_group_entry",
        "v12": "paint_y",
        "v13": "bottom_inner_bound",
        "v14": "right_inner_bound",
        "v15": "saved_paint_y",
        "v16": "paired_material_x",
        "v17": "paired_material_y",
        "v19": "left_wake_bound_x",
        "v20": "right_wake_bound_x",
        "v21": "bottom_wake_bound_y",
        "v22": "wake_scan_start_x",
        "v23": "wake_scan_x",
        "v24": "placement_pattern",
        "v25": "material_id",
        "v26": "variant_count",
        "v27": "random_state_snapshot",
        "v28": "variant_offset",
        "v29": "diagonal_pattern_match",
        "v30": "diagonal_distance",
        "v31": "row_pattern_match",
        "v32": "wake_row_x",
        "v33": "next_paint_y",
        "v34": "wake_column_x",
        "v35": "wake_column_cursor_x",
    }.items():
        draw_square = re.sub(r"\b" + old_name + r"\b", semantic_name, draw_square)
    generated = generated[:start] + draw_square + generated[end:]

    # Clarify the small convenience wrappers that feed rectangles or a centered
    # brush dab into the shared rasterizer.
    for start_marker, parameter_names in (
        ("//----- (0000000000014F68)", {
            "a1": "left_x", "a2": "top_y", "a3": "right_x", "a4": "bottom_y",
        }),
        ("//----- (0000000000014F74)", {
            "a1": "center_x", "a2": "center_y",
        }),
    ):
        start = generated.index(start_marker)
        end = generated.index("//-----", start + len(start_marker))
        wrapper = generated[start:end]
        for old_name, semantic_name in parameter_names.items():
            wrapper = re.sub(r"\b" + old_name + r"\b", semantic_name, wrapper)
        generated = generated[:start] + wrapper + generated[end:]
    # Make the JNI command dispatcher readable in terms of packed arguments,
    # touch coordinates, material-table selection, and grid clearing.
    start_marker = "//----- (00000000000155A0)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    run_command = generated[start:end]
    for old_name, semantic_name in {
        "a1": "packed_command",
        "v1": "command_case_index",
        "v2": "command_parameter_1",
        "v3": "command_parameter_2",
        "v8": "scaled_touch_x",
        "v9": "touch_x",
        "v10": "touch_y",
        "v11": "material_group_entry",
        "v12": "base_material_id",
        "v13": "material_variant_count",
        "v14": "material_variant_offset",
        "v15": "cell_count",
        "v16": "cell_cursor",
        "v17": "cell_grid_end",
        "v18": "eraser_toggle_state",
    }.items():
        run_command = re.sub(r"\b" + old_name + r"\b", semantic_name, run_command)
    generated = generated[:start] + run_command + generated[end:]

    # Name the frame scheduler's command-queue indices, simulation passes,
    # particle-list cursors, pixel banks, and bounds-check temporaries.
    start_marker = "//----- (0000000000015BB0)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    main_loop = generated[start:end]
    for old_name, semantic_name in {
        "v0": "engine_mode_snapshot",
        "v2": "command_read_cursor",
        "v3": "command_write_snapshot",
        "v4": "command_slot_index",
        "v5": "mode_after_command_queue",
        "v6": "mode_phase",
        "v7": "particle_state_class",
        "v8": "processed_particle_count",
        "v9": "particle_state_list",
        "v10": "active_particle_index",
        "v11": "active_particle_record",
        "v12": "next_particle_index",
        "v13": "render_particle_index",
        "v14": "render_particle_record",
        "v15": "framebuffer_bank_index",
        "v16": "dirty_ring_read_index",
        "v17": "dirty_ring_write_offset",
        "v18": "dirty_ring_next_index",
        "v19": "dirty_ring_end_index",
        "v20": "dirty_ring_entry",
        "v21": "framebuffer_bank_entry",
        "v22": "framebuffer_bank_cursor",
        "v23": "framebuffer_bank_state",
        "v24": "grid_width",
        "v25": "grid_height",
        "v26": "grid_x",
        "v27": "background_pixel",
        "v28": "right_grid_bound",
        "v29": "cell_grid_base",
        "v30": "grid_y_cursor",
        "v31": "framebuffer_stride",
        "v32": "bottom_grid_bound",
        "v33": "grid_x_cursor",
        "v34": "grid_y",
        "v35": "transition_particle_record",
        "v36": "transition_material_id",
        "v37": "transition_period",
        "v38": "simulation_substep",
        "v39": "simulation_substep_limit",
        "v40": "field_center_x",
        "v41": "field_sample_spacing",
        "v42": "field_sample_width",
        "v43": "field_row_index",
        "v44": "field_column_count",
        "v45": "field_left_edge_x",
        "v46": "field_inner_left_x",
        "v47": "field_inner_right_x",
        "v48": "field_right_edge_x",
        "v49": "field_column_index",
        "v50": "field_random_state",
        "v51": "field_material_x_1",
        "v52": "field_material_x_2",
        "v53": "field_material_x_3",
        "v54": "simulation_particle_index",
        "v55": "particle_iteration_budget",
        "v56": "simulation_particle_record",
        "v57": "next_simulation_particle",
        "v58": "simulation_particle_state",
        "v59": "transition_random_state",
        "v60": "dirty_clear_cell_grid",
        "v61": "dirty_clear_framebuffer",
        "v62": "dirty_clear_grid_width",
        "v63": "dirty_clear_queue_index",
        "v64": "dirty_clear_stride",
        "v65": "dirty_clear_color",
        "v66": "dirty_clear_entry",
        "v67": "dirty_clear_x",
        "v68": "dirty_clear_particle_record",
        "v69": "previous_frame_timestamp",
        "v70": "elapsed_frame_ticks",
        "v71": "frame_timestamp_storage",
        "v72": "frame_timestamp_ticks",
    }.items():
        main_loop = re.sub(r"\b" + old_name + r"\b", semantic_name, main_loop)
    # The original ARM64 instruction loads dword +0x0C: the head of state
    # list 1. Hex-Rays labels this overlapping 12-byte list structure as
    # HIDWORD(xmmword_E50A40), which points at dword +0x04 (the wrong list).
    scheduler_list_cursor = "HIDWORD(xmmword_E50A40)"
    if main_loop.count(scheduler_list_cursor) != 2:
        raise SystemExit("Expected two scheduler reads of the state-list cursor")
    main_loop = main_loop.replace(scheduler_list_cursor, "particle_state_list_head(1)")
    generated = generated[:start] + main_loop + generated[end:]

    # Clarify the material placement path: target grid cell, incumbent material,
    # replacement checks, new-particle allocation, and state-list insertion.
    start_marker = "//----- (000000000000DA28)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    add_type = generated[start:end]
    for old_name, semantic_name in {
        "a1": "x",
        "a2": "y",
        "a3": "material_id",
        "a4": "force_place",
        "v4": "requested_material_id",
        "v5": "target_x",
        "v6": "target_y",
        "v7": "target_row_offset",
        "v8": "existing_material_id",
        "v9": "requested_material_properties_offset",
        "v11": "replacement_material_id",
        "v12": "existing_particle_index",
        "v13": "new_particle_record",
        "v14": "new_particle_packed_state",
        "v15": "framebuffer_bank_count",
        "v16": "framebuffer_pixel_offset",
        "v17": "background_pixel",
        "v18": "particle_state_group",
        "v19": "state_group_tail_index",
        "v20": "state_group_particle_record",
        "v21": "state_group_packed_state",
        "v22": "material_render_bank_count",
        "v23": "material_pixel",
        "v24": "material_pixel_offset",
    }.items():
        add_type = re.sub(r"\b" + old_name + r"\b", semantic_name, add_type)
    generated = generated[:start] + add_type + generated[end:]

    # Name the particle material conversion, grid write, and transition metadata.
    start_marker = "//----- (000000000000DE24)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    trans_particle = generated[start:end]
    for old_name, semantic_name in {
        "a1": "particle_index",
        "a2": "target_material_or_tool_id",
        "a3": "select_random_variant",
        "v3": "material_group_entry",
        "v4": "base_material_id",
        "v5": "variant_count",
        "v6": "variant_offset",
        "v7": "target_motion_class",
        "v8": "particle_index_value",
        "v9": "is_new_particle",
        "v10": "particle_record",
        "v11": "grid_width",
        "v13": "particle_flags",
        "v14": "particle_transition_record",
        "v15": "particle_packed_state",
        "v16": "particle_flags_byte",
        "v17": "transition_random_state",
        "v18": "transition_source_material_id",
        "v19": "next_transition_random_state",
    }.items():
        trans_particle = re.sub(r"\b" + old_name + r"\b", semantic_name, trans_particle)
    generated = generated[:start] + trans_particle + generated[end:]

    # Deobfuscate the particle displacement helpers. Keep the scan/hold/release
    # state manipulation intact while naming coordinates, candidate cells, and
    # temporarily saved pixel state.
    start_marker = "//----- (000000000000BF48)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    shift_line = generated[start:end]
    for old_name, semantic_name in {
        "a1": "source_x",
        "a2": "source_y",
        "a3": "line_delta_x",
        "a4": "line_delta_y",
        "v6": "destination_x",
        "v7": "destination_y",
        "v8": "found_x",
        "v9": "found_y",
        "v10": "current_particle_index",
        "v11": "current_particle_record",
        "v12": "grid_width",
        "v13": "destination_row_offset",
        "v14": "neighbor_material_matches",
        "v15": "released_x",
        "v16": "released_y",
        "v17": "destination_motion_class",
        "v18": "saved_particle_flags",
        "v19": "destination_particle_index",
        "v20": "saved_low_pixel_coordinate",
        "v21": "saved_high_pixel_coordinate",
        "v22": "saved_hold_flag",
        "v23": "saved_release_flag",
        "v24": "saved_current_particle_index",
        "v25": "saved_state_byte",
        "v26": "destination_cell_x",
        "v27": "destination_particle_value",
        "v28": "destination_particle_index_value",
        "v29": "collision_delta_x",
        "v30": "collision_delta_y",
        "v32": "destination_cell_y",
        "v33": "saved_pixel_owner",
        "v34": "saved_pixel_y",
        "v35": "saved_pixel_x",
        "v36": "saved_pixel_state",
    }.items():
        shift_line = re.sub(r"\b" + old_name + r"\b", semantic_name, shift_line)
    generated = generated[:start] + shift_line + generated[end:]

    start_marker = "//----- (000000000000C9B0)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    shift_drop = generated[start:end]
    for old_name, semantic_name in {
        "a1": "source_x",
        "a2": "source_y",
        "v2": "gravity_direction_vector",
        "v3": "destination_x",
        "v4": "destination_y",
        "v5": "found_x",
        "v6": "found_y",
        "v7": "current_particle_record",
        "v9": "saved_particle_state",
        "v10": "destination_row_offset",
        "v11": "interrupted_particle_index",
        "v12": "gravity_direction_entry",
        "v13": "candidate_x",
        "v14": "candidate_y",
        "v15": "candidate_motion_class",
        "v16": "saved_hold_state",
        "v17": "saved_release_state",
        "v18": "packed_destination_coordinates",
        "v19": "saved_current_pixel_x",
        "v20": "saved_current_pixel_y",
        "v21": "saved_current_particle_index",
        "v22": "saved_particle_state_byte",
        "v23": "saved_particle_flags",
        "v24": "saved_pixel_owner",
        "v25": "saved_pixel_y",
        "v26": "saved_pixel_x",
    }.items():
        shift_drop = re.sub(r"\b" + old_name + r"\b", semantic_name, shift_drop)
    generated = generated[:start] + shift_drop + generated[end:]

    # Describe particle initialization in terms of grid coordinates, material
    # properties, gravity-relative collision probes, and randomized state.
    start_marker = "//----- (0000000000009D24)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    init_particle = generated[start:end]
    for old_name, semantic_name in {
        "a1": "particle_index",
        "a2": "x",
        "a3": "y",
        "a4": "material_id",
        "v4": "particle_index_value",
        "v5": "material_properties_record",
        "v6": "particle_x",
        "v7": "particle_record",
        "v9": "material_has_special_init",
        "v10": "initial_packed_state",
        "v12": "initialized_packed_state",
        "v13": "material_motion_class",
        "v14": "gravity_direction_vector",
        "v15": "particle_density",
        "v16": "gravity_delta_x",
        "v17": "gravity_delta_y",
        "v18": "forward_cell_x",
        "v19": "grid_width",
        "v20": "current_particle_record",
        "v21": "existing_particle_flags",
        "v22": "cell_grid_base",
        "v23": "neighbor_existing_particle_record",
        "v25": "neighbor_particle_record",
        "v26": "neighbor_particle_flags",
        "v27": "random_state_snapshot",
        "v28": "initialized_particle_record",
        "v29": "initialized_particle_flags",
        "v30": "randomized_direction_step",
        "v31": "random_state_value",
        "v32": "next_random_state",
        "v33": "search_step_direction",
        "v34": "direction_random_state",
        "v35": "direction_next_random_state",
        "v36": "neighbor_delta_x",
        "v37": "particle_state_record",
        "v38": "source_particle_record",
        "v39": "particle_state_byte",
        "v40": "state_random_value",
        "v41": "final_particle_record",
        "v42": "found_delta_x",
        "v43": "neighbor_cell_offset",
        "v44": "occupied_particle_record",
        "v45": "occupied_particle_flags",
        "v46": "neighbor_state_byte",
        "v47": "neighbor_packed_state",
        "v48": "neighbor_delta_y",
        "v49": "neighbor_material_class",
        "v50": "initialization_flags",
        "v51": "initialization_grid_base",
        "v52": "particle_cell_offset",
        "v53": "existing_particle_class",
        "v54": "secondary_found_delta_x",
        "v55": "found_delta_y",
    }.items():
        init_particle = re.sub(r"\b" + old_name + r"\b", semantic_name, init_particle)
    generated = generated[:start] + init_particle + generated[end:]

    # Name the allocator's free-list/recycle decisions and the four dirty-pixel
    # ring-buffer cursors used when a particle slot is reclaimed.
    start_marker = "//----- (000000000000D5D0)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    new_particle = generated[start:end]
    for old_name, semantic_name in {
        "a1": "allow_recycle",
        "v1": "particle_index_result",
        "v3": "recycled_particle_index",
        "v4": "next_particle_link",
        "v5": "free_particle_record",
        "v6": "free_particle_state",
        "v7": "particle_list_head",
        "v8": "particle_list_next",
        "v9": "particle_list_record",
        "v10": "particle_list_state",
        "v11": "reclaimed_particle_index",
        "v12": "reclaimed_particle_index_value",
        "v13": "reclaimed_particle_record",
        "v14": "reclaimed_particle_state",
        "v15": "framebuffer_bank_count",
        "v16": "reclaimed_particle_x",
        "v17": "reclaimed_particle_y",
        "v18": "dirty_ring_end",
        "v19": "dirty_ring_next",
        "v20": "dirty_ring_entry",
        "v21": "secondary_dirty_ring_end",
        "v22": "secondary_dirty_ring_next",
        "v23": "secondary_dirty_ring_entry",
        "v24": "tertiary_dirty_ring_end",
        "v25": "tertiary_dirty_ring_next",
        "v26": "tertiary_dirty_ring_entry",
        "v27": "quaternary_dirty_ring_end",
        "v28": "quaternary_dirty_ring_next",
        "v29": "quaternary_dirty_ring_entry",
    }.items():
        new_particle = re.sub(r"\b" + old_name + r"\b", semantic_name, new_particle)
    generated = generated[:start] + new_particle + generated[end:]

    # Name the coordinate lookup, class filter, and optional resume path used
    # to resolve a particle handle from a simulation-grid cell.
    start_marker = "//----- (000000000000A744)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    particle_lookup = generated[start:end]
    for old_name, semantic_name in {
        "a1": "x",
        "a2": "y",
        "a3": "class_filter",
        "a4": "allow_resume",
        "v4": "cell_x",
        "v6": "cell_record",
        "v7": "material_motion_class",
        "v8": "particle_index",
    }.items():
        particle_lookup = re.sub(r"\b" + old_name + r"\b", semantic_name, particle_lookup)
    generated = generated[:start] + particle_lookup + generated[end:]

    # Clarify the neighbor-wake path used to reactivate particles around a
    # changing cell, without changing its sentinel or packed-state handling.
    start_marker = "//----- (000000000000B080)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    wake_particle = generated[start:end]
    for old_name, semantic_name in {
        "a2": "y",
        "v2": "particle_x",
        "v4": "cell_record",
        "v5": "particle_index",
        "v6": "packed_particle_state",
        "v7": "updated_particle_flags",
    }.items():
        wake_particle = re.sub(r"\b" + old_name + r"\b", semantic_name, wake_particle)
    generated = generated[:start] + wake_particle + generated[end:]

    start_marker = "//----- (000000000000B1A4)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    wake_neighbors = generated[start:end]
    for old_name, semantic_name in {
        "a1": "center_x",
        "a2": "center_y",
        "v2": "center_x_copy",
    }.items():
        wake_neighbors = re.sub(r"\b" + old_name + r"\b", semantic_name, wake_neighbors)
    generated = generated[:start] + wake_neighbors + generated[end:]

    # Name the coordinates, allocated particle, target list, and per-bank
    # dirty-pixel queue fields used when an asleep grid cell is resumed.
    start_marker = "//----- (000000000000A474)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    resume_particle = generated[start:end]
    for old_name, semantic_name in {
        "a1": "x",
        "a2": "y",
        "v2": "particle_x",
        "v3": "particle_y",
        "v4": "allocated_particle_index",
        "v5": "particle_index",
        "v6": "new_particle_record",
        "v7": "state_list_index",
        "v8": "state_list_tag",
        "v9": "state_list_head",
        "v10": "particle_record",
        "v11": "framebuffer_bank_count",
        "v12": "packed_state_byte",
        "v13": "primary_dirty_ring_end",
        "v14": "primary_dirty_ring_next",
        "v15": "primary_dirty_ring_entry",
        "v16": "secondary_dirty_ring_end",
        "v17": "secondary_dirty_ring_next",
        "v18": "secondary_dirty_ring_entry",
        "v19": "tertiary_dirty_ring_end",
        "v20": "tertiary_dirty_ring_next",
        "v21": "tertiary_dirty_ring_entry",
        "v22": "quaternary_dirty_ring_end",
        "v23": "quaternary_dirty_ring_next",
        "v24": "quaternary_dirty_ring_entry",
    }.items():
        resume_particle = re.sub(r"\b" + old_name + r"\b", semantic_name, resume_particle)
    generated = generated[:start] + resume_particle + generated[end:]

    # Make the per-particle list reconciliation readable in terms of the
    # encoded source/destination list and the particle's current record.
    start_marker = "//----- (0000000000008610)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    list_sync = generated[start:end]
    for old_name, semantic_name in {
        "v0": "current_particle_index",
        "v2": "current_particle_record",
        "v3": "packed_particle_state",
        "v4": "material_state_id",
        "v5": "source_list_index",
        "v6": "destination_list_index",
        "v7": "destination_list_offset",
        "v8": "destination_list_head",
        "v9": "destination_packed_state",
        "v10": "source_list_header",
        "v11": "source_list_tail",
        "v12": "source_particle_record",
        "v13": "source_packed_state",
        "v14": "previous_particle_index",
    }.items():
        list_sync = re.sub(r"\b" + old_name + r"\b", semantic_name, list_sync)
    generated = generated[:start] + list_sync + generated[end:]

    # Clarify the list-splice operation: it appends one complete particle
    # state list to another, optionally rewriting each particle's list tag.
    start_marker = "//----- (00000000000082A0)"
    start = generated.index(start_marker)
    end = generated.index("//-----", start + len(start_marker))
    append_list = generated[start:end]
    for old_name, semantic_name in {
        "a1": "source_list_index",
        "a2": "destination_list_index",
        "a3": "update_particle_list_tags",
        "v3": "destination_list_index_copy",
        "v4": "source_list_header_for_append",
        "v5": "source_list_tail",
        "i": "particle_index",
        "v7": "particle_record",
        "v8": "source_list_state",
        "v9": "destination_list_state",
        "v10": "destination_list_count",
        "v11": "source_list_count",
        "v13": "source_list_header_for_empty_destination",
    }.items():
        append_list = re.sub(r"\b" + old_name + r"\b", semantic_name, append_list)
    generated = generated[:start] + append_list + generated[end:]

    # Resolve the list-management and material-table helper names from their
    # observed roles. Keep this as a final symbol-only pass so prototypes,
    # definitions, and every call site stay in sync.
    for old_name, semantic_name in FUNCTION_NAMES.items():
        if len(re.findall(r"\b" + re.escape(old_name) + r"\b", generated)) < 2:
            raise SystemExit(f"Expected declaration/definition for {old_name}")
        generated = re.sub(r"\b" + re.escape(old_name) + r"\b", semantic_name, generated)

    append_list_prototype = (
        "__int64 __fastcall append_particle_state_list("
        "unsigned __int8 a1, unsigned __int8 a2, char a3);"
    )
    if generated.count(append_list_prototype) != 1:
        raise SystemExit("Expected one append_particle_state_list prototype")
    generated = generated.replace(
        append_list_prototype,
        "__int64 __fastcall append_particle_state_list("
        "unsigned __int8 source_list_index, "
        "unsigned __int8 destination_list_index, "
        "char update_particle_list_tags);",
    )
    particle_lookup_prototype = (
        "bool __fastcall lookup_particle_at_cell(__int16 a1, __int16 a2, "
        "char a3, char a4);"
    )
    if generated.count(particle_lookup_prototype) != 1:
        raise SystemExit("Expected one lookup_particle_at_cell prototype")
    generated = generated.replace(
        particle_lookup_prototype,
        "bool __fastcall lookup_particle_at_cell(__int16 x, __int16 y, "
        "char class_filter, char allow_resume);",
    )

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(generated)
    print(f"Wrote {OUTPUT.relative_to(ROOT)} ({OUTPUT.stat().st_size} bytes; BSS {BSS_SIZE:#x})")


if __name__ == "__main__":
    main()
