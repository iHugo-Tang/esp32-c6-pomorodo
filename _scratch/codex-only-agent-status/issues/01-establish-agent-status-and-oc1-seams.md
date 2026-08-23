# 01 — Establish Agent Status and OC1 firmware seams

**What to build:** Preserve the device's existing status behavior while introducing separate public seams for Agent Status behavior and the OC1 wire protocol, so domain code no longer needs to understand protocol text and the later product-wide rename can land safely.

**Blocked by:** None — can start immediately.

**Status:** complete

- [x] The Agent Status module owns enabled, connected, revision, mask, local acknowledgement, Completed display timeout, heartbeat timeout, and display-wake behavior using Agent Status vocabulary.
- [x] The Agent Status interface accepts parsed revision, mask, and wall time without accepting or producing OC1 text.
- [x] The OC1 adapter exclusively parses STATE input and formats ENABLED, DISABLED, and ACK output.
- [x] OC1 parsing continues to reject malformed, unknown-version, overlong, overflowing, or out-of-range input without affecting Agent Status.
- [x] Existing application behavior remains unchanged while callers still using the legacy entry point continue to compile during the expand phase.
- [x] Agent Status behavior tests exercise only the Agent Status public interface.
- [x] OC1 protocol tests exercise only the OC1 adapter public interface, including exact output formatting.
- [x] Existing application and native host tests continue to pass.

## Comments

- 2026-08-23: Implemented the Agent Status domain module and OC1 adapter. Native host tests (6/6), plugin checks (8/8), broker checks (5/5), and flash-reservation checks (10/10) pass. ESP-IDF build was not run because `idf.py` is unavailable in the current shell.
