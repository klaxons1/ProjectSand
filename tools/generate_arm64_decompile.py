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
    # Exclude ELF process-startup glue and the bundled libc++abi runtime: the
    # Android NDK supplies those. Keep the application's simulation/JNI code.
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
    first_func = src.index("//----- (0000000000005880)")
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
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text("\n".join(out).rstrip() + "\n")
    print(f"Wrote {OUTPUT.relative_to(ROOT)} ({OUTPUT.stat().st_size} bytes; BSS {BSS_SIZE:#x})")


if __name__ == "__main__":
    main()
