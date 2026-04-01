Guide me through adding a new optional hardware module (`HAS_xxx`) to the project.

Ask me for:
1. Module name (e.g. `dht22`, `bh1750`)
2. What it measures (metrics it will contribute)
3. Whether it handles any MQTT commands
4. Which PlatformIO environments should enable it

Then create/update all required files following the project conventions:

### Firmware
- `lib/thermo_core/src/modules/<name>_module.h` and `.cpp` — with `<name>_module_register(ModuleRegistry&)` and `<name>_module_contribute(PayloadBuilder&, ...)`
- `src/hw/<name>_hw.h` — hardware driver (with `#ifdef` for platform)
- `#ifdef HAS_<NAME>` blocks in `src/main.cpp` (include, register, contribute)

### Configuration
- Add `-DHAS_<NAME>` flag in the relevant `platformio.ini` environments
- Add `lib_deps` if a new library is needed

### Tests
- Add unit tests in `test/test_native/`

### Documentation
- Update `CLAUDE.md` flag table
- Update `docs/architecture.md`
- Create `docs/modules/<name>.md` with wiring and schematic SVG references
- Update `docs/configurations.md` module matrix
- Update `docs/components.md` if new hardware components are involved

Build and test after all changes: `pio run -e native && pio test -e native`.
