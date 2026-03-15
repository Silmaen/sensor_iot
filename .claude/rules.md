# Coding Rules

## Language
- All code, comments, commit messages, and documentation in English.
- Conversations with the user in French.

## Platform
- Target hardware: Wemos D1 Mini v3.0.0 (ESP8266).
- Two PlatformIO environments: `esp8266` (hardware) and `native` (debug/test).
- Always ensure code compiles on both environments.

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
