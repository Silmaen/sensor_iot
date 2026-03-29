# Coding Rules

## Language
- All code, comments, commit messages, and documentation in English.
- Conversations with the user in French.

## Platform
- Target hardware: Wemos D1 Mini v3.0.0 (ESP8266) and Arduino MKR WiFi 1010 (SAMD21).
- PlatformIO environments: `thermo_display`, `thermo_1`, `thermo_sht30`, `thermo_mkr`, `cell_tester`, `native`.
- Always ensure code compiles on both ESP8266 and native environments.
- `HAS_BME280` and `HAS_SHT30` are mutually exclusive.

## Code Style
- C++17 for native builds; C++11-compatible for ESP8266.
- Use `#ifndef NATIVE` / `#ifdef NATIVE` to guard platform-specific code.
- Use `#ifdef ESP8266` only when ESP8266-specific APIs are needed.
- Build with `-Wall -Wextra`, fix all warnings before committing.
- Prefer `constexpr` and `const` over `#define` for constants.
- Use `snake_case` for functions and variables, `PascalCase` for classes/structs, `UPPER_SNAKE_CASE` for compile-time constants.

## Architecture
- Keep business logic platform-independent in `src/` and `include/`.
- Hardware abstraction: isolate hardware access behind interfaces so native builds can mock them.
- Avoid dynamic memory allocation (`new`, `malloc`) on the ESP8266 where possible.
- Prefer stack allocation and fixed-size buffers.

## Components
- Consult [COMPONENTS.md](../COMPONENTS.md) for available electronic components.
- Prioritize components from that list when proposing hardware designs or library choices.

## Testing
- Unit tests go in `test/test_native/` using the Unity framework.
- Run `pio test -e native` to validate changes.
- Write tests for all non-trivial business logic.

## Dependencies
- Prefer well-maintained PlatformIO libraries.
- Declare dependencies in `platformio.ini` using `lib_deps`.
- Avoid adding libraries for trivial functionality.

## Documentation Structure
- **`CLAUDE.md`**: Claude-facing project overview. Keep lean, ASCII diagrams are fine here (efficient for LLM context).
- **`docs/modules/*.md`**: User-facing per-module documentation. One file per optional module (`HAS_xxx`).
- **`docs/img/*.svg`**: SVG diagrams referenced from module docs. Two types per module: wiring (realistic components) and schematic (standard electronic symbols).
- **`COMPONENTS.md`**: Hardware inventory and device configurations. ASCII diagrams kept for Claude, SVG images added for users.

## SVG Diagrams
- All SVGs must support light and dark themes via `@media (prefers-color-scheme: dark/light)` CSS.
- First element after `<defs>` must be a background `<rect>` with class `bg`.
- Use class-based styling: `.text`, `.text-dim`, `.wire-pwr`, `.wire-gnd`, `.wire-sig`.
- Monospace font. No overlapping of wires or text.
- When adding a new hardware module, create both wiring and schematic SVGs.

## Adding a New Module
1. Create firmware: driver in `src/hw/`, module in `lib/thermo_core/src/modules/`, tests in `test/test_native/`.
2. Integrate: `#ifdef HAS_xxx` blocks in `main.cpp`, flag in `platformio.ini`, environment.
3. Document: `docs/modules/<name>.md` with datasheet link, wiring SVG, schematic SVG.
4. Update: `CLAUDE.md` flag table, `docs/architecture.md` file reference, `COMPONENTS.md` component list.
