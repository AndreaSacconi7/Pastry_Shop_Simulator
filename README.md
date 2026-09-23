# Progetto API — Final Project, Algorithms and Data Structures (2023-2024)

Final project for the **Algoritmi e Principi dell'Informatica** (API) course, Politecnico di Milano, academic year
2023-2024.

**Final grade: 30/30** — top score, achieved by landing inside the tightest time and memory band.

---

## Grading

The grade depends exclusively on **execution time** and **memory usage** on the large test cases: a band is awarded
only when **both** thresholds are met at the same time.

| Grade | Memory | Time | Result |
|---|---|---|---|
| 18 | 35 MiB | 14 s | ✅ |
| 21 | 30 MiB | 11 s | ✅ |
| 24 | 25 MiB | 9 s | ✅ |
| 27 | 20 MiB | 6 s | ✅ |
| **30** | **15 MiB** | **4 s** | **✅** |

Every threshold cleared, up to the top band: **30/30**. The submitted version ran the grader's test cases in about
**2 seconds**, using **under 14 MiB** of memory — comfortably inside the top band on both axes.

---

## The problem

A discrete-time simulation of an industrial pastry shop. Every command read from `stdin` consumes one time instant;
the simulation starts at `t = 0` and ends with the last input line.

Entities in the simulation:

- **Ingredients**, identified by a name (alphabet `a-z A-Z 0-9 _`, up to 255 characters).
- **Recipes**, also identified by a name, made of an arbitrary set of (ingredient, quantity in grams) pairs.
- **The warehouse**, which stocks ingredients as **batches**, each with a quantity and an expiration date.
- **Customer orders**, prepared immediately if stock suffices, otherwise put on hold.
- **The courier**, who shows up every `n` instants with a fixed capacity in grams and picks up the ready orders.

Key rules:

- Preparing an order always draws from the **batches closest to expiration first**.
- A batch is expired (and therefore unusable) when `expiration <= current_time`.
- If stock is insufficient, the order goes into a waiting queue. On every `rifornimento` the waiting orders are
  re-evaluated **in chronological order of arrival**.
- The weight of one pastry is the sum of the gram quantities of its ingredients; an order's weight is
  `recipe_weight * number_of_items`.
- When the courier arrives, ready orders are picked **chronologically**, stopping at the first one that exceeds the
  remaining capacity; they are then loaded **by decreasing weight**, ties broken chronologically.

### Input commands

The first line holds two integers: the courier's periodicity and capacity. The commands follow, one per line.

| Command | Format | Output |
|---|---|---|
| `aggiungi_ricetta` | `aggiungi_ricetta <recipe> <ingr> <qty> ...` | `aggiunta` / `ignorato` |
| `rimuovi_ricetta` | `rimuovi_ricetta <recipe>` | `rimossa` / `ordini in sospeso` / `non presente` |
| `rifornimento` | `rifornimento <ingr> <qty> <expiration> ...` | `rifornito` |
| `ordine` | `ordine <recipe> <number_of_items>` | `accettato` / `rifiutato` |

On every courier pass (before executing the command at time `k*n`) the program prints one triple per loaded order —
`<order_instant> <recipe> <number_of_items>` — or `camioncino vuoto` if the van is empty.

Command keywords and output strings are part of the assignment's I/O contract, so they stay in Italian.

---

## How the implementation works

The whole solution lives in a single C11 file, [`main.c`](main.c), with no dependencies beyond libc. The design was
driven by minimizing both running time and dynamic allocation, since those are the only two grading criteria.

### Data structures

```
RecipeHashTable  ──► Recipe*  (open addressing, linear probing, lazy deletion)
                       │
                       └─ List of Node (ingredients, sorted by name)
                             │
                             └─ Item**  ──┐  direct pointer to the warehouse cell
                                          │
HashTable        ──► Item*  ◄─────────────┘  (open addressing, linear probing)
                       │
                       └─ Batch list (batches sorted by ascending expiration)

Queue waitQueue   ──► waiting orders, chronological
Queue readyQueue  ──► ready orders, chronological
Queue vanQueue    ──► loaded orders, decreasing weight (temporary, for printing)
```

**`HashTable` (the warehouse).** An **open addressing** hash table with **linear probing**, initial size 5000,
doubling whenever the load factor exceeds 0.7. Each cell (`Item`) holds the ingredient name and the list of its
batches. The container is a contiguous array of pointers: no collision-list overhead and much better cache locality
than chaining.

**Batch list sorted by expiration.** Every `Item` keeps its `Batch` nodes sorted by ascending expiration, so drawing
stock — which must favor the batches closest to expiration — is a plain head-to-tail scan, with no sorting at order
time. `rifornimento` performs the ordered insertion in a single pass.

**`RecipeHashTable` (the recipe catalog).** Same structure as the warehouse. Each recipe's ingredients are kept in a
list sorted by name, so the ingredient scan is deterministic and comparable.

**The trick that makes the difference: `Item**` inside the ingredient node.** Each `Node` in a recipe's ingredient
list does not store the ingredient *name*, but a **pointer to the warehouse hash table cell** (`Item** item`,
assigned in `insertItemInHashTable`). As a result, preparing an order requires **no hashing and no string
comparison at all** — it jumps straight to that ingredient's stock. It is a double pointer precisely because the
table can be resized: `resize` reallocates the cell array, but the pointers held by the recipes stay valid.

**Order queues.** Singly linked lists with `head` and `tail`:

