# CSAPP Gap Next Plan

This note records the next work plan after comparing the current CPU volume 2
with CS:APP-scale systems material. It is a project plan, not final book prose.

## Current Baseline

- Current generated PDF: 371 pages.
- Current official count:
  - chapters: 253,634 units, including 235,154 CJK characters and 18,480 English words.
  - full book: 263,696 units, including 244,279 CJK characters and 19,417 English words.
- The counter excludes code/listings/verbatim/inline code macros.
- Current book already has the main conceptual spine, but it is still much
  smaller than a CS:APP-scale system text and does not yet have an equally
  strong lab/code system.

## Gap Assessment

CS:APP is not just a thick book. Its strength comes from the combination of:

- a large body of tightly edited prose;
- chapter concepts derived from executable code and machine evidence;
- a full lab sequence;
- repeatable experiments and grading-style checks;
- multiple editions of course feedback.

Volume 2 should not imitate CS:APP chapter by chapter, because volume 1 already
covers many foundations and volume 2 focuses on modern CPU hardware, concurrency,
runtime, I/O, and distributed computation. The right target is to match the
depth pattern: concrete failure, code shape, assembly or kernel/hardware path,
measurement, protocol, edge cases, and engineering boundary.

## Size Targets

Use the repository script as the official count:

```bash
python3 books/cpu-volume-2/source/latex/scripts/count_text_units.py --chapters-only
python3 books/cpu-volume-2/source/latex/scripts/count_text_units.py
```

Near-term target:

- principle volume chapters: 320,000 to 360,000 units;
- generated PDF: roughly 450 to 520 pages, depending on diagrams and tables;
- no padding: growth must come from cases, experiments, failure sequences,
  source-reading paths, and implementation boundaries.

Longer target:

- principle volume chapters: 400,000 to 500,000 units;
- code practice volume: 150,000 to 250,000 units plus a complete C++20 project;
- together: 600,000 to 750,000 units of systems material, with executable labs.

## Principle Volume Expansion Priorities

### 1. Hardware Chapters

Primary files:

- `ch02-core-pipeline-ooo-branch.tex`
- `ch03-cache-tlb-virtual-memory.tex`
- `ch04-numa-interconnect-coherence.tex`

Required expansion:

- add more assembly-centered walkthroughs for I-cache, branch prediction,
  uop cache, ROB, reservation stations, LSQ, store buffer, and precise
  exceptions;
- connect DTLB, page walk, first-touch, mmap, and page cache to concrete
  C++ access patterns;
- add MESI/MOESI state transition timelines for atomic RMW, false sharing,
  lock handoff, and queue metadata;
- add PMU Top-Down reading examples: retiring, bad speculation, frontend bound,
  backend bound;
- add experiments that still work when privileged counters are unavailable.

Acceptance:

- each hardware concept must be derived from a code or assembly shape;
- every performance claim must include a measurement fallback and a boundary;
- hardware chapters should be able to support later atomics and runtime chapters
  without hand-waving.

### 2. Concurrency And Memory Model

Primary files:

- `ch10-locks-condition-semaphores.tex`
- `ch11-atomics-memory-order.tex`
- `ch12-lock-free-data-structures.tex`

Required expansion:

- extend condition-variable examples with futex-style state checks, lost wakeup
  timelines, spurious wakeups, notify-one/all decisions, timeout/cancel/close
  distinctions, and capacity-one queue fault injection;
- expand litmus tests: message passing, store buffering, load buffering, IRIW,
  publication chains, seqlock snapshots, CAS failure memory order, and fence
  review rules;
- deepen x86 TSO versus ARM weak memory through C++ contracts, not platform
  folklore;
- expand memory reclamation: hazard pointer protocol, epoch stalls, RCU grace
  periods, object-pool generation handles, ABA, scan cost, and instrumentation;
- add TSAN/ASAN/stress/linearizability testing strategy for every candidate
  data structure.

Acceptance:

- no atomic example should be presented without a failed alternative and a
  clear synchronization edge;
- no lock-free example should omit lifecycle and reclamation;
- every waiting protocol should name its state, predicate, notification source,
  and shutdown path.

### 3. Runtime, I/O, And Backpressure

Primary files:

- `ch13-reduce-scan-partition.tex`
- `ch14-task-runtime-work-stealing.tex`
- `ch15-io-async-backpressure.tex`

Required expansion:

- connect parallel algorithms to assembly and cache behavior: count/scan/write,
  partition ownership, prefix sums, stable filter, and merge trees;
- deepen work-stealing deque internals: head/tail ownership, final-element CAS,
  release/acquire publication, cancellation, work-first/help-first tradeoffs,
  and stealing metrics;
- expand Linux I/O path: fd table, VFS, page cache, address_space, writeback,
  block layer, fsync/fdatasync, rename crash matrix, directory fsync, direct I/O,
  dirty throttling, cgroup writeback, io_uring SQ/CQ ownership, registered
  buffers, cancellation, and short-write taxonomy.

