# Review continuation evidence — 2026-09-18

Historical opening snapshot source: master `7f1e8168688ca393b2097c22508db998768b1c0b`,
through merged PR #148. GitHub issue/PR inventory refreshed at 2026-09-18 20:24 UTC.
All 52 original issues remain open, with no additional open issue. This records
implementation evidence and next actions; it does not replace issue acceptance.

## Assessment

Retain all 52 issues, their priorities and their acceptance criteria. The
[initial reassessment](review-2026-09-17.md) already distinguished committed
candidates from incomplete work. Further AAS, allocator and bot/parser steps
have merged, so obsolete pending-merge descriptions must give way to current
implementation status. Source/build evidence does not establish complete
malformed-input coverage, aggregate resource budgets or target compatibility.

PRs #126–#130/#132 finish selected AAS portal, travel/routing, workspace, cache,
initialization and cache-limit conversions. PRs #131/#133–#148 finish selected
native variable/parser/character ownership, numeric, source-error, weight and
getter checks. Each merged step passed its exact-head Portable CI, completed
clean Codex review and resolution of every CodeRabbit finding. Detailed dated
evidence remains in the subsystem documents; retain #47/#48 for broader work.

The July advisory comparison in security-provenance.md is historical. #29 stays
an assurance gate: refresh authoritative advisory/upstream provenance, map each
accepted family to actual regressions and document exceptions. Do not infer
complete modern-CVE coverage from the many loader/parser fixes.

## Pending implementation and validation

| PRs | Scope | Remaining gate |
| --- | --- | --- |
| #149–#162 | Character inputs/float getters, synonyms, expression ownership/arithmetic, parser factories/defines/empty expansion, movement and compressed-file boundaries | Dependency-ordered merge after current-head CI/review gates |
| #163–#168 | AAS/action/item/structure/goal setup and proximity checks | Same merge gate; full world/aggregate acceptance remains open |
| #169–#170 | Level-pool/map-info staging, engine/game propagation and restart readiness | Same merge gate; retail void import and unknown-variable restart compatibility retained |
| #171 | Retail QVM source recipes and bounded formatter/print imports | Independent merge gate; real retail/module-selection acceptance remains open |
| #172–#177 | Projectile model offset, weapon configuration/setup/weight/table publication and public consumers | Same merge gate; #172 host job exceeded its ten-minute timeout |
| #178–#179 | Chat lifecycle and validated console-pool replacement | Same merge gate; #179's node-ownership finding is fixed and resolved |
| #180 | Increase only the accumulated host regression job allowance to thirty minutes | Independent merge gate; four required CI checks remain mandatory |
| #181 | Bounded public chat queue strings and input/output guards | Complete bot review and dependency-ordered merge gate |

PR #179's initially reviewed helper accepted foreign, one-past, misaligned,
stale or shared queue nodes. The revision validates aligned current-pool
membership before dereference and uses a checked temporary bitmap for unique
ownership across queues. All five actual-body failure proofs reproduce against
the reviewed body; fixed checks and both PPC products pass. Codex completed a
clean review of `5a7b809ee4b3766635dbef4837b96a8e02136770`; its CodeRabbit thread
is resolved. Follow-up CodeRabbit hourly backoff does not waive other gates.

### CI snapshot

This is dated scheduling evidence, not a merge authorization. Re-query every
current head and all review threads immediately before any merge.

