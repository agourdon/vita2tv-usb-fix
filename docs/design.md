# EP0 phase-order correction

EP0 is the USB control endpoint used during enumeration and negotiation.
A control request contains a type/direction byte, a request ID, and a data
length. Request IDs are meaningful within their request type: a standard USB
request and a class request can legitimately use the same numeric ID.

The observed SceUdcd post-dispatch selection at text offset `0x7a8e` chooses
an early command for IDs `1`, `3`, `5`, `9`, and `11`. It does not qualify this
selection by request type, direction, or length. Those values correspond to
standard no-data OUT commands, but UVC `SET_CUR` also uses ID `1` and sends a
data payload, including the 34-byte PROBE/COMMIT structure in the observed case.

The early path writes command `1` to the controller at offset `+0x20c` after
dispatch, while the receive request can still be queued. The normal complete
receive path also writes command `1`, after the complete payload has arrived.
The precise hardware meaning of that command has not been established from
an official register specification. The working hypothesis is that the early
command permits an incorrect phase transition before the data receive completes.

Before the correction, a firmly connected trial entered the EP0 receive path
with 34 bytes expected, consumed only 26 bytes, and spent about 85.6 seconds
inside the interrupt handler. Removing USB allowed the handler to return.
Following the correction, the same 34-byte receives completed in approximately
50 microseconds in the instrumented observations. Those observations support
the hypothesis; they do not prove that this decision causes every reported
freeze or the subsequent shutdown that has sometimes occurred after unplug.

## Runtime change

The installer replaces the four-byte `LDR.W` at offset `0x7aa0` with a
non-linking Thumb-2 branch to `src/phase_checkpoint.S`. At this point the original
numeric request-ID filter has already matched.

The trampoline examines the cached request at controller state offsets
`+0x544` (type/direction) and `+0x54a` (length). For a nonstandard OUT request
with positive length it resumes at `0x77dc`, bypassing the early command.
All other requests execute the displaced RAM load once and resume at `0x7aa4`.
The normal successful receive path remains unchanged. Reserved request-type
encodings are included in the predicate tests, not advertised as valid USB
requests.

The trampoline preserves all registers except the original load's expected
effect on `r3`, and preserves stack, link register, and NZCVQ/GE flags. It makes
no C calls, does not read the FIFO, and performs no logging or scheduling.
The change concerns control transfers; it does not add a media copy or per-frame
processing step.

## Installation safety

The exact guarded windows, module segment sizes, relocated controller-state
anchors, original instruction, both continuation sites, and branch range must
match. Additional existing module signatures remain conservative identity
checks; the correction does not call their timestamp export.

The installation requires an early boot, stopped controllers, and interrupt
sources 151/155 already disabled. Previous interrupt states are restored in
reverse order. The injected instruction is read back. An unsuccessful injection
is not assumed harmless: the original bytes are restored when needed and read
back again. Uncertain code or interrupt restoration stops boot. After any
attempted injection, the module remains resident even when rollback succeeds.

These checks cannot detect every possible concurrent third-party actor.
Correct load order and physical USB disconnection are part of the installation
contract. Reboot removes the in-memory modification.