Acceptance:

- I/O correctness must distinguish kernel acceptance, page cache state,
  durable persistence, manifest commit, and business visibility;
- runtime metrics must explain tail latency, not only throughput;
- all backpressure examples must include bounded resources and rejection or
  waiting semantics.

### 4. Distributed Chapters

Primary files:

- `ch16-distributed-system-model.tex`
- `ch17-partition-replication-consensus.tex`
- `ch18-distributed-compute-engineering.tex`

Required expansion:

- deepen control-plane versus data-plane separation;
- expand stale owner, stale leader, stale epoch, duplicate attempt, late reply,
  orphan block, corrupt block, and checkpoint mismatch timelines;
- add quorum intersection reasoning, leader term, log index, commit index,
  apply index, prevLogIndex/prevLogTerm intuition, membership change safety,
  tombstone retention, and snapshot as state-machine prefix;
- add read contracts: stale, monotonic, read-your-writes, linearizable reads,
  lease assumptions, and stale read hazards;
- add deterministic failure-injection matrix and recovery scanner classification.

Acceptance:

- reduce must read manifest facts, never directories;
- exactly-once-like behavior must be explained as idempotent commit plus dedup,
  not as magical single execution;
- recovery must classify every observed artifact before accepting it.

## Code Practice Volume Plan

Primary directory:

```text
books/compute-systems-engine-code/
```

The code practice volume must become the lab counterpart of the principle
volume. It should provide a complete C++20 project with tests, benchmarks, and
reports.

Required modules:

- input generator, parser, `RecordId`, `InputSplit`, reference engine, and
  diagnostics;
- `HotBatch`, field encoding, hot/cold layout, benchmark harness, and
  `HotKernelReport`;
- cache/TLB experiments, branch prediction experiments, and assembly report
  snippets;
- thread pool, bounded queue, condition-variable tests, `QueueContract`;
- atomic protocols, SPSC ring, optional MPMC candidate, `AtomicProtocolNote`;
- lock-free lifecycle experiments, hazard/epoch model, and
  `LockFreeLifecycleReport`;
- parallel count/scan/write, partition manifest, deterministic reduce, top-k;
- work-stealing runtime with metrics and shutdown semantics;
- reliable writer, short write simulation, buffer pool, completion queue,
  checkpoint, manifest commit;
- in-memory `MessageBus`, `MessageEnvelope`, retry budget, attempt id, epoch,
  stale reply handling, failure injection, recovery scanner, and `RunReport`.

Required validation:

- unit tests for state transitions and failure cases;
- stress tests for concurrency paths;
- deterministic fault injection rules;
- benchmark scripts with fixed input families;
- generated reports checked into `results/` only as curated examples;
- CI-friendly command sequence.

## Immediate Next Three Milestones

### Milestone A: Lab Skeleton And Report Contracts

Deliver:

- expand `books/compute-systems-engine-code/README.md` with the command matrix;
- create or reorganize C++ project skeleton if missing;
- define report structs and JSON/text output for:
  - `HotKernelReport`
  - `MemoryShape`
  - `QueueContract`
  - `AtomicProtocolNote`
  - `LockFreeLifecycleReport`
  - `PartitionRunReport`
  - `RunReport`

Done when:

- project builds;
- tests run;
- one tiny input produces reference output and a stable report.

### Milestone B: Hardware And Concurrency Lab Pair

Deliver:

- branch prediction input families;
- cache/TLB access pattern experiments;
- mutex queue versus condition-variable bounded queue;
- store-buffer and message-passing litmus experiments where supported;
- SPSC ring with misuse tests;
- report sections that tie source, assembly, counters or fallback timing.

Done when:

- the principle volume can cite these experiments as runnable evidence;
- every experiment has correctness output and boundary notes.

### Milestone C: Manifest, Retry, And Recovery Spine

Deliver:

- logical task and attempt model;
- reliable writer with short-write injection;
- manifest append-if-absent;
- late reply and stale attempt handling;
- checkpoint that cannot override manifest;
- recovery scanner that classifies accepted, candidate, orphan, stale, corrupt.

Done when:

- reduce reads only manifest entries;
- deterministic fault rules reproduce duplicate reply, lost reply, short write,
  stale epoch, and driver restart cases;
- `RunReport` explains all accepted and rejected artifacts.

## Non-Negotiable Style Rules

- Do not add word-count padding.
- Do not leave meta commentary in final chapters.
- Do not use slogans about quality inside the book.
- Each added concept must be forced by a concrete failure, machine fact, code
  path, measurement, or recovery case.
- Prefer the chain:

```text
C++ semantics -> assembly/machine shape -> hardware/runtime/distributed structure
-> evidence/experiment -> engineering boundary
```

- Keep generated PDF valid after every meaningful expansion.
- Export final artifacts to:

```text
book-exports/从C++到计算系统第二册
```
