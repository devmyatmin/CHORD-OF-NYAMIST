#!/usr/bin/env python3
"""Convert compiled SPIR-V binaries into a deterministic C++ header."""

from __future__ import annotations

import argparse
import pathlib
import struct
import sys


def read_words(path: pathlib.Path) -> list[int]:
    data = path.read_bytes()
    if not data or len(data) % 4:
        raise ValueError(f"{path}: SPIR-V byte length must be non-zero and divisible by 4")
    words = list(struct.unpack(f"<{len(data) // 4}I", data))
    if words[0] != 0x07230203:
        raise ValueError(f"{path}: invalid SPIR-V magic number")
    return words


def render_array(name: str, words: list[int]) -> str:
    lines = [f"inline constexpr std::uint32_t {name}[] = {{"]
    for offset in range(0, len(words), 8):
        chunk = words[offset : offset + 8]
        lines.append("    " + ", ".join(f"0x{word:08x}U" for word in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--vertex", required=True, type=pathlib.Path)
    parser.add_argument("--fragment", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()

    try:
        vertex_words = read_words(args.vertex)
        fragment_words = read_words(args.fragment)
        content = (
            "#pragma once\n"
            "#include <cstdint>\n"
            "namespace nyamist::shaders {\n"
            + render_array("triangle_vertex_spirv", vertex_words)
            + "\n"
            + render_array("triangle_fragment_spirv", fragment_words)
            + "\n} // namespace nyamist::shaders\n"
        )
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(content, encoding="utf-8", newline="\n")
    except (OSError, ValueError, struct.error) as error:
        print(f"SPIR-V embedding failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
