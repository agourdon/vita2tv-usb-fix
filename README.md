# Vita2TV USB Fix

**Vita2TV USB Fix addresses a USB freeze encountered when streaming a PS Vita's
screen to an external device.** Connecting or reconnecting USB can leave the
console unresponsive, with no video reaching the host. Unplugging the cable may
restore responsiveness, although some observed failures were followed by an
unexpected shutdown.

The plugin applies a small, guarded, in-memory correction to the Vita's USB
firmware driver. It targets a control-transfer ordering issue identified during
investigation, rather than masking failures with retries, timeouts or automatic
resets. It does not capture video or audio and is independent of Vita2TV Link.

Initial hardware tests are encouraging, including successful video/audio
streaming and repeated reconnections. **The fix remains experimental:** it has
only been tested against one identified firmware module on a Vita 2000 running
firmware 3.65, and does not claim to resolve every USB freeze.

## Compatibility

The supported target is the captured SceUdcd module from one Vita 2000 running
firmware 3.65: text size `0xb104`, data size `0x1760`, and the code/relocation
signatures checked by this repository. The displayed firmware version alone
does not establish compatibility. Unknown module layouts are rejected.

Other firmware revisions, other Vita models, and other USB video plugins have
not been validated. Compatibility with community USB video plugins needs a
separate hardware test; using the same Sony driver is not proof of compatibility.

## Repository layout

```text
src/       Kernel plugin, assembly trampoline, firmware guards and exports
tests/     Native regression suites, synthetic fixtures and ARM harness
docs/      Design, evidence and validation limits
release/   Precompiled experimental plugin and build provenance
build/     Local build outputs (ignored by Git)
```

## Precompiled plugin

Use [release/vita2tv_usb_fix.skprx](release/vita2tv_usb_fix.skprx) if you do
not want to compile the plugin. See [release/README.md](release/README.md)
for its build identity and validation status. This is an experimental candidate,
not a stable release. Follow the installation and verification instructions
below before connecting USB; the firmware compatibility restrictions still apply.

## Build from source

Install VitaSDK and taiHEN kernel/module utility stub libraries, and make their
tools available on `PATH`. GNU Make is required. For example, with VitaSDK
already installed:

```sh
export VITASDK=/path/to/vitasdk
export PATH="$VITASDK/bin:$PATH"
make all check
```

The required stub libraries are `taihenForKernel`, `taihenModuleUtils`,
`SceIntrmgrForDriver`, `SceThreadmgrForDriver`, `SceIofilemgrForDriver`, and
`SceSysclibForDriver`. The SDK installation must include their kernel headers
and libraries. No CMake project, network access, private dump, or main Vita2TV
checkout is needed to build this repository.

To refresh the precompiled artifact after a checked build:

```sh
make release
```

The output is `build/vita2tv_usb_fix.skprx`. The strict cross-build rejects
compiler warnings and checks for unresolved symbols and unintended UDCD
request, cancellation, FIFO, activation, or start/stop imports. Some VitaSDK
versions emit a linker warning about the `snprintf` stub's GNU-stack annotation.

Native tests require Clang or GCC with AddressSanitizer/UndefinedBehaviorSanitizer:

```sh
make test-native CC=clang
```

The optional exact ARM assembly test additionally requires VitaSDK and
`qemu-arm` from QEMU user-mode emulation:

```sh
make test-arm
```

No private firmware dump is required. See [validation](docs/validation.md) for
what these tests establish and what they do not.

## Install and verify

Disconnect USB physically from the Vita before installation and reboot. Keep
it disconnected until the boot report confirms successful installation.
Back up the active taiHEN configuration first.

Copy `release/vita2tv_usb_fix.skprx` (or your locally built
`build/vita2tv_usb_fix.skprx`) to `ur0:tai/vita2tv_usb_fix.skprx`, for example
using VitaShell FTP. Add its entry under
`*KERNEL` **before every plugin that starts or activates the Sony USB driver**:

```text
*KERNEL
ur0:tai/vita2tv_usb_fix.skprx
# Existing USB video/audio plugin entries follow here.
```

Load the correction synchronously during boot. Do not hot-load it, run an
earlier/concurrent USB activator, or install multiple copies. Reboot normally.
The installation is accepted only within the first ten seconds of boot, with
both relevant controllers stopped and both interrupt sources already disabled.
Suspending an interrupt is not treated as draining a live handler.

Read the new `ur0:data/vita2tv_usb_fix/boot-*.log` file. A successful report has
`guard=00000000`, `verified=1`, and `restore=00000000`. A rejection states
`installation_rejected; no patch attempted`. After an attempted installation
with `verified=0`, the original instruction was verified restored, but the
correction is not active. Do not mistake a successful boot for an active patch.

The report is written once at startup. There is no interrupt logger or
background diagnostic worker. Once installation completes, the only ongoing
work is a short assembly trampoline on the affected control-request path.

## Remove and recover

Delete the plugin entry from the configuration, then reboot. The modification
exists only in RAM; no Sony firmware file is changed. Never attempt to unload
the module while its trampoline can be reached. The module refuses hot unload.

If instruction rollback or interrupt restoration cannot be verified, startup
stops before later USB plugins run. Reboot while holding **L** to bypass plugins,
then remove the entry or restore your configuration backup. A kernel plugin
remains experimental and can cause a freeze or shutdown.

## Scope

This correction suppresses an early command only for nonstandard OUT requests
with a positive data length, after the original firmware request-ID selection.
It preserves the normal command following successful full receive completion.
It adds no timeout, retry, FIFO clear, reset, synthetic completion, media buffer,
or per-frame processing. It is not a general recovery mechanism for a stuck
USB driver or crashed host service.

See [design](docs/design.md) for the observed firmware decision and the limits
of the explanation. Cold starts, prolonged use with minimal instrumentation,
other devices/plugins, and the separate shutdown-after-unplug failure remain
part of the validation work.

## License

Original project source and documentation are provided under the [MIT license](LICENSE).
The repository contains minimal byte signatures used to reject incompatible
firmware, but no firmware image, private dump, raw user log, or community
project source. The license does not grant rights to third-party firmware.
