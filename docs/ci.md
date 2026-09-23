# Continuous integration

`Portable CI` is the first automated signal layer for review and GasCity
workers. It runs on master pushes, pull requests, and manual dispatch without a
Retro68 installation, proprietary retail data, or a Mac OS 9 emulator.

Feature branch updates run through the pull-request trigger. Filtering the
push trigger to master avoids duplicate copies of the same jobs for each PR
head while retaining checks on the merged default branch. A newer PR head
cancels the older run. Push runs use one concurrency group per commit, so
merging several PRs in a row never cancels or drops a master run.

## Required checks

- `Scripts and review ledger`
  - parses every tracked Bash script individually (`tests/check_shell_syntax.sh`) and the PowerShell entry points;
  - runs both build-script help paths;
  - compiles the Python utilities;
  - requires `docs/task.md` to remain in P0-to-P3 order with exactly one direct
    link for every confirmed issue #1 through #52, allowing both open and
    completed checkboxes as the queue is worked down;
  - checks portable image decoders are included in Unix Make/Cons, Visual
    Studio, Xcode source phases and lint manifests, including the sole standard JPEG compressor APIs.
- `Packaging tools (Python 3.11)` and `(Python 3.14)`
  - validate AppleDouble entry offsets, resource data, Finder type/creator,
    and bundle flag;
  - validate MacBinary name bytes, fork lengths/padding, deterministic Mac
    dates, version fields, and CRC;
  - reject filenames that cannot be represented in MacRoman.