| PR | Exact head | Successful Portable CI checks |
| --- | --- | ---: |
| #149 | `48d43d6b832c96450f938138ad446deca2ebd78d` | 3/4 |
| #150 | `05c0f61d830c7dc36a3c99ce24b7b3dc0d816587` | 4/4 |
| #151 | `88550a9dfcc1075bdadf0d7ad6cec352dcf679cf` | 4/4 |
| #152 | `8b159b2c43b2194633445ca31c25c09880378369` | 4/4 |
| #153 | `ec3f80d7c5d0331fefd976624ddd5c03de6be010` | 4/4 |
| #154 | `10dbe1de9dd657373b72a8d362014b062d6ed739` | 4/4 |
| #155 | `8e498ea96783012ae35165406d5b349c3c073c36` | 4/4 |
| #156 | `a9bd49a3aceb35ff551b29145e648ab613541d93` | 4/4 |
| #157 | `6630c3c8c7d4004e3b09f266d07fc2e6222263ee` | 4/4 |
| #158 | `a82523ed78d212c0178ed5ae377b0e7f064b0269` | 4/4 |
| #159 | `05dc84e072dbe64f461f015ffd3a4c1d21e07a77` | 4/4 |
| #160 | `8f8a36781c0db3dfc8414a316f6da4ed31ac5648` | 4/4 |
| #161 | `38ee5fdad8473ddc5a09d00724569497fca34e21` | 4/4 |
| #162 | `a282c46d7917019023f7f10af358de661adc38c5` | 4/4 |
| #163 | `9877c1991bb38c326393453c01cadb6af4e141c7` | 4/4 |
| #164 | `beb95f8d9e1ff5052ab56161a735372d947b2189` | 4/4 |
| #165 | `1d10a6267b15dc9bcdd79aa1a21ebe155355b3e8` | 4/4 |
| #166 | `7ce5c909372ad5a4b3534fc8c5645ea78388504c` | 4/4 |
| #167 | `9d1811d3468ee875c4ee6c11cb0f030fb23e9292` | 4/4 |
| #168 | `617a6e8a4b297fc8abd1d7a90195ee116b7780ca` | 4/4 |
| #169 | `cd4cb789fa15855ac70f407cc44c3f17dbf811d5` | 2/4 |
| #170 | `8f16b45bee908c7602136f0ef85ea987d44d9099` | 3/4 |
| #171 | `a4584211aa1a9eafae0c98f9e16de70825108787` | 2/4 |
| #172 | `d246a80d1a7a2deb0d4489a195e00d3af111a48b` | 2/4 |
| #173 | `ea6c05b6397acdec74a130c1d5fafcdbdc104469` | 0/4 |
| #174 | `ee6bfcfad195c449fbc7f05c0cc6e1b77bada60e` | 1/4 |
| #175 | `8ca005c3e361e9e3dfb9def2c1f7610ece7d8a99` | 0/4 |
| #176 | `e74e0f20187504659049d163ce276009618b6fe1` | 0/4 |
| #177 | `1776c347bc748521593a02351db837959006910a` | 1/4 |
| #178 | `83d1325c28290bcc282f53b5c70867e908ca78e2` | 0/4 |
| #179 | `5a7b809ee4b3766635dbef4837b96a8e02136770` | 0/4 |
| #180 | `3baa7ff5c6cb3d8602f176821e938b58377c1f71` | 0/4 |
| #181 | `0d83b8212719be6442190542e6e7731b91741658` | 0/4 |

## Next actions and deferred acceptance

Merge eligible independent PRs and then burn down the dependency chain from
#149, checking four successful Portable CI jobs, completed clean exact-head
Codex review and every resolved CodeRabbit finding, including outside-diff
findings. A pending/failed/timed-out check is a closed gate. Inspect execution
time and workflow allowance before retrying timed-out host jobs; do not keep
restarting queued workflows without new evidence.

Continue reply/expansion and dictionary/cache consumers, then aggregate
expression/parse/recursion/memory budgets and complete library/world
transactions. Finish remaining renderer/AAS aggregate/query work, legacy-default
network compatibility/security, filesystem/ZIP and message/download regressions,
command/cvar privileges and the wider platform/build queue in docs/task.md.

The user requires commercial 1.32c interfaces and legacy network compatibility
by default, and explicitly defers retail/Mac OS 9 execution. Build both PPC
products after engine changes. Preserve public packed layouts, syscall/table
ABI, valid native behavior and physical engine-arena ownership. Native Windows,
mounted HFS/Finder, legal retail assets and live Mac OS 9 checks remain acceptance
work when their environments are available. No issue closes solely from builds.

Scratch uses `${TMPDIR:-/var/tmp}`, never `/tmp`. Preserve the original untracked
q3-logs/. Remove owned merged worktrees, then prune; retain shared toolchain or
compiler fixtures until their dependents no longer need them.

## Subsequent continuation evidence

Refreshed at 2026-09-18 21:26 UTC, source master
`a0364783c25e7b242e2c0324275cb9df0086239e` through #170 and independent #180.
The earlier dated snapshot remains historical. All 52 issues remain open.

PRs #149–#170 and #180 merged after four green exact-head CI checks, completed
clean Codex and fresh checks of all threads/formal CodeRabbit reviews. #158's
later documentation finding was fixed and resolved before renewed clean Codex
and all four fresh CI checks. Source steps #169–#170 preserve the retail void
import and foreign-engine unknown-variable restart compatibility.

Workflow-only thirty-minute host allowances are on #172–#179/#181/#183/#184,
with renewed completed clean Codex and no unresolved findings. Product source/PPC
evidence is unchanged; each new head requires all four fresh CI jobs. #171's
single long-queued scripts job received one failed-job retry, retaining its other
three successful checks; no merge skips its final gate.

