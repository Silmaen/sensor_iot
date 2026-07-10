"""PlatformIO post-build hook: emit firmware.json next to firmware.bin.

Wired via `extra_scripts = post:tools/gen_manifest.py` on the device envs. The
manifest carries the image identity (from the env's build flags) plus the MD5
and size, so tools/publish_firmware.py can publish it to the update server
without re-deriving anything. See docs/ota-calibration-protocol.md §8.
"""

Import("env")  # noqa: F821  (injected by PlatformIO/SCons)

import hashlib
import json
import os

_PLATFORM_BY_BOARD = {
    "d1_mini": "ESP8266",
    "esp32-c3-devkitm-1": "ESP32-C3",
    "mkrwifi1010": "SAMD21",
}


def _defines():
    """Return the build's -D defines as a {name: value} dict."""
    out = {}
    for item in env.get("CPPDEFINES", []):  # noqa: F821
        if isinstance(item, (list, tuple)):
            out[str(item[0])] = item[1]
        else:
            out[str(item)] = None
    return out


def _clean(value):
    """Strip the quoting PlatformIO keeps around string defines (\\"E8BMEBAT\\")."""
    if value is None:
        return None
    # Remove any leading/trailing whitespace, backslashes and double quotes.
    return str(value).strip().strip('\\"')


def _write_manifest(source, target, env):  # noqa: F821
    defines = _defines()
    hw_rev = _clean(defines.get("HW_REV"))
    board = env.get("BOARD", "")  # noqa: F821

    bin_path = target[0].get_abspath()
    with open(bin_path, "rb") as fh:
        data = fh.read()

    manifest = {
        "hw_code": _clean(defines.get("HW_CODE")),
        "hw_rev": int(hw_rev) if hw_rev and hw_rev.isdigit() else hw_rev,
        "version": _clean(defines.get("FIRMWARE_VERSION")),
        "md5": hashlib.md5(data).hexdigest(),
        "size": len(data),
        "platform": _PLATFORM_BY_BOARD.get(board, env.get("PIOPLATFORM", "")),  # noqa: F821
        "modules": sorted(k[4:] for k in defines if k.startswith("HAS_")),
    }

    out_path = os.path.join(env.subst("$BUILD_DIR"), "firmware.json")  # noqa: F821
    with open(out_path, "w") as fh:
        json.dump(manifest, fh, indent=2)
    print("gen_manifest: wrote %s -> %s" % (out_path, manifest))


# Fire once the flashable .bin exists.
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", _write_manifest)  # noqa: F821