- `Host C regressions (ASan/UBSan)`
  - summarizes eight parallel jobs, `Host C regressions (gcc|clang K/4)`. Each
    runs one quarter of the sorted `tests/run_*_tests.sh` runners through
    `tests/run_host_regressions.sh`, which discovers runners with
    `git ls-files`, runs them in parallel, prints each result as it finishes,
    bounds each runner at 10 minutes and the shard at 20 minutes (naming any
    runner that did not finish), and prints the full log of any runner that
    fails. Every runner runs under
    both GCC and Clang. A new runner needs no workflow edit;
  - exercises actual world traversal with stock visibility/frustum/draw goldens,
    deep pending-state cleanup and all 32 dynamic-light mask bits;
  - exercises actual swept BSP traces with stock point/box/capsule clipping
    goldens, deep traversal/pruning and injected temporary-allocation failure;
  - builds a deliberately isolated portion of `q_shared.c`;
  - checks bounded extension stripping, formatting, and token termination
    under AddressSanitizer and UndefinedBehaviorSanitizer;
  - exercises the real QVM create/restart loaders with exact-sized files,
    every truncation of a small image, invalid signed ranges/counts, data-size
    overflow, larger/smaller replacement data images, and same-sized changes
    to code, counts, layout, BSS, or initial data (issue #35);
  - checks recoverable rejection, file-buffer ownership, no hunk allocation
    before validation, unchanged data on rejected restart, and valid data
    initialization/restart, plus cleanup after failed bytecode preparation;
  - exercises actual bytecode preparation with truncated operands, invalid
    opcodes/counts/branch targets, unaligned immediates, and valid translation.
  - executes synthetic QVMs through the actual interpreter to check operand
    and program stacks, CALL/JUMP targets, forged return addresses, recursive
    syscalls, faulted shutdown re-entry, and valid execution;
  - checks full-width loads, legacy masked stores, ARG boundaries, block-copy
    bounds/overlap, and bounded syscall argument snapshots;
  - checks wrapping integer arithmetic and the defined division/modulo by
    zero, oversized shift count, and float conversion results;
  - checks native/compiled/interpreted VM dispatch with empty, partial, and
    twelve-parameter calls, zero padding, single argument evaluation, and
    invalid counts, and faulted VM re-entry;
  - checks common MEMSET/MEMCPY/STRNCPY trap ranges, overlap, null/negative/
    oversized buffers, terminating sources at image boundaries, and VM
    destination return values;
  - checks typed syscall buffer alignment, bounded strings, array-size
    arithmetic, nullable query/reset arguments, and nonempty string outputs;
  - exercises the real UI CD-key and parser filename outputs with exact-sized
    allocations;
  - exercises the real cgame polygon-batch and fragment filters to check
    dimension multiplication, complete array ranges, empty submissions, and
    rejection before renderer callbacks;
  - exercises actual server game-data registration and persistent access with
    private strides, saved client capacities, controlled index/slot rejection,
    and unchanged registration on failure;
  - checks native debug-polygon point limits, invalid handles, and zero-point
    line reservation;
  - checks returned connection-denial strings in the owning VM with a
    different active VM, boundary termination, aliases, and optional NULL.
  - checks actual botlib common/navigation dispatch with bounded strings,
    structs, complete/optional arrays, zero-capacity outputs, NULL map/entity
    operations, and client indices bounded by actual server allocation.
  - checks the actual bot chat dispatcher with complete output structures,
    nullable variables and their combined capacity, bounded match metadata,
    native synonym expansion/search bounds, and overlapping substring output.
  - checks actual bot action dispatch into native elementary-action routines,
    whole input output, bounded strings/vectors, separate server/bot capacities,
    invalid native client indices, allocation arithmetic, and shutdown reset.
  - checks remaining AI goal/move/weapon structures, retail inventory arrays,
    bounded output strings, optional no-goal operations, genetic arrays and
    scalar outputs, and the inclusive native random endpoint.
    Scalar indices and indirect native accesses remain under review.
  - checks actual RoQ open/run/stop, chunk capacities and short reads, final
    payloads, embedded packet boundaries/nesting, mono/stereo expansion limits,
    malformed looping movies, and cleanup before the first frame;
  - checks RoQ dimension/product limits, complete quad groups in both frame
    halves, exact-sized resampling sources and textures, hardware limits,
    native averaging, and matching preview/videoMap upload dimensions;
  - checks every full RoQ codebook truncation and fixed RGBA table entry,
    partial updates, all VQ opcodes, exact payload prefixes, control-word refill,
    both frame halves, signed/unaligned motion, and rejection before writes;
  - checks the actual BMP loader at four input alignments with every small
    file truncation, 8/16/24/32-bit pixels, palettes, row padding/offsets,
    orientation, extended headers, dimensions, and cleanup before errors;
  - checks the actual PCX loader with exact input/output allocations, every
    small-file truncation, all palette colors, literals/runs, padded scanlines,
    extents, maximum dimensions, and nonfatal failure ownership;
  - checks the actual TGA loader at four alignments with every small-file
    prefix, raw/gray/RLE pixels, IDs, packets crossing rows, native origin
    behavior, extreme dimensions, and cleanup before errors;
  - executes the actual JPEG renderer and bundled codec with exact input
    allocations, every small-file prefix, short files/refill boundaries,
    grayscale/RGB pixels, malformed tables/dimensions, complete cleanup,
    tiny screenshots, growing output and injected allocation failures;
  - executes actual MD3 registration/conversion with exact unaligned files,
    every small-file prefix, signed offsets/counts, disjoint sections, names,
    indexes, finite metadata, native format limits, LOD staging and fallback;
  - executes actual MD4 registration, complete layout validation, native endian
    conversion and animation skinning, with variable weights, LOD/surface
    progress, bones/back references, extreme frame indexes and tess bases;
  - executes native shader parsing/registration and both archive index passes
    with 0–10 stages, following definitions, quotes/comments/nesting, EOF tails,
    skipped excess-stage cinematics, native modifiers and valid/default caching,
    malformed/non-finite color/texture vectors and 1,021 native byte goldens,
    under both normal sanitizer and optimized release-fast-math configurations,
    plus native modifier/deformation bytes and every non-finite numeric field,
    sky/sun/fog/sort metadata, exact sky paths/imports and retained rejected sun state,
    native registration/remap names, lighting modes, images and extended-byte caches;
  - executes native shader archive initialization, checked file/aggregate sizes,
    reverse input release, empty startup/restarts, native paths and 4,096-file cap,
    malformed-file isolation and 768 stock duplicate/missing lookup goldens;
  - compares patch LOD propagation with 401 tiny stock-recursive oracles and
    passes a 4,096-patch chain with a 512 KB stack and no traversal allocation;
  - checks native planar face distances for finite-source product/sum/cancellation
    overflow with four-alignment state retention, plus exact finite classification
    and large-coordinate compatibility;
  - independently measures complete native collision-map allocations and checks
    exact aggregate fits and four-alignment over-budget rejection, including
    derived visibility/areas/inline indexes and actual repeated patch output;
  - executes the actual permanent allocator in release/debug modes, checking
    native cacheline/header costs, both banks, live temporary capacity, exact
    fits and failure ownership at negative/signed-maximum boundaries;
  - executes native renderer curve refinement with eight captured output
    fingerprints, finite-source overflow and workspace-failure rejection at
    four FS alignments, and actual nodraw skips before control allocation;
  - rejects finite collision controls that overflow native midpoint/distance,
    edge differences, cross products or normalization before publication,
    retaining the loaded patch and releasing file/grid/winding ownership;
  - executes native collision plane/facet budget rejection before map reset,
    checks last legal plane/border insertions and matching full-table planes,
    and retains loaded geometry and persistent debug state after preflight;
  - checks actual facet winding ownership on every missing border, full
    clipping and invalid bounds, plus exact native copies of zero to 64 points;
  - executes actual collision patch/winding refinement with captured native
    output, subdivision overflow rejection on both axes and all four FS
    alignments, plus workspace failure cleanup before world/checksum changes;
  - executes actual BSP/model allocation and null-model bootstrap with remaining
    cache-slot boundaries, complete brush descriptors/handle lookups and
    four-alignment rejection preserving registry/world/ownership;
  - validates all BSP tree/forest components before resets with actual loaders,
    native parents/point queries, cycle/alias/OOM mutations, exhaustive small
    graphs, deep/raised-budget trees and a limited test stack;
  - executes actual world loading, bounded entity tokenization and light-grid
    sampling with non-NUL/prefix text, remaps/partial settings, unsafe grid
    calculations, temporary OOM, exact one-point/corner arrays and RGB goldens;
  - executes actual collision/renderer geometry rejection before state changes,
    with finite-field mutations, native patch capacities and real face/triangle/
    curve-parser goldens at tessellation/storage boundaries;
  - executes the actual BSP lightmap loader with exact RGB input and bounded
    upload slots, including the single-source workaround, final RGB samples,
    color-coded textures, empty/partial records and aliased geometry alpha;
  - checks BSP payload material/index/span references through shared bytewise
    preflight and actual collision loading, preserving map/checksum/ownership
    before rejection, with retail area/submodel boundaries and ignored fields;
  - checks the shared collision/renderer BSP header preflight and actual
    collision loading with exact input, every header prefix, lump ranges/strides,
    actual native allocation sizes and raised map-compiler budgets,
    visibility rows, cleanup before errors and retained world state.

GitHub Actions dependencies are pinned to exact release commits, and
Dependabot is configured to propose GitHub Actions updates.

The native skin runner exercises complete default/plain surface allocations,
name and 32-surface/token boundaries, malformed exact input prefixes, file
ownership and valid/default cache reuse through the actual entry points.

The actual legacy font runner checks the unchanged 20,548-byte little-endian
layout, signed words, 256 glyph names/handles, all short input lengths and FS
alignments, malformed names/floats, publication boundaries, cache capacity and
balanced file ownership.

The enabled native FreeType ownership runner replaces only unavailable legacy
header imports in a disposable source copy. API/FS/graphics imports are isolated;
all native bodies remain unchanged. It checks face-before-input shutdown,
bitmap/page/file ownership, controlled failure/default output, checked requests
and repeated library init/shutdown. The same actual-body seam checks bounded
metric arithmetic, complete uploaded glyph rectangles across zero/small/max
sizes and pages, TGA output shapes, generated cache reuse, independent literal
legacy LE bytes and public-reader reload of saved multi-page output. It does
not test a real rasterizer.

The actual shader fixture also covers native identity-alpha skip and matching
multitexture alpha/RGB waves, rejects changes without mutating stages, and
checks real registration pass counts and cache reuse.

## Run the portable checks locally

```sh
export TMPDIR="${TMPDIR:-/var/tmp}"
bash tests/check_shell_syntax.sh
./build_mac.sh --help
python3 -m py_compile create_appledouble.py generate_icon_r.py macbinary_encode.py
python3 -m unittest discover -s tests -p 'test_*.py' -v
bash tests/run_host_regressions.sh            # every runner, $CC (default cc)
CC=clang bash tests/run_host_regressions.sh   # the same runners with Clang
pwsh -NoProfile -File ./build_mac.ps1 --help
```

PowerShell parser validation is also part of CI; see
`.github/workflows/portable-ci.yml` for the exact command.

The native shader runtime runner compiles the actual math/noise bodies in normal
and release fast-math sanitizer configurations. It compares 645 valid waveform/
color/alpha samples and 513 noise samples with the original native formulas under
the same compiler flags. Noise matches exactly in normal builds and within four
float epsilons under release reassociation. Conversion boundaries, every noise
coordinate's non-finite/extreme values and actual waveform, bulge and diffuse
consumers check overflow handling; all 8,192 native fog samples retain their
density and non-finite coordinates return zero. Graphics imports and target GPU behavior remain
deferred.

The native sky runner compares complete graphics traces and cloud vertex/UV/index
arrays with pre-fix native bodies across 1,122 finite bounds cases in normal and
release fast-math sanitizers. Non-finite sides skip before conversion; finite
extremes clamp to the cube before multiplication. Zero through eight stages share
one indexed cloud mesh, preserving draw geometry. Invalid grid/count/capacity
cases reject before buffer/counter writes and preserve backend sentinel slots.
The runner extracts the unchanged native Q_acos body from common.c into TMPDIR;
its exact source seam fails if the signature stops matching. Graphics callbacks
are isolated and do not claim live GPU acceptance.

The sky fixture also checks complete native cloud tables for 12 stable heights
(5,832 points), six extreme heights against independent sphere/direction
invariants and complete default fallback for invalid intersections/non-finite
inputs. Normal parameters/UVs match native values exactly. Release reassociation
must remain within eight float epsilons for parameters and four for normalized
UV directions; acos near an endpoint can amplify a final-bit rounding change.
The stage fixture checks that later shader failures preserve cloud state and
accepted duplicate sky fields publish only the final layer.

The native AAS file runner checks both retail v4/plain and v5/header-obfuscated
formats, every header prefix with actual/advertised read lengths, all fourteen
lump range/stride classes and preflight retention of a previous world. Empty,
sequential, reversed and mixed payload layouts retain native decoding and exact
read/seek behavior. Short reads, seek failures and every real/dummy allocation
failure close once and clear partial logical owners. The fixture distinguishes
logical releases from physical arena reclamation: native hunk memory stays owned
by the arena until reset. Graph/reference/numeric and aggregate budget checks
remain follow-on #47 work; accepted layout fixtures are not live bot worlds.

The native AAS endian runner compiles the actual swap body with independent
identity and byte-reversal helpers. It checks bbox integer and float fields,
fractions, signed zero and finite float extremes against literal byte reversal,
including complete round trips. Existing unsigned 16-bit reachability travel
times retain their full range and round-trip bits. This models PowerPC byte
order on the host; it does not execute a PowerPC binary.

The native AAS writer runner checks complete independent header/lump byte output
and retained world metadata/data under both endian models. Open, every header/
payload write and header-seek failures report false, close once and leave the
world unchanged. Every negative/overflow count and missing nonempty buffer
rejects before opening the output; aggregate signed file-offset overflow also
rejects. Native zero-length layouts and repeated writes preserve behavior.
These fixtures check serialization and ownership, not traversable bot graphs.

The stage fixture also verifies complete native default materials after shader
rejection or missing implicit textures. Parsed sky/fog/deform/stage/render flags
are discarded while the failed name remains cached, all lighting-mode probes
reuse it, exported registration keeps returning zero, and the following healthy
definition remains usable. Fixture bootstrap now supplies the native default
image after each reset, matching the renderer's actual initialization contract.

The native AAS geometry runner loads independent retail triangle/area byte
layouts in both versions and both lump orders. All six legacy plane types,
signed edge/face orientations and native flag bits retain exact payload bytes.
Every geometric float is tested with six non-finite patterns; reference/range
failures, INT_MIN signed references, paired-plane limits and inverted bounds
reject before loaded publication and clear partial logical ownership. Normal
and release fast-math sanitizer configurations both run in CI. Node termination,
routing references and derived runtime math/budgets remain follow-on work.

The native bot allocator runner compiles shipped, debug and optional tracked
implementations. Zero-to-65-byte raw/cleared allocations retain ownership-prefix
and payload behavior. Nullable imports, unsigned/header/signed-length overflow
and null cleanup cannot write or acquire ownership; tracked counter limits also
reject before imports. Heap releases physically and hunk releases logically.
This does not prove that every bot-parser caller handles allocation failure.

The native AAS node runner checks paired plane and area-leaf references, root/
settings storage and termination in every graph component. Iterative validation
retains chains and shared acyclic graphs without modifying node bytes, releases
its temporary heap workspace and rejects workspace failure. It extracts the
actual AAS_PointAreaNum body through a strict source seam and retains native
area/solid results in normal and release fast-math sanitizers. Optional empty
lumps retain required safe root/plane/area/settings fixtures. Routing references
and runtime/arena budgets remain follow-on work.

The native AAS reachability runner retains all 32 travel slots, team flags,
signed geometry references, uint16 travel times and packed elevator/jump-pad/
func-bob fields. Invalid endpoints, destinations, ordinary references, per-area
spans and aggregate reference ownership reject before loaded publication. Both
normal and release fast-math sanitizer configurations check actual loader bodies.
Portal/cluster references and derived routing math/work budgets remain pending.

The native AAS portal runner retains literal portal/index/cluster/settings bytes,
side ordering and unclustered roots. Signed/one-past indices, relative area slots,
cluster area/reach counts, inverse portal ownership, missing real portal sides
and aggregate backing spans reject before loaded publication. Both normal and
release fast-math sanitizer configurations run; full runtime geometry/budget
and transactional hunk acceptance remain pending.

## What this CI does not prove

Portable CI does not compile a PowerPC PEF, preserve/inspect a Classic resource
fork inside a mounted HFS image, use retail PK3s, or exercise AGL,
DrawSprocket, InputSprocket, Sound Manager, Open Transport, Finder events, or
Mac OS 9 runtime behavior. Passing these starter checks alone does not close
security or target-runtime issues; each issue needs its specified regressions
and applicable target evidence.

For every new PR, require successful CI and a clean Codex review on the current
head, plus resolution of every CodeRabbit finding. The user confirmed that
CodeRabbit rate limits need not block a merge once this gate is satisfied.
Resolve findings, push fixes, and obtain successful checks and renewed Codex
review. Absent, pending, or failed Codex review is not a clean review.

The user has deferred retail-content and Mac OS 9 live testing to a follow-up
session. Host-tested fixes may merge with those limitations recorded; do not
mark the outstanding target acceptance checks complete.
Host-tool-only changes do not require unrelated engine or target tests unless
their issue's acceptance criteria specify them.

The next CI layers should be:

1. a legally provisioned/self-hosted Retro68 runner that builds base and Team
   Arena and validates `Joy!peff` / `pwpc`;
2. hostile-input ASan/UBSan harnesses for the P0 parser/protocol issues;
3. mounted HFS resource/Finder inspection;
4. emulator smoke tests using externally provisioned legal game data.

When adding a regression for a GitHub issue, name the issue in the test and
update its nested checkbox in [task.md](task.md); leave the issue-level
checkbox open until all required target evidence exists.

The bot-zone runner exercises the actual native zone allocator, engine bot
imports and bot adapters under release/debug metadata and normal/optimized
sanitizers. It checks payload/header/trailer/alignment costs, ownership and
nullable rejection before native allocator expansion can overflow. CI also
invokes this runner with explicit Clang.

The area travel-time runner extracts actual native area classification and
conversion bodies with strict source seams and uses the real vector length.
Literal walk/crouch/swim, minimum-time and representable route costs retain
legacy results. Nonfinite inputs and finite overflowing derived distances
saturate before integer casts under normal and release fast-math sanitizers.
The routing-time runner exercises actual area/portal cache updates, route
selection and hide-area routing, replacing only cache providers and using
the native projection body. Literal normal and maximum costs
prove that area/crossing/reachability additions cannot wrap into cheap routes;
nonfinite or oversized float cache starts saturate before uint16 conversion,
and enemy-distance penalties cannot overflow casts or additions.

The isolated-area routing runner uses actual native clustering and routing.
Reachability-only clustering legitimately leaves isolated nonreachable areas
in cluster zero. Distinct start/goal queries involving those areas return
unreachable before cache allocation or mutation; their retail AAS bytes remain
accepted. The previous native routing body aliases another cluster's cache.
Normal and release fast-math sanitizer configurations cover both operations.

The routing-workspace runner executes actual native area-cache updates with
22 degree/filter goldens through 1,024 incoming links, including 128/129, and
nullable/unrepresentable workspace failures. Scratch has exactly one checked
heap owner and leaves no retained freed pointer. The failure runner uses actual
area/portal cache providers and routing consumers; nullable cache/workspace
failures preserve existing cache/LRU ownership, release unfinished owners,
clear pending portal flags and permit successful retries. All three public
route-provider failure paths are exercised under normal/fast-math sanitizers.

The cache-allocation runner exercises actual native allocation, cache linking
and physical free. Literal count/layout goldens retain zero through 1,024
entries; negative/wrapped counts and invalid signed byte budgets reject before
imports or mutation. Exact signed byte capacity remains accepted. Nullable
ordinary and large representable requests preserve accounting. Normal/fast-math
sanitizers cover original pointer/counter overflow and undersized-cache proofs.

The cache-file runner round-trips the actual native version-two writer and
reader, retains literal route results, and regenerates routes after area state
changes with exact loaded-cache accounting. Stored pointers are discarded.
Every file truncation, malformed headers/records/floats/indices, short reads,
nullable imports and cumulative signed budget failure reject without changing
existing cache/table/LRU ownership. Empty dumps remain valid. GCC and Clang
normal/release fast-math sanitizer runs cover their different float assumptions.

The routing-initialization runner executes the complete actual derived-table
pipeline and native initialization continuation. Every one of ten allocation
failures clears partial owners and prevents world publication/repeated frame
allocation. Valid and empty-dummy worlds retain their native table/route values.
Unrepresentable array sums/products and a fully backed large graph's travel
matrix reject before imports. Pointer rows remain aligned after odd 16-bit
cost counts. Normal/release fast-math sanitizer runs cover the pipeline.

Initialization regressions also execute the actual frame entry: each nullable
stage with a pending cache-save request skips serialization of cleared tables.
Pending reachability defers the request until a later initialized frame performs
the actual native cache write/close/reset. Frame return values/bookkeeping stay
compatible; the previous frame body dereferences a null cache table.

The bot-libvar runner includes the actual converter and variable backend.
Seventeen literal native-value goldens, long/trailing decimals, malformed text
and unrepresentable integer text exercise normal/release fast-math sanitizers
with GCC and Clang. Name lookup, cached defaults, setter replacement and physical
name/string cleanup retain native behavior. Original actual-body proofs cover
trailing-NUL overread, signed divisor overflow and incorrect malformed/overflow
values. Allocation failure in other parser callers remains separate acceptance.

The native initialization fixture also checks twelve literal routing-cache cvar
KB/byte limits, including defaults, fractional truncation, the last safe signed
multiple, larger finite inputs and unsupported negative capacity. Original
float-to-int and signed multiplication overflow proofs fail; checked conversion
retains successful table initialization and cleanup. This establishes numeric
representability; aggregate cache RAM/work budget enforcement remains separate.

Libvar regressions also cover whole/interior aliased replacement and nullable
name/value allocation. Sixteen failed new factory/getter/setter creations with
and without an existing node, plus two failed replacement variants, preserve
prior dictionary/value/flags ownership and retry. Invalid pointer inputs have
native missing-value defaults, and successful cleanup physically frees all
owners. Auditing every direct pointer-factory consumer remains separate work.

The builtin runner includes the complete actual native preprocessor and its
token copy/free/error helpers. Literal date/time/line/file expansions retain
their text, native types/subtypes, whitespace and source-location metadata.
Repeated expansions borrow unchanged runtime time storage. Failed time
conversion releases the copied token, reports an error and retries safely;
empty/unknown expansions release their unused copy. Nullable copies keep the
native fatal diagnostic and return no token if that diagnostic returns.
Maximum valid filenames remain accepted. Six original actual-body failure
proofs cover invalid static frees, null conversions, an unused copy leak and
null token access. GCC and Clang run normal/release sanitizer modes in CI.

The include runner executes the actual native preprocessor directive, token
read/unread, path normalization, prefix updates and script publication. Native
direct/fallback lookup order, complete angle pieces and the last fitting path
remain valid. Oversized quoted fallback and angle paths reject before any
partial lookup; angle failures consume through the delimiter while missing
delimiters/operands preserve next-line tokens. Empty filenames, skipped includes,
prefix/terminator/separator costs, overlapping separator removal and recursive
script rejection exercise diagnostics and exact physical ownership. Eight
original actual-body proofs cover the unsafe paths. GCC and Clang run normal
and release fast-math sanitizer configurations in CI.

The macro runner uses the whole actual preprocessor and shared lexer/heap/print
interfaces. Native text/type goldens also pass against the original code.
Stringize reserves closing quote/NUL space and builds privately; name/string
pastes check complete costs before mutation. Boundary, alias and malformed-empty
cases cover token helpers. Actual parameter substitution, stringized invocation
metadata and source-queue publication remain valid. Every one of six returned
nullable copies, oversized paste/stringize, incomplete arguments and unsupported
pasting releases private argument/output chains without publishing partial
output. Failed expansion retries and preexisting source tokens remain owned.
Maximum native parameter count is retained. Eight original actual-body failure
proofs fail; GCC and Clang normal/release sanitizer CI covers the fixed paths.

The character runner links the actual native character parser, preprocessor,
lexer, libvars and Q_shared core; file/heap/printing are imports. Real skill
blocks retain integer/float/string values and public characteristic 79. Complete
prefix/path boundaries reject before VFS imports or default fallback. Index 80
and larger reject before narrowing/assignment. Nullable character/either string
allocation and malformed selected skill blocks free prior source/character/string
owners and retry. Real quote stripping uses overlap-safe copies and handles
empty/unmatched text. Ten original actual-body proofs reproduce the previous
path/index/null/EOF/quote defects. GCC and Clang run normal/release sanitizer
configurations; default/interpolated-character ownership remains separate work.

The source runner links the same actual native parser/lexer/core and exercises
file/memory source creation, punctuation tables and real copied global macros.
Every base owner and all twelve base/global-copy allocation positions unwind
without partial publication; original global owners/counters survive and retry.
Negative/overflowing signed costs and full native path overflow reject before
imports, last signed/byte/name boundaries remain representable, short reads
close/release staged owners, and empty/anonymous sources remain valid. High bytes
outside strings reject with unsigned whitespace/table classification. Real
memory macros and punctuation parse through EOF. Fourteen original actual-body
proofs cover the previous null/cost/read/name/path/global/byte defects. GCC and
Clang run normal/release fast-math sanitizers; other directive/indent allocations
and aggregate parse budgets remain separate acceptance.

The global registry runner executes actual definition creation/deletion, real
source/global copying and macro expansion. Head, middle and tail deletion unlink
the selected node before physical cleanup; prior source copies remain valid and
later sources see only surviving originals. Parameter/body/name ownership and
native token counts release exactly once. Missing, case-distinct and null names
preserve the registry; duplicate removal retains native first-match behavior.
Five original actual-body proofs expose three use-after-free paths, null-name
lookup and failure to unlink a duplicate. GCC and Clang run normal/release
fast-math sanitizers; remaining directive factories and parse budgets stay open.

The interpolation runner uses real parsed lower/upper skill characters and the
actual native interpolation/cleanup/getter bodies. Literal native float values,
lower integer/string choices, mixed/unset fields, independent ordinary/empty
string copies and first-free handles remain compatible. Every output/header and
string import observes an unpublished result. All three nullable stages release
partial output, preserve every input field/string owner and retry into the same
handle. Native invalid handles and a full table reject before imports. Four
original actual-body proofs fail; ordinary native goldens also pass against the
original body. GCC/Clang normal/release sanitizer CI covers ownership; default
inheritance and character numeric bounds remain separate work.

The default runner parses real default/target characters, executes native
inheritance and skill/cache/reload paths, and measures physical owners. Missing
strings clone before any target field changes. Three primitive nullable stages
and nine cached/new/reload caller failures preserve prior targets/defaults and
registry, free prospective strings/new targets and retry. Literal native numeric
and ordinary/empty string values, occupied fields, self/repeated inheritance and
missing-file fallback remain compatible. Six original proofs isolate null copies,
early mutation and ignored caller failure; original valid goldens pass too.
GCC/Clang normal/release sanitizer CI covers these owners; numeric/cursor bounds
and remaining parser consumers stay open.

The number runner executes real lexer tokens, the actual converter and character
parser. Eighteen original native goldens preserve spellings/bases/suffixes and
values. Each base retains its unsigned maximum and rejects the next overflow
before wrap, preserving following tokens. Long leading zeros retain native
token capacities. Trailing dots, maximum fractions, uppercase hex, malformed
prefixes/multiple dots and finite floats beyond auxiliary integer range exercise
conversion. Direct converter extremes exceed host long-double/divisor limits;
real character files reject wrapped indexes and release earlier owners. Eleven
original proofs fail; original ordinary goldens pass too. GCC/Clang normal/release
sanitizer CI covers these paths. Consumer numeric bounds and lexical cursors stay
open.

The escape runner executes real quoted-token reads and actual escape/legacy
literal helpers. Eighteen escape spellings retain native bytes, decimal ASCII
and hex letter mappings across direct/double/single-quoted paths. Oversized
values clamp once before signed overflow, consume complete digits and preserve
following tokens. Invalid escapes reject without modifying the direct output.
Separate legacy literal tests cover bounded EOF/empty/newline failures and
first-byte warnings without skipping closing quotes or following token bytes.
Active single quotes still use the native string reader and length subtype.
Concatenation, disabled escapes, string limits, comment EOF and short longest
punctuation matches stay compatible. Eight original proofs fail; native goldens
pass original and fixed code. GCC/Clang normal/release sanitizer CI covers these
paths; lexer-error propagation and remaining parser consumers stay open.


The source-error runner links the actual source/lexer/parser/core bodies. Failed
root and nested include reads retain their physical stack and queued tokens;
string lookahead rejects its outer token after a nested read error. Suppressed
diagnostics and formatting flags preserve private status. Cached script tokens
cannot bypass a lexical failure. Preprocessor error history survives normal
include unwinding while native directive recovery remains available. Historical
errors do not reject later EOF strings; new lookahead errors still reject, and
explicit script reset clears private parse status while retaining format flags. Ordinary
one/two-level includes and EOF remain valid, with balanced file and heap owners.
Raw-memory unterminated comments reject; file comment compression and publication
callers remain separate acceptance work. GCC/Clang normal and release fast-math
sanitizers run the same fixture.


The weight runner links the actual native weight/source/lexer/libvar/core bodies.
Ordinary return, nested case/default and balance fields, implicit zero defaults,
name lookup, public fuzzy evaluation, cache reuse and the native 128-weight
warning/truncation behavior stay valid. All eleven configuration/name/separator
imports fail in both cached/reload modes without private owner or cache mutation.
Root/nested source errors, recovered inner directive errors, incomplete named
weights and malformed/duplicate defaults reject before publication. A prior
cached configuration retains its header, names, tree values and slots. Every
failed load retries, and complete shutdown frees physical owners. Both GCC/Clang
normal and release fast-math sanitizers run the fixture.


The characteristic integer runner links actual character/source/lexer/libvar/core
bodies and compares 9,612 representable truncation/bound results with original
native getters. Parsed wide floats and directly supplied extreme/non-finite
values reject unsafe unbounded casts; bounded finite floats clamp before casting,
including exact full signed bounds. Signed integer fields, wrong-type/index and
reversed-bound fallbacks retain native behavior. Every getter preserves complete
character/string owners without imports. The reference header contains original
actual getter bodies at the documented parent and is used only for safe values.
Both GCC/Clang normal and release fast-math sanitizers run the fixture.


The characteristic string runner executes actual character/source/lexer/libvar/
core bodies. Ordinary/empty strings retain every copied/truncated/NUL/padding byte
and surrounding canary for capacities one through 1,024 (2,048 outputs). Missing
buffers and zero/negative/INT_MIN capacities perform no write or import. Valid
capacity handle/index/type errors retain native diagnostics and unchanged output.
Complete character and string owners remain intact until balanced cleanup.
Both GCC/Clang normal and release fast-math sanitizers run the fixture.


The character numeric runner links actual character/source/lexer/libvar/core
bodies. Finite representable values, full FLT_MAX, subnormal narrowing and native
signed/unsigned 32-bit integer word patterns retain stored bits. Oversized raw,
macro and included floats/words reject before character publication, release
prior strings and all physical source/table/dictionary/token owners, and can
retry. Both GCC/Clang normal and release fast-math sanitizers run the fixture.


The skill runner links actual public/cached character/source/lexer/libvar/core
bodies. Quarter-step requested skills, finite extremes and signed infinities
retain native one/five clamps and cohort interpolation/cache values. Cached
rounding, exact cache hits and any-skill fallback retain behavior through safe
signed endpoints. Invalid NaN/public or non-finite/unrepresentable cached skills
reject in cold/occupied/reload states without file/heap activity or mutation of
any cached header/field/string/slot. Both GCC/Clang normal and release fast-math
sanitizers run the fixture.


The interpolation numeric runner links the actual character/source/lexer/libvar/
core bodies. Non-finite desired/endpoints, equal skills and overflowing endpoint/
scale arithmetic reject before imports. Invalid fields/derived results reject
with physical release of the output and any earlier copied strings. Native
ordinary/extrapolated values and input owners remain intact; compiler
reassociation may retain a complete mathematically finite zero for extreme
opposite fields. Missing second-skill fallback returns the existing available
character. A variadic log interface renders the actual dump, including floating
skill text. Representation checks and fixture input words use volatile integer
materialization to prevent release finite-math assumptions from erasing tests.
Both GCC/Clang normal and release fast-math sanitizers run the fixture.


The float getter runner links actual character/source/lexer/libvar/core bodies.
It compares 9,612 finite result bit patterns against original getter bodies and
retains signed integer conversion, negative zero, native error fallbacks and
five defined infinity-bound pairs. NaN bounds and non-finite stored fields reject
with zero/error in normal and release fast-math builds, preserving complete
character/string/registry owners without heap imports. Both GCC/Clang sanitizer
configurations run the fixture.


The synonym runner links actual chat/source/lexer/libvar/core bodies with physical
heap/hunk and file imports. It retains native contexts, list/entry/string order
and float weights, checks aligned private entries and rebased final hunk pointers,
and rejects root/included lexical/preprocessor errors before publication.
Changed second-pass capacities, missing second sources, all eight source factory
imports, heap staging and final hunk allocation failures retain no partial owners.
Unrepresentable weights and non-finite aggregate float weights reject. Both
GCC/Clang normal and release fast-math sanitizer configurations run the fixture.


The expression token runner links actual directive/evaluator/source/lexer/libvar/
character/core bodies. All four hash/dollar result helpers retain 324 native
spelling/type/sign cohorts with complete magnitude metadata; 284 defined original
text cohorts also pass the original body. Long magnitudes avoid signed absolute
value overflow; wide finite floats clamp auxiliary unsigned integers before
casting, and NaN/infinity results reject. Eight nullable number/sign imports keep
native queued next-line owners and release uncommitted copies, retaining copy
factory fatal severity. Public directive flow and real character fields consume
correct metadata. GCC/Clang normal and release fast-math sanitizers run the fixture.


The expression collection runner links actual directive/evaluator/source/lexer/
macro/character/core bodies. Undefined names, divide-by-zero, malformed operands,
missing defined names, invalid dollar delimiters and lexical tails release copied
operand chains before returning failure. Six nullable operand imports retain no
partial tokens and initialize caller outputs. Real macro expansion failure keeps
original definition owners, while valid macro/defined values and historical-error
recovery retain native behavior. GCC/Clang normal and release fast-math sanitizers
run the fixture.


The expression arithmetic runner links actual evaluators/source/lexer/character/
libvar/core bodies. It compares 5,184 ordinary integer/float calculations, with
4,536 defined original cases also passing the original body. Signed boundaries,
division/remainder traps and invalid shifts reject without partial owners. Native
negative/high-bit left shifts, arithmetic right shift, literal minimum words,
fractional float division and finite float calculations retain intended results.
Float mode avoids unused integer arithmetic; double narrowing and non-finite
intermediates reject before unsafe operations/publication. GCC/Clang normal and
release fast-math sanitizers run the fixture.


The source factory runner links actual unread/indent/directive/source/lexer/
expression/character/core bodies. Failed unread copies preserve complete prior
queued token/caller bytes, record native copy-factory severity/source status and
retry. Failed line lookahead records status without a partial queued owner.
Conditional push failures preserve prior stack/skip bytes; if/ifdef/ifndef callers
propagate failure, retaining native next-line token ownership. Else/elif reuse
complete frames without replacement imports. Failed expressions preserve prior
frame bytes/skip and subsequent endif recovery; exhausted included scripts reject
cross-script replacement without touching an enclosing frame. Six pre-review
proofs reproduce these failures. Real nested else/elif source conditionals and
unread order pass original goldens. GCC/Clang normal and release
fast-math sanitizers run the fixture.

The definition runner links real macro/source/expression/lexer/libvar/character
bodies and physical heap callbacks. Twenty-nine original actual-body proofs expose
partial new/replacement definitions, external-factory leaks, nullable imports,
lexical-prefix publication and prior-name parameter expansion. Fourteen malformed
new/replacement cases preserve complete hash/header/name/token bytes and release
staged owners. All seven new/replacement imports and nine external imports fail
cleanly; the native 128-parameter capacity remains and 129 rejects. Original valid
object/function/replacement/empty-marker/external macros pass separately. GCC and
Clang run normal and release fast-math checks. Empty macro invocation is a separate
source-reader defect, retained as outstanding work rather than claimed covered.

The empty-expansion runner links actual source/macro/lexer/libvar/character bodies.
Ten original-body proofs reproduce false EOF after object/function/empty-argument
expansion, broken string concatenation, hidden lexical errors, premature included
EOF, a false direct queue result, rejected complete character fields and unconsumed
empty-only input. Fixed readers verify actual script EOF, native token/string order,
unchanged queued bytes/no imports, physical included-script release and complete
native character publication. Original nonempty object/function goldens remain.
GCC/Clang run normal and release fast-math sanitizer modes.

The movement-setup runner compiles the complete actual movement implementation,
actual libvar backend and native brush classification. Forty original-body proofs
cover both imports of all ten required variables with empty or complete prior
references. Fixed failures stop setup before publishing references/model bytes,
retain only complete shared cached owners, retry, and physically release all owners.
Original default/configured/cached reference identity and brush classifications
remain. GCC/Clang run normal/release fast-math sanitizers. The full source exposed an
existing integer-abs-on-float elevator warning during initialization validation.
The later [distance step](bot-elevator-distance-validation.md) corrects it; the
current movement fixture compiles without diagnostics in both compiler modes.

The file-comment runner validates actual file/source/include/character imports
before compression can erase unterminated block comments. Six original-body proofs
fail, including both native include forms/fallbacks and complete-prefix character
publication. Two pre-review escaped/single-quote proofs additionally fail; the
scan matches native compression delimiters exactly to prevent erased suffixes. Malformed files close/release all script/punctuation owners with raw
line diagnostics; included failures preserve synchronized parent recovery. The
1,035 valid cases retain native compressed bytes and actual token metadata across
quotes/escapes/line comments/block lengths/newlines. Raw memory lexer behavior and
native characters remain. GCC/Clang run normal/release fast-math sanitizers.

The compressed-file EOF runner compiles real file/source/include/conditional and
lexer bodies with physical ownership. Five original-body proofs expose stale
nonempty/empty EOF intervals, retained root frames/skip state and active/skipped
child conditions that survive script release. Fixed EOF updates reclaim exhausted
child frames/scripts, preserve complete parent frame bytes/skip and recover native
body/endif/tail token order. Repeated root EOF warns once and retains only complete
source owners. The original 1,035 compressed-byte/token comparisons and unchanged
noncompacting source EOF pass. GCC/Clang run normal/release fast-math sanitizers.
