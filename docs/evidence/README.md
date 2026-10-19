# Text evidence from the local experiment

The JSON files and `.log` files here are numerical/text records of local builds and tests. They contain no ROM payload or redistributed emulator implementation. Compiled images and old source ZIPs remain private and are not linked as downloadable artifacts.

Start with [v24 summary](performance-v24-summary.json), [v24 native test log](sonic-v24-native-test.log), [v23 recovery summary](recovery-v23-summary.json) and [v21 summary](sonic-v21-summary.json). The [development history](../development-history.md) supplies the later hardware outcomes for older builds.

Many historical summaries were written immediately after compilation and still say `hardware_tested: false` or "awaiting test." They are build-time snapshots, not a complete current hardware ledger. Later hardware observations in the history and current v24 summary supersede those pending-status fields. Similarly, later corrections supersede the original transient 7 FPS report.

The v24 summary has been updated with the author's confirmation of hardware operation and the latest report: about 5 FPS, VBskip 15-21%, Draw 46 ms/frame and Copy 10 ms/frame. Exact scene and Run timing were not supplied. Measurements are author reports, not an independently instrumented benchmark.

Native tests compare paths within the same experimental core. Their PASS lines establish the described regression comparisons, not original-hardware accuracy, full gameplay completion or board FPS. Framebuffer hashes identify local test results without distributing game images.
