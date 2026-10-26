# Direct-Flash ROM provider experiment

Original board-support implementation and minimal instruction-line callback patch. Historical CompressedRom class name is retained for integration compatibility; this variant expects a complete uncompressed ROM image in Flash. It returns immutable pointers only for aligned complete256-byte instruction lines, preserving checked fallback at a short tail.

The generated ROM index/blob and emulator core API are intentionally excluded. This folder is not independently buildable and must not be paired with compressed ROM data. No game/BIOS bytes or firmware are included. See ../../GBAPrototype/PERFORMANCE-2026-10-26.md for matched hardware results and limitations. The normal compressed provider already had an instruction-line callback.