- `waitQueue` — waiting orders, naturally chronological (append at the tail, `O(1)`).
- `readyQueue` — ready orders, inserted in arrival-time order.
- `vanQueue` — built on each courier pass to reorder the selected orders by **decreasing weight** (ties stay
  chronological, because insertion only triggers on a strict `<`).

### Optimizations

**Lazy deletion (`isDeleted`).** Neither removed recipes nor depleted ingredients are ever physically erased from the
table: the cell is just flagged `isDeleted`. This is mandatory with linear probing (physical removal would break the
probe chains) and it avoids repeated malloc/free: if the same ingredient comes back in a later supply, the cell is
recycled.

**Two-phase draw with rollback-free semantics (`quantity` / `quantityLeft` / `lastTimeModified`).** Checking whether
an order can be prepared means walking all of its ingredients, but stock must be drawn **only if all of them** are
available. Instead of doing two full passes or copying state, every `Batch` carries:

- `quantity` — the real, committed quantity;
- `quantityLeft` — the *tentative* quantity during the check in progress;
- `lastTimeModified` — the "timestamp" (global `lastTimeModified` counter, bumped on every attempt) of the attempt
  that touched this batch.

If the attempt fails, nothing needs to be restored: on the next attempt the timestamp no longer matches and
`quantityLeft` is realigned to `quantity`. If it succeeds, `fixHashTable` walks only that recipe's ingredients and
**commits** the values, dropping depleted or expired batches along the way. In practice: **no copy, no explicit
rollback, a single pass in the worst case.**

**Pruning the waiting queue (`Recipe.state` / `Recipe.lastTimeUpdated`).** Every `rifornimento` would in principle
require re-evaluating the entire waiting queue. Instead, when an order for recipe `R` fails at instant `t`, the
*reason* is recorded on the recipe: `state = -1` (ingredient missing entirely) or `state = requested quantity`
(insufficient stock). Later orders for the same recipe at the same instant, with an equal or larger quantity, are
skipped without even querying the warehouse — they cannot possibly succeed. On realistic inputs, where the same
recipe is ordered many times, this cuts away the dominant share of the work.

**Command dispatch via string hash.** `UTILS_hashString` sums the command's bytes and the `switch` compares an
integer (`aggiungi_ricetta_HASH`, `rimuovi_ricetta_HASH`, `rifornimento_HASH`, `ordine_HASH`) instead of chaining
`strcmp` calls. Parsing uses `strtok` over a static command buffer, so reading input allocates nothing.

**`hash_strcmp`.** A string comparison that computes a 32-bit hash first and falls back to `strcmp` only when the
hashes collide: in the common case (different strings) it skips the character-by-character comparison.

**No auxiliary structures in the hot loop.** Order preparation allocates nothing: it works in place on batches
already in memory. The only allocation per command is the `Order` or `Batch` node actually needed.

### Complexity

With `k` = number of ingredients in the recipe and `b` = number of live batches per ingredient:

| Operation | Cost |
|---|---|
| `aggiungi_ricetta` | `O(k)` amortized (hash + ordered ingredient insertion) |
| `rimuovi_ricetta` | `O(1)` amortized (lookup + `isDeleted`) |
| `rifornimento` (per batch) | `O(b)` ordered insertion by expiration |
| `ordine` | `O(k * b)`, with no hashing over ingredients thanks to `Item**` |
| courier pass | `O(m)` over the loaded orders, plus `O(m log m)` worst case for the weight ordering |

The hash table `resize` is `O(n)`, but amortized `O(1)` per insertion thanks to the doubling strategy.

---

## Build and run

The project was developed in **CLion** and compiled with **gcc**.

With CMake:

```bash
cmake -S . -B build && cmake --build build
```

Or directly, with the flags used for grading ([`compiler_flag.cmake`](compiler_flag.cmake)):

```bash
gcc -Wall -Werror -std=gnu11 -g3 -O2 main.c -o proapi -lm
```

The program reads from `stdin` and writes to `stdout`:

```bash
./proapi < tests/open4.txt > out.txt
```

Comparing against the expected output:

```bash
diff <(./proapi < tests/open4.txt) tests/open4.output.txt
```

---

## Testing and debugging

- **Public test cases**: the [`tests/`](tests) folder holds every input (`openN.txt`) with its expected output
  (`openN.output.txt`), from the small cases (`open1`, plus the `example` from the assignment) up to the
  tens-of-megabytes ones (`open9`, `open10`, `open11`) used to measure time and memory.
- **Valgrind** for debugging: used both for memory correctness (memcheck) and to confirm the absence of leaks, since
  the score depends directly on peak memory.

```bash
valgrind --leak-check=full --show-leak-kinds=all ./proapi < tests/open4.txt > /dev/null
```

On termination the program explicitly frees every structure (`freeHashTable`, `freeRecipeInHashTable`, `freeOrders`,
`freeBatch`, `freeIngredientList`) — not only out of hygiene, but because it keeps the Valgrind measurements clean
and verifiable.

---

## Repository layout

```
main.c                  complete implementation (single translation unit)
CMakeLists.txt          CMake configuration (target Progetto_API)
compiler_flag.cmake     compiler flags used for grading
specifica.pdf           official assignment text (Italian)
tests/                  public test cases: openN.txt inputs + openN.output.txt expected outputs
README.md               this file
LICENSE                 MIT license
```

---

## License

Released under the MIT License. See [LICENSE](LICENSE).
