# QVM arithmetic validation — 2026-09-17

The fifth step for issue #35 defines 32-bit wrapping integer NEG/ADD/SUB/MUL
using unsigned C arithmetic, so normal QVM overflow does not become native
undefined behavior. Signed right shift explicitly sign-extends. Shift counts
outside 0–31, integer division/modulo by zero, and signed INT_MIN/-1 division
or modulo raise a controlled interpreter fault before executing a native trap.
Float-to-int conversion rejects NaN, infinity, and values outside the signed
32-bit range; valid values still truncate toward zero.

## Validation

The actual-interpreter ASan/UBSan harness covers wrapping integer limits,
signed/unsigned quotient/remainder, division traps, negative/extreme shift
counts, both shift boundaries, negative arithmetic shifts, valid positive and
negative float truncation, the largest convertible positive float, INT_MIN,
NaNs, infinities, and values just outside the conversion range. The existing
stack, control-flow, memory, and recursive syscall fixtures continue to pass.
All four sanitizer runners and eight Python checks pass.

Both Retro68 products build without compiler warning/error diagnostics and
pass PEF validation with the temporary libraries described in the
[loading evidence](qvm-loading-validation.md).

| Product | PEF bytes | SHA-256 |
| --- | ---: | --- |
| Quake3 | 3,673,885 | `ab98f4928bd4aa84909c47c14be7b294aa01f60d410516d3f85523e7557ac2d1` |
| Quake3_TeamArena | 3,822,459 | `691b0c9b01270847d7ee8aa1a955e500ad9c49edff855d30cd2126ab9bf4a243` |

## Remaining acceptance

The [next step](qvm-call-validation.md) covers VM call arguments.
Keep #35 open: syscall-specific pointer/range checks remain. Retail baseq3/Team Arena and Mac OS 9 acceptance remains
deferred. The controlled errors cover invalid arithmetic; no complete sandbox
or target runtime compatibility claim is made.
