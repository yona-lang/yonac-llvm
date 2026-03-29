#!/usr/bin/env python3
"""
Generate API reference docs from ## doc comments in .yona source files.

Reads lib/Std/*.yona, extracts ## comments and function/type definitions,
writes markdown files to docs/api/.

Usage:
    python3 scripts/gendocs.py
"""

import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LIB_DIR = ROOT / "lib" / "Std"
OUT_DIR = ROOT / "docs" / "api"


def parse_module(path: Path) -> dict:
    """Parse a .yona file, extracting doc comments and definitions."""
    lines = path.read_text().splitlines()
    module = {
        "name": "",
        "module_doc": [],
        "types": [],
        "functions": [],
        "exports": [],
    }

    i = 0
    # Collect module-level doc comments (before `module` keyword)
    while i < len(lines):
        line = lines[i]
        if line.startswith("##"):
            module["module_doc"].append(line[2:].strip())
        elif line.startswith("module "):
            module["name"] = line.split("module ", 1)[1].strip()
            i += 1
            break
        else:
            break
        i += 1

    # Collect exports and definitions
    doc_buffer = []
    while i < len(lines):
        line = lines[i]

        # Export lines
        if line.startswith("export type "):
            module["exports"].append(line)
        elif line.startswith("export "):
            module["exports"].append(line)

        # Doc comments
        elif line.startswith("##"):
            doc_buffer.append(line[2:].strip())

        # Type declaration
        elif line.startswith("type "):
            type_def = line.strip()
            module["types"].append({
                "definition": type_def,
                "doc": doc_buffer[:],
            })
            doc_buffer.clear()

        # Function definition (starts with identifier followed by params and =)
        elif re.match(r'^[a-z][a-zA-Z0-9_]*\s', line) and not line.startswith("export") and not line.startswith("module"):
            # Extract function name and signature
            match = re.match(r'^([a-z][a-zA-Z0-9_]*)\s*(.*?)$', line)
            if match:
                fname = match.group(1)
                rest = match.group(2)
                # Collect full definition (may span multiple lines)
                full_def = line.strip()
                module["functions"].append({
                    "name": fname,
                    "definition": full_def,
                    "doc": doc_buffer[:],
                })
                doc_buffer.clear()
        else:
            # Empty line or other — reset doc buffer only if truly empty
            if line.strip() == "" and not doc_buffer:
                pass
            elif not line.startswith("##") and not line.strip() == "":
                doc_buffer.clear()

        i += 1

    return module


def render_doc(doc_lines: list[str]) -> str:
    """Render doc comment lines to markdown."""
    result = []
    in_code = False
    for line in doc_lines:
        if line.startswith("```"):
            in_code = not in_code
            result.append(line)
        elif in_code:
            result.append(line)
        elif line == "":
            result.append("")
        else:
            result.append(line)
    return "\n".join(result)


def render_module(module: dict) -> str:
    """Render a parsed module to markdown."""
    out = []

    # Module header
    name = module["name"].replace("\\", ".")
    out.append(f"# {name}")
    out.append("")

    if module["module_doc"]:
        out.append(render_doc(module["module_doc"]))
        out.append("")

    # Types
    if module["types"]:
        out.append("## Types")
        out.append("")
        for t in module["types"]:
            out.append(f"### `{t['definition']}`")
            out.append("")
            if t["doc"]:
                out.append(render_doc(t["doc"]))
                out.append("")

    # Functions
    if module["functions"]:
        out.append("## Functions")
        out.append("")
        for f in module["functions"]:
            out.append(f"### `{f['name']}`")
            out.append("")
            out.append(f"```yona\n{f['definition']}\n```")
            out.append("")
            if f["doc"]:
                out.append(render_doc(f["doc"]))
                out.append("")

    return "\n".join(out)


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    modules = []
    for yona_file in sorted(LIB_DIR.glob("*.yona")):
        module = parse_module(yona_file)
        if not module["name"]:
            continue

        md = render_module(module)
        out_name = yona_file.stem + ".md"
        (OUT_DIR / out_name).write_text(md + "\n")
        modules.append(module)
        print(f"  {module['name']}: {len(module['functions'])} functions, {len(module['types'])} types")

    # Generate index
    index = ["# Yona Standard Library API Reference", ""]
    index.append(f"**{sum(len(m['functions']) for m in modules)} functions** across **{len(modules)} modules**")
    index.append("")
    index.append("| Module | Functions | Types | Description |")
    index.append("|--------|-----------|-------|-------------|")
    for m in modules:
        name = m["name"].replace("\\", ".")
        fname = m["name"].split("\\")[-1]
        desc = m["module_doc"][0] if m["module_doc"] else ""
        # Truncate desc at first period
        if ". " in desc:
            desc = desc[:desc.index(". ") + 1]
        index.append(f"| [{name}]({fname}.md) | {len(m['functions'])} | {len(m['types'])} | {desc} |")
    index.append("")
    (OUT_DIR / "README.md").write_text("\n".join(index) + "\n")

    print(f"\nGenerated {len(modules)} module docs in {OUT_DIR}/")


if __name__ == "__main__":
    main()
