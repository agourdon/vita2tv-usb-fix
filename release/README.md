# Precompiled experimental plugin

`vita2tv_usb_fix.skprx` is a ready-to-install build of the standalone component.
Installation, removal, recovery and supported firmware checks are documented
in the [main README](../README.md#install-and-verify).

This is **not a stable release**. Native sanitized tests, the exact ARM
trampoline harness and VitaSDK compilation/import checks pass. Hardware trials
of the preceding isolated candidate are documented in
[validation](../docs/validation.md); this standalone binary still requires its
own hardware boot verification. Other USB plugins and firmware revisions are
not yet validated.

## Build provenance

- Compiler: VitaSDK `arm-vita-eabi-gcc` 15.2.0, Cortex-A9 Thumb, `-O2`.
- Packaging: `vita-elf-create` and `vita-make-fself -c`.
- Rebuild: `make release` from the repository root with VitaSDK on `PATH`.

The SHA-256 below identifies this exact packaged file; a different SDK/toolchain
may produce different bytes without changing the source behavior.

```text
2e4c9bee83b6e031da83ec13c8e96335ddd2e86b33491d4a84c3c423170efe1d  vita2tv_usb_fix.skprx
```

After refreshing the binary, update its checksum and validation status here
before distributing it. Do not bundle firmware dumps or private logs.
