# Memory explorer: data, interaction and design

The explorer shows the V44 board's physical memory allocations as a
disk-usage-style treemap. It is a **snapshot**, not a live RAM scanner.
The board log supplies free-heap observations; the build and source provide
the other allocations. See [MEMORY-LAYOUT.md](MEMORY-LAYOUT.md) for the
hardware explanation and output/memory-map-v44.json (local/private reference: `output/memory-map-v44.json`)
for the complete hierarchy.

## Using the map

Open the local preview at <http://127.0.0.1:8772/memory-preview.html> while
the preview server is running. The same visualization can render inline in
the conversation.

1. Choose **SRAM**, **PSRAM**, **Flash**, or **OTP**. SRAM opens first because
   its limited free heap is relevant to the current optimization work.
2. Click a grouped rectangle or a grouped row in **Allocation list** to open
   its breakdown. The table provides access to allocations too small to label
   or tap precisely in the treemap.
3. Click a leaf rectangle or row to display its exact byte count and source
   note. Hover/tap tooltips expose complete names where labels are shortened.
4. Use **Back** or a breadcrumb ancestor to move upward. Back is disabled
   at a chip's root.

Rectangle area is proportional to bytes **within the current view**. A
child's Share is its percentage of that view; the context line also gives
the group's share of its physical chip. Switching chips changes the scale:
the 520 KiB SRAM rectangle is not the same absolute scale as 8 MiB PSRAM.

## UI/UX changes

The first version placed four small maps beside one another. Small native
code and runtime allocations were hard to label, and the single long picker
made navigation cumbersome. The revised layout uses:

- One larger treemap with four compact memory tabs.
- Stable category colors for ROM/caches, native code, game/runtime data,
  video buffers, known free space and reserved/unmeasured areas.
- Wrapped rectangle labels, sizes and percentages where space permits.
- Breadcrumbs and an explicit Back action for hierarchical navigation.
- A sorted allocation table with exact sizes and percentages; it also offers
  native keyboard controls for every allocation.
- A selected-allocation detail containing bytes and the measurement note.
- Responsive layout and theme-derived colors rather than a fixed light or
  dark palette. Meaning is also expressed in labels, not only colors.
- Remembered view/selection in the conversation host, with no live board
  connection or network data fetching inside the visualization.

The hierarchy uses a deterministic binary split along the longer rectangle
dimension, sorted by descending allocation size. Layout recomputes from
the actual available width. Tiny cells retain accurate area; the list and
tooltips provide their readable details rather than enlarging their values.

## Data sources and accounting

`build/memory_map_v44.py` builds the hierarchy and inserts its JSON into
the visualization fragment. It verifies non-overlapping ELF function ranges
and exact physical totals:

| Chip | Capacity |
| --- | ---: |
| Internal SRAM | 532,480 bytes / 520 KiB |
| External PSRAM | 8,388,608 bytes / 8 MiB |
| External flash | 8,388,608 bytes / 8 MiB |
| Internal OTP | 8,192 bytes / 8 KiB |

Native code size uses the exact V44 function map. The flash view distinguishes
code executed from flash from the stored load image of code copied into
SRAM. Those copies occupy different physical memories and must both be
counted. Tables, initialized data, vector material and address padding are
kept separate from symbol-mapped executable code.

The PSRAM arena totals 7,673 KiB allocated + 7 KiB spare. The other 512 KiB
lies outside the project arena; its actual usage was not measured and is
not labeled free. OTP provisioning occupancy was not read either.

The SRAM map includes a residual **Other heap use + alignment** category.
This balances the verified allocations, main-stack reservation, scratch-bank
allocation and reported free heap. It is not a claim that each allocator or
driver object was independently measured. Stack reserves are capacity,
not measured peak stack consumption. The free heap sample is 11,088 bytes
in the latest retained V44 logs, not a guaranteed minimum.

## Allocator correction discovered during this work

The live 320 x 240 RGB565 HDMI framebuffer is **150 KiB of internal SRAM**.
The display driver's Arduino path calls ordinary `malloc()`; the installed
Arduino core's `__wrap_malloc()` routes to the internal allocator.
External `pmalloc()` is a separate API. The explicit 150 KiB HDMI recovery
backup is in the project PSRAM arena. Earlier descriptions placing the
live display buffer in PSRAM were corrected in the architecture documents.

This distinction matters: the display itself consumes 28.8% of internal
SRAM. A second 98 KiB video-memory image cannot fit in the observed free
heap, and moving active display/renderer storage to PSRAM would require
its own correctness and bandwidth evaluation.

## Reproduction and verification

The editable inline fragment is in the thread's writable visualization
folder as `rp2350-memory.html`. Rebuild its data with:

```powershell
python <workspace>/build/memory_map_v44.py
```

The visualization skill's `scripts/render.py` wraps that fragment into
`<workspace>/build/memory-preview.html`; use `--force` to replace the
existing preview. The local server binds to loopback only:

```powershell
python -m http.server 8772 --bind 127.0.0.1 --directory <workspace>/build
```

Checks cover the four exact capacity totals, ELF function-range overlap,
browser loading, chip switching, grouped native-code navigation and returning
to the SRAM root. The UI does not write to the Feather, flash, SRAM or OTP.
