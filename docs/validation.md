# Validation and limits

## Automated checks

`make test-native` runs three suites with address/undefined-behavior sanitizers:

- The scheduling policy covers all 256 request types, all 256 request IDs,
  and lengths 0, 1, 26, 34, and 65535: 327,680 combinations. It verifies that
  the correction never adds an early command and leaves independently observed
  successful-receive scheduling unchanged.
- Guard tests use a deterministic synthetic buffer containing only required
  signature fragments and generated relocation instructions. They exercise
  guarded-byte mutations, invalid sizes/anchors, branch displacement boundaries,
  and rejection of an already patched site. The buffer is not a firmware image
  and does not emulate USB hardware or real firmware execution.
- Installation tests inject interrupt suspension/restoration failures, enabled
  prior states, reverse-order restoration, and uncertain-state propagation.

`make test-arm` executes the production trampoline in QEMU ARM user mode for
3,840 cases: 256 request types, five lengths, and three flag patterns. The
harness verifies both continuations, all general registers, SP, LR, and flags.
The skipped path uses a null displaced-load base to detect an unwanted load.
QEMU does not emulate the USB controller or establish command semantics.

`make all check` builds the kernel module using VitaSDK and checks its imports.
The standalone component's rename and report-path extraction require a fresh
hardware boot verification before distributing a binary as tested.

An optional private guard validation accepts an exact previously captured,
relocated text image using `build/phase_guard_test /private/path/to/text.bin`.
That path assumes text base `0x01cd8000` and data base `0x01c7a000`; a dump from
another boot may correctly fail. Such images must not be committed or bundled.

## Hardware observations inherited from the candidate

The original isolated correction was tested on one Vita 2000 with the captured
3.65 SceUdcd module and a Raspberry Pi Zero 2 W. Instrumented tests completed
34-byte UVC receives, sustained approximately 60 video frames/s, and restarted
video following removal during streaming. Subsequent tests without the separate
IRQ diagnostic plugin displayed video and audio together and completed an
automatic audio/video reconnection followed by three additional cycles. The
reported audio counters showed no interruptions in those successful trials.

These are bounded successful trials, not proof of a universally corrected root
cause. Usual plugin/host logs remained enabled; “without the IRQ diagnostic
plugin” must not be described as a completely logging-free test. A host-driven
device reset occurred in one earlier reconnect; it was not a manual controller
reset. The observed host autosuspension behavior remains a separate consideration.

## Remaining work before stable release

- Boot-verify the extracted standalone binary and its report/module identity.
- Repeat cold-start and prolonged audio/video sessions with minimal logging.
- Exercise active unplug, short/long reconnect, and normal controls repeatedly.
- Validate compatibility and load order with other USB activators/video plugins.
- Identify the separate shutdown-after-unplug case if it recurs.
- Investigate host-service crash/recovery independently; this patch does not
  provide general reset or cancellation recovery.

Compatibility claims should name the exact tested hardware, firmware module,
USB plugin, and host combination. A displayed firmware version alone is not
sufficient evidence.
