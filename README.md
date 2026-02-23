# CLinker — Static Linker in C++

A simplified static linker implemented in C++ that reads custom binary object files, resolves cross-function symbol references, and produces a single flat binary output — mimicking the core linking step of a real toolchain.

## What It Does

Given one or more `.o` object files in a custom binary format, `CLinker`:

1. **Parses** each file's header (export count, import count, total byte size)
2. **Registers exported symbols** — maps each function name to its raw machine-code definition
3. **Resolves imports** — records, for each exported function, which other symbols it calls and at what byte offset
4. **Links** — starting from a specified entry-point function, performs a DFS to collect all transitively used functions, concatenates their raw code, then back-patches every import address with the correct absolute position in the output binary
5. **Writes** the result to a flat binary output file

The entry point is always placed first at offset `0`; all transitively reachable functions follow in order.

## Object File Format

Each `.o` file is a little-endian binary with the following layout:

```
Header:
  [4 bytes] export count
  [4 bytes] import count
  [4 bytes] total code bytes

Exports (repeated export_count times):
  [1 byte]  name length
  [N bytes] function name
  [4 bytes] byte offset of function's code within this file's code section

Imports (repeated import_count times):
  [1 byte]  name length
  [N bytes] symbol name being imported
  [4 bytes] usage count
  (repeated usage_count times):
    [4 bytes] absolute byte address within the code section where this symbol is referenced

Code section:
  raw bytes for all exported functions, laid out contiguously
```

Import addresses are 4-byte little-endian slots; the linker overwrites them with the final absolute offset of the target function in the output file.

## API

```cpp
CLinker& addFile(const std::string& fileName);
void     linkOutput(const std::string& fileName, const std::string& entryPoint);
```

`addFile` is chainable. `linkOutput` triggers the link and writes the binary. Both throw `std::runtime_error` on any error (missing file, duplicate symbol, missing definition, write failure, etc.).

## Test Data

`data/` contains two sets of object files:

| Set | Files | Description |
|---|---|---|
| Basic (0–7) | `0in0.o` … `7in0.o` | Single- and multi-file link scenarios, plus error cases (duplicate symbol, missing file, missing entry point) |
| Extended (0010–0014) | `0010_0.o` … `0014_2.o` | Multi-file links with non-trivial entry-point names; reference outputs provided for byte-exact comparison |

## Key Implementation Details

- **DFS symbol walk** (`findUsed`) collects the transitive closure of all reachable functions before any output is written, ensuring only live code is included
- **Import address back-patching** (`writeAddresses`) writes 4-byte little-endian addresses directly into the output buffer after all final positions are known
- **Relative → absolute address conversion**: imports are stored relative to the start of their containing function; the linker converts them to absolute positions in the output file at link time
- No dynamic memory beyond `std::string` / `std::map` — the entire linked binary is assembled in a single `std::string` before being written out in one `ofs.write` call

## Requirements

- C++20 (uses `std::map::contains`)
- Standard library only — no external dependencies
