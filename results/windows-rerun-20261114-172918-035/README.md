# Windows rerun, 2026-11-14

The original `experiments/Run-Experiment.ps1` ran the reconstructed Race, Critical, Atomic, and Reduction snapshots in that order on Windows 10.0.26300 (Intel Core i7-10510U, eight logical processors), using MSYS2 UCRT64 GCC 16.2.0 (Rev4) with `-std=c++17 -O2 -fopenmp`. Each variant ran five times at each requested team size of 2, 4, and 8, in ascending order, with `OMP_DYNAMIC=FALSE`. The script records the requested team size. It does not instrument the actual team size.

The snapshots share the 400 × 225 scene at 10 samples per pixel and a maximum ray depth of 20. Scene generation uses seed 42, and each pixel reseeds a thread-local MT19937 with `42u ^ (pixel_id * 0x9e3779b9u)`. The steady-clock measurement covers the dynamically scheduled row loop, including the pixel counter and OpenMP synchronization. Scene setup, image allocation, and PPM output fall outside that interval.

- [Per-run CSV](runs.csv) records all 60 reported render times, completed counts, and SHA-256 values. [Logs](logs) contains the corresponding raw stderr.
- [Comparison CSV](comparison.csv) compares the arithmetic mean of each five-run group against [the historical reported mean](../reported-means.csv). Deltas are fresh minus historical. The historical measurements came from earlier source versions. Their individual timings and count logs are unavailable, so the differences are descriptive and do not isolate the effect of synchronization.
- The raw hash covers the bytes emitted in this batch. The archival hash recodes the PPM as UTF-16LE with BOM and CRLF for comparison to the [archived reference](../archived-image-hashes.csv). The archived Race batches differ from that reference. Reconstructed Race uses the same pixel RNG and disjoint image writes as the corrected snapshots, with an unsynchronized shared count.

All 45 synchronized runs reported 90,000 pixels. All 15 Race runs undercounted. Every PPM contained 90,000 pixel rows and shared raw SHA-256 `ED2207EC4D95A5515D94718DF2DA49EE8F0FF9DB48332F70FF31CBE087C181A7`. All 60 archival-encoding hashes matched the archived reference.

Critical at eight threads had the lowest fresh mean (2.550236 s). Reduction averaged 4.156028 s at four threads and 4.959246 s at eight, respectively 34.37% and 52.09% above the historical means. The variants ran in fixed order with Reduction last. Run order, machine conditions, and the different historical sources limit causal interpretation.