#183–#186 cover remaining chat consumers, combined variables, complete encoded
construction/timing and measured message components. #187 stages aligned random
dictionaries. #188 owns checked complete match pieces and keeps the source
borrowed; CodeRabbit completed with no actionable comments and Codex is clean.
#189 loads checked complete template containers and requires every context
closure; Codex completed clean. Each has original failure proofs, separate native
goldens, six-mode Clang/GCC checks and both zero-diagnostic PPC products. #187's
Codex is also clean. Their four-job CI/dependency gates remain mandatory.

#190 is open for complete reply dictionaries, nullable private owners, bounded
combined bot names, source-error rejection and priorities representable by public
selection. It has 198 nullable checks, 120 original failure proofs, twelve native
goldens, six-mode Clang/GCC checks and both zero-diagnostic PPC products. Its
current-head completed clean review remains required; it is pending at this
snapshot. Complete initial/cache/library/world publication and aggregate parser/
work/memory budgets remain separate work. Retail/Mac OS 9 execution is deferred.

The ledger PR itself is omitted to avoid a self-referential stale SHA.
Query its exact current head and review/CI gates directly before merging.

| PR | Current head | Successful Portable CI checks |
| --- | --- | ---: |
| #171 | `a4584211aa1a9eafae0c98f9e16de70825108787` | 3/4 |
| #172 | `7a16e7735ea3d27384cc5d2c5cd70f84329cbf3b` | 1/4 |
| #173 | `a97619f4c387a4f9ac109df6e316c60536bcfc39` | 0/4 |
| #174 | `14523b3a211e8d506f6152604794ec65969fa96a` | 1/4 |
| #175 | `d9e3236a105ccb8e85459cf248e56334f2bd58da` | 0/4 |
| #176 | `ba2d426e85a7121321b3feeb0b524f17e437cf5d` | 0/4 |
| #177 | `a86892d86e04804548aef91aff8de62b1588b90e` | 0/4 |
| #178 | `5f9f557d5ab406bd439737b57d4db70162142b6f` | 0/4 |
| #179 | `5d9a47f4fb8a7bd0f9eb85b02914af595afb36db` | 2/4 |
| #181 | `494e70e6099d81b2d58f9ea8cbe27b5497aab8da` | 1/4 |
| #183 | `09c2b22ecbf182d4091d5e2e9d378c27b270ced9` | 0/4 |
| #184 | `f153e8944ea8bbe5a7a9e0096cf9cbc4b625ac7b` | 0/4 |
| #185 | `431c889f467c7eff0737360ece35e2a5b0f6e1d9` | 0/4 |
| #186 | `22cee573d7e6c3dfff95733d406c6e8d50c7d74d` | 0/4 |
| #187 | `942999b901493d972979b62c31c352f64c070b68` | 0/4 |
| #188 | `d9e4746a260589d6c9cf5a7d614784da02cf5ee2` | 0/4 |
| #189 | `efecb0331d36e148a3ed4e392d8804b1a5f91016` | 0/4 |
| #190 | `4b1bcaeae12c23c634cc97ee37dd9824a662ee03` | 0/4 |


## Latest continuation evidence

Refreshed at 2026-09-18 23:15 UTC. Actual master is `31a6554caa3c941d6657d664849511e27233089d` through #172 and independent #180. All 52 original issues remain open; no additional open issue exists. Earlier dated snapshots and source SHA references are historical.

#172 merged after all four exact-head CI checks, completed clean Codex and a fresh full CodeRabbit/thread check. CodeRabbit's subsequent actual #173 review identified an outside-diff setup rollback finding. The prepared #174 fix was combined into #173 and validated with both config/setup suites under both compilers and both rebuilt PPC products. #174 is merged into that feature branch, not master. Combined #173 has renewed completed clean Codex with every finding addressed; all four new CI checks remain required. Retarget #175 to master only after #173 merges.

#171's latest workflow conflict is resolved against current master, retaining every master regression and putting the five retail QVM recipe syntax checks in a separate step. All six complete source QVMs retain retail headers/hashes, both PPC products rebuilt without diagnostics, and shared/projectile checks pass both compilers. Renewed exact-head Codex completed clean. Fresh CI remains required.

#175–#193 retain completed clean exact-head Codex, actual original failure proofs, separate native goldens, native allocator/ownership checks and both zero-diagnostic PPC products. #194 stages both goal/movement state factories across all 64 slots. #195 stages goal weight/index pairs and resolves Codex's policy-transition ownership finding by detecting actual shared cache membership for both goal and weapon consumers. #196 preserves exact/any-skill/tolerance character cache hits when all 64 slots are full. All three have completed clean Codex and resolved findings; full world/library/aggregate and physical hunk recovery remain open.

