# Pastry Shop Simulator — Algorithms & Data Structures Final Project

![C11](https://img.shields.io/badge/C-C11-00599C?logo=c)
![Build](https://img.shields.io/badge/build-gcc%20--O2%20--Wall%20--Werror-success)
![Tests](https://img.shields.io/badge/public%20tests-12%2F12%20passing-success)
![Grade](https://img.shields.io/badge/grade-30%2F30-brightgreen)
![License](https://img.shields.io/badge/license-MIT-blue)

A discrete-event simulation of an industrial pastry shop — inventory with perishable batches, order scheduling and
courier logistics — written in **1,450 lines of plain C11, no dependencies beyond libc**.

Final project for *Algoritmi e Principi dell'Informatica* at **Politecnico di Milano** (2023-24). The grade is
awarded purely on **execution time and peak memory** against a reference test suite, which makes it an exercise in
engineering for constant factors, not just asymptotics.

**Result: 30/30** — the top band, cleared with roughly 2× headroom on time and memory.

---

## Results

Measured locally on the largest public test cases (`-O2`, Apple Silicon), peak RSS via `/usr/bin/time -l`:

| Test case | Input size | Time | Peak memory | Output |
|---|---:|---:|---:|---|
| `open9` | 24 MB | **2.07 s** | **10.2 MiB** | ✅ byte-identical |
| `open10` | 16 MB | **2.19 s** | **9.4 MiB** | ✅ byte-identical |
| `open11` | 17 MB | **0.43 s** | **8.5 MiB** | ✅ byte-identical |

The official grader assigns a band only when **both** thresholds are met at once:

| Grade | Memory limit | Time limit | Cleared |
|---|---|---|---|
| 18 | 35 MiB | 14 s | ✅ |
| 21 | 30 MiB | 11 s | ✅ |
| 24 | 25 MiB | 9 s | ✅ |
| 27 | 20 MiB | 6 s | ✅ |
| **30** | **15 MiB** | **4 s** | **✅** |

On the grader the submitted version ran in about **2 seconds** and stayed **under 14 MiB**, comfortably inside the
top band on both axes. All 12 public test cases produce byte-identical output and the binary compiles clean under `-Wall -Werror`.
Memory behaviour was tracked with Valgrind during development, since peak usage is half the grade.

---

## Quickstart

```bash
gcc -Wall -Werror -std=gnu11 -O2 main.c -o proapi -lm
./run_tests.sh            # runs all 12 public test cases, diffs against expected output
```

Running a single case, reading from `stdin` and writing to `stdout`:

```bash
./proapi < tests/open4.txt | diff - tests/open4.output.txt && echo PASS
```

Reproducing the performance numbers:

```bash
/usr/bin/time -l ./proapi < tests/open9.txt > /dev/null     # macOS
/usr/bin/time -v ./proapi < tests/open9.txt > /dev/null     # Linux
```

The project was developed in **CLion** with CMake ([`CMakeLists.txt`](CMakeLists.txt),
[`compiler_flag.cmake`](compiler_flag.cmake)) and debugged with **Valgrind**.

---

## The problem

A discrete-time simulation: each command read from `stdin` consumes exactly one time instant, starting at `t = 0`.

- **Recipes** map ingredient names to quantities in grams.
- The **warehouse** stocks each ingredient as a list of **batches**, each with a quantity and an expiration instant.
  A batch is unusable once `expiration <= now`.
- **Orders** are prepared immediately if stock suffices, always drawing from the **batches closest to expiration
  first**. Otherwise the order waits, and every restock re-evaluates the waiting orders in arrival order.
- A **courier** arrives every `n` instants with a fixed capacity in grams. It takes ready orders **chronologically**,
  stopping at the first that does not fit, then loads them **by decreasing weight** (ties broken chronologically).

Names use the alphabet `a-z A-Z 0-9 _`, up to 255 characters; all quantities fit in 32 bits. Command keywords and
output strings are part of the assignment's I/O contract, so they remain in Italian.

| Command | Meaning | Output |
|---|---|---|
| `aggiungi_ricetta <recipe> <ingr> <qty> ...` | add a recipe | `aggiunta` / `ignorato` |
| `rimuovi_ricetta <recipe>` | remove a recipe | `rimossa` / `ordini in sospeso` / `non presente` |
| `rifornimento <ingr> <qty> <expiration> ...` | restock batches | `rifornito` |
| `ordine <recipe> <count>` | place an order | `accettato` / `rifiutato` |

On every courier pass the program prints one line per loaded order — `<order_instant> <recipe> <count>` — or
`camioncino vuoto`.

<details>
<summary><b>Worked example</b> (click to expand)</summary>

```
in                                                    out           t
5 325                       ← courier: every 5 instants, 325 g
aggiungi_ricetta torta farina 50 uova 10 zucchero 20  aggiunta       0
rimuovi_ricetta sfogliatella                          non presente   1
rifornimento farina 100 10 uova 100 10 zucchero 100 10  rifornito    2
ordine torta 1                                        accettato      3
                                                                     ...
                                                      5 torta 1      ← printed at t = 5, before the command
```

The order placed at `t = 3` weighs `50 + 10 + 20 = 80 g`, fits the van, and is shipped on the courier's first pass.
</details>

---

## Design

Everything lives in one translation unit, [`main.c`](main.c). The design target was not asymptotic elegance but
**cache behaviour and allocation count**, since those dominate at this input size.

```
RecipeHashTable  ──► Recipe*  (open addressing, linear probing, lazy deletion)
                       │
                       └─ ingredient list (Node, sorted by name)
                             │
                             └─ Item**  ──┐  direct pointer into the warehouse
                                          │
HashTable        ──► Item*  ◄─────────────┘  (open addressing, linear probing)
                       │
                       └─ batch list, sorted by ascending expiration

waitQueue   ──► waiting orders, chronological
readyQueue  ──► ready orders, chronological
vanQueue    ──► loaded orders, decreasing weight (built per courier pass)
```

### Key decisions

| Problem | Approach | Payoff |
|---|---|---|
| Ingredient lookup dominates every order | Recipe ingredient nodes hold an `Item**` straight into the warehouse cell | Order preparation does **zero hashing and zero string comparison** |
| Must draw from the soonest-expiring batch | Batch lists kept sorted by expiration at insertion time | Draw is a head-to-tail scan; no sorting at order time |
| Preparation must be all-or-nothing | Tentative `quantityLeft` + a global attempt counter | **No state copy and no explicit rollback** on failure |
| Every restock re-checks the whole waiting queue | Cache the failure reason on the recipe per instant | Repeat orders of the same recipe skip the warehouse entirely |
| Linear probing forbids physical deletion | Lazy `isDeleted` flags with cell recycling | Probe chains stay intact; depleted ingredients cost no malloc on return |

### Why these work

**`Item**` inside the ingredient node.** A recipe's ingredient node does not store the ingredient *name* — it stores
a pointer to the warehouse hash cell, wired once in `insertItemInHashTable`. Preparing an order therefore jumps
straight to the stock, skipping the hash and `strcmp` that would otherwise be paid per ingredient per order. It is a
*double* pointer on purpose: `resize` reallocates the cell array, and the indirection keeps every recipe's pointers
valid across a rehash.

**Rollback-free two-phase draw.** Checking an order means walking all its ingredients, but stock may only be
consumed if *all* of them are available. Rather than two full passes or a state copy, each `Batch` carries its
committed `quantity`, a tentative `quantityLeft`, and the `lastTimeModified` stamp of the attempt that touched it.
A failed attempt needs no cleanup: the next attempt sees a stale stamp and realigns `quantityLeft` on its own. A
successful one calls `fixHashTable`, which commits values and drops depleted or expired batches while walking only
that recipe's ingredients. Net effect: one pass, no copies, no undo log.

**Waiting-queue pruning.** When an order for recipe `R` fails at instant `t`, the *reason* is recorded on the recipe
itself — `state = -1` for a missing ingredient, `state = requested quantity` for insufficient stock. Later orders of
the same recipe in the same instant, with an equal or larger quantity, are provably doomed and are skipped without
touching the warehouse. On the grader's inputs, where a handful of recipes are ordered thousands of times, this
removes the dominant share of the work.

**Open addressing over chaining.** Both tables are contiguous arrays of pointers with linear probing, initial size
5000, doubling past a 0.7 load factor. No per-node collision lists means fewer allocations and far better locality
than a bucket-chained table.

**Allocation-free hot path.** Parsing uses `strtok` over a single static command buffer, and command dispatch
compares an integer hash of the keyword instead of chaining `strcmp` calls. Order preparation mutates batches in
place. The only allocation per command is the `Order` or `Batch` node that genuinely has to exist.

### Complexity

With `k` ingredients per recipe and `b` live batches per ingredient:

| Operation | Cost |
|---|---|
| `aggiungi_ricetta` | `O(k)` amortized |
| `rimuovi_ricetta` | `O(1)` amortized |
| `rifornimento` (per batch) | `O(b)` ordered insertion |
| `ordine` | `O(k · b)`, with no hashing over ingredients |
| courier pass | `O(m)` selection, `O(m log m)` worst case for weight ordering |

Table growth is `O(n)` per resize, `O(1)` amortized per insertion.

---

## Testing and debugging

[`tests/`](tests) holds the 12 public cases — every `openN.txt` input with its expected `openN.output.txt`, from a
few kilobytes up to 24 MB. [`run_tests.sh`](run_tests.sh) builds and diffs all of them in one command.

Memory correctness was checked with Valgrind throughout, since peak memory is half the grade:

```bash
valgrind --leak-check=full --show-leak-kinds=all ./proapi < tests/open4.txt > /dev/null
```

Every structure is explicitly torn down at exit (`freeHashTable`, `freeRecipeInHashTable`, `freeOrders`,
`freeBatch`, `freeIngredientList`) — partly hygiene, mostly so that Valgrind's numbers stay meaningful.

---

## What I would change

Written under exam constraints, with hindsight worth stating:

- **Split the translation unit.** A single 1,450-line `main.c` was convenient for the grader and is the wrong shape
  for a maintained project; the warehouse, catalog and queues each want their own module and header.
- **Replace the sorted batch list with a small binary heap.** Insertion is `O(b)` today; a heap makes both insertion
  and the minimum-expiration draw `O(log b)`, which would matter for inputs with many live batches per ingredient.
- **Formalise the tentative-draw protocol.** The `quantity` / `quantityLeft` / `lastTimeModified` triple is fast and
  correct, but it is an implicit transaction spread across several functions — it deserves an explicit API and unit
  tests rather than comments.
- **Add property-based tests.** The public cases are end-to-end diffs; a generator producing random command streams
  against a naive reference implementation would catch edge cases the fixed suite cannot.

---

## Repository layout

```
main.c                  complete implementation (single translation unit, C11)
run_tests.sh            builds and runs the full public test suite
CMakeLists.txt          CMake configuration (target Progetto_API)
compiler_flag.cmake     compiler flags used for grading
tests/                  public test cases: openN.txt inputs + openN.output.txt expected outputs
specifica.pdf           official assignment text (Italian)
LICENSE                 MIT license
```

---

## License

Released under the MIT License. See [LICENSE](LICENSE).
