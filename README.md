# Parallel-RayTracing-ARTI503

OpenMP parallel ray tracing: race conditions, synchronization techniques, and performance analysis.

The renderer processes a 400 × 225 image at 10 samples per pixel. It times the rendering loop with `std::chrono::steady_clock` and writes the P3 image to stdout afterward. Pixel counts and render time go to stderr. The parallel variants dynamically schedule rows, store colors in disjoint image slots, and seed a thread-local RNG by pixel index.

| Snapshot | Pixel counter | Source provenance |
| --- | --- | --- |
| Sequential | Single-thread increment | Copied from local `camera.h` |
| Race | Unsynchronized shared increment | New controlled counter-race implementation derived from Sequential |
| Critical | Named OpenMP critical section | New implementation derived from Sequential |
| Atomic | OpenMP atomic update | New implementation derived from Sequential |
| Reduction | OpenMP sum reduction | New implementation derived from Sequential |
| Lock | OpenMP lock around the increment | Copied from local `camera_lock_fixed.h` |

Each directory under `snapshots/` contains its own sources and CC0 notice. The exact historical Race, Critical, Atomic, and Reduction camera sources were unavailable in the inspected project and clone. The Race snapshot intentionally has a data race. Its count may vary, and the archived Race images come from earlier code.

## Build and run on Windows

Install MSYS2 UCRT64 GCC and run this from the repository root in PowerShell. Pass `-Compiler <path>` if GCC is installed elsewhere.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\experiments\Run-Experiment.ps1
```

The script builds with GCC C++17 and `-O2`, adding `-fopenmp` for the five OpenMP variants. It runs Sequential with one thread and requests 2, 4, and 8 threads for each OpenMP variant, with five runs per configuration. It disables dynamic team resizing. Use `-Runs 1` for a smoke test or `-Variants Lock` to run a single variant from an existing PowerShell session.

To build a snapshot on its own, open a PowerShell session in that snapshot directory with the UCRT64 bin directory on PATH:

```powershell
g++ -std=c++17 -O2 -fopenmp main.cc -o raytracer.exe
$env:OMP_NUM_THREADS = '4'
cmd.exe /d /c '.\raytracer.exe > image.ppm 2> render.log'
```

For Sequential, omit `-fopenmp` and use one thread. The experiment script stores binaries, PPMs, logs, per-run metrics, and computed means in a new `runs/` directory. It checks the corrected snapshots and Sequential for a count of 90,000 and a matching reference hash.

## Results

- [Reported five-run means](results/reported-means.csv) records the supplied timing summaries. Individual historical timings and console pixel-count logs were unavailable. The expected corrected count is 90,000.
- [Archived image hashes](results/archived-image-hashes.csv) records SHA-256 hashes computed from local historical PPM files. It covers the Sequential and corrected batches and both identifiable Race batches. The supplied Race means are recorded without assigning them to a particular PPM batch.

The archived reference SHA-256 is `F17522BFC38E2ED16A2352D3EC4C747AE2FE89B9E52D1385C8678E5D915858EB`. The archived PPMs use UTF-16LE with a BOM and CRLF line endings. New PPMs contain native stdout bytes. The script records their raw hash and also hashes the text with the archival encoding and line endings for comparison.
