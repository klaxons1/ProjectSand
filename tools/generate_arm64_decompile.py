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

# Convert the most important address-derived Hex-Rays globals into stable,
# role-based names. Keep this table explicit: each name is tied to observed use.
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
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(generated)
    print(f"Wrote {OUTPUT.relative_to(ROOT)} ({OUTPUT.stat().st_size} bytes; BSS {BSS_SIZE:#x})")


if __name__ == "__main__":
    main()