#197–#203 address actual ZIP allocator types/products, buffered slot ownership, complete mount metadata/costs, independent decoder and physical cursors, real hostile-size/truncation acceptance, bounded read/seek/accounting and remaining FILE/handle operations. Original actual-body failure proofs and separate native goldens accompany each step. Source steps have both zero-diagnostic PPC products; tests-only #201 references #200's unchanged product evidence. Codex's #200 physical-cursor and #201 incomplete streamed-cap findings are fixed/resolved with renewed clean reviews. Both streamed cap payloads now verify every byte through refills and stable EOF. #202 also has completed clean Codex. #203 is newly opened and its completed clean review remains required. Retail PK3/Mac OS 9 execution stays deferred.

Several required GitHub jobs remain queued; the table records the current mix of completed, running and queued checks. Do not infer a cause, repeatedly restart queues, or waive any gate. CodeRabbit was requested on every PR; the agreed hourly follow-up policy remains. Read every full formal review, including outside-diff findings, and query every thread after Codex completion immediately before any merge.

The ledger PR is omitted to avoid a self-referential stale SHA. Query its own exact head and current CI/review gates directly. This dated table records scheduling only.

| PR | Exact head | Successful Portable CI checks | Other jobs |
| --- | --- | ---: | --- |
| #171 | `067464edfec1d558e5af7bc375a170727d7d36ce` | 0/4 | QUEUED |
| #173 | `874f448410a67ae4d478830d042477f7fc3f7741` | 0/4 | QUEUED |
| #175 | `d9e3236a105ccb8e85459cf248e56334f2bd58da` | 4/4 | None |
| #176 | `ba2d426e85a7121321b3feeb0b524f17e437cf5d` | 4/4 | None |
| #177 | `a86892d86e04804548aef91aff8de62b1588b90e` | 4/4 | None |
| #178 | `5f9f557d5ab406bd439737b57d4db70162142b6f` | 4/4 | None |
| #179 | `5d9a47f4fb8a7bd0f9eb85b02914af595afb36db` | 4/4 | None |
| #181 | `494e70e6099d81b2d58f9ea8cbe27b5497aab8da` | 4/4 | None |
| #183 | `09c2b22ecbf182d4091d5e2e9d378c27b270ced9` | 4/4 | None |
| #184 | `f153e8944ea8bbe5a7a9e0096cf9cbc4b625ac7b` | 4/4 | None |
| #185 | `431c889f467c7eff0737360ece35e2a5b0f6e1d9` | 4/4 | None |
| #186 | `22cee573d7e6c3dfff95733d406c6e8d50c7d74d` | 4/4 | None |
| #187 | `942999b901493d972979b62c31c352f64c070b68` | 4/4 | None |
| #188 | `d9e4746a260589d6c9cf5a7d614784da02cf5ee2` | 2/4 | QUEUED |
| #189 | `efecb0331d36e148a3ed4e392d8804b1a5f91016` | 3/4 | QUEUED |
| #190 | `4b1bcaeae12c23c634cc97ee37dd9824a662ee03` | 2/4 | IN_PROGRESS, QUEUED |
| #191 | `527c05be4c800214ffbe2c69e5026496d3cdac77` | 3/4 | QUEUED |
| #192 | `79245e6ba5aaae9693ce07811d1cc6704e899a55` | 1/4 | IN_PROGRESS, QUEUED |
| #193 | `a0386e823b357b64c06ed8df15c53d05d1c7b147` | 2/4 | QUEUED |
| #194 | `90918bfc4d7c748a05ce5677e1ba632ff630fb21` | 0/4 | QUEUED |
| #195 | `3097d5a4044d80ec79d4cd56151fbb9561882e88` | 0/4 | QUEUED |
| #196 | `93b6bfa0bb5b83b0b31476283b3be5d24fdca855` | 0/4 | QUEUED |
| #197 | `58c89fee2abd0fa8a25aaa42e095fe73341d2582` | 0/4 | QUEUED |
| #198 | `39c23778b2502a914d3c09ad9ab3b40ea24a8aab` | 0/4 | QUEUED |
| #199 | `ed5407b9578bd1214c2fa438f4758b7b826a55ab` | 0/4 | QUEUED |
| #200 | `adac2c8da2d063db44f7c32c80469a5318790f7b` | 0/4 | QUEUED |
| #201 | `d683017fd4495c98ceb3c28fc7c8b83723d6096f` | 0/4 | QUEUED |
| #202 | `7c188f1f80a6f0a09c299f98800e213815b48005` | 0/4 | QUEUED |
| #203 | `1941de0c4cd215e2f8682810b8e20b75d5286f32` | 0/4 | QUEUED |
