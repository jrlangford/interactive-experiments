# Fluid Hose

A desktop fluid toy for macOS. Hold **Option** to pour blue water from a nozzle controlled by the mouse. Water lands on window tops, spills around their sides, and pools at the bottom of the usable desktop, above the Dock.

The overlay uses **AppKit + native Metal**, with C++ physics. It is transparent and lets mouse clicks pass through, so you can move and resize the real windows normally while the simulation runs.

## Run

From the workspace root:

```sh
./examples/fluid-hose/build.sh
./examples/fluid-hose/run.sh
```

Building requires macOS 14 or newer and full Xcode with its C++ and Metal tools. This example links only macOS system frameworks and requires no SDL SDK or third-party runtime library. The shared [devenv environment](../../README.md) provides the repository's development and checking tools; it is optional for building and running the hose itself.

Or open `build/Fluid Hose.app` from this example folder in Finder.

- Move the mouse above a window and **hold Option** to pour. Release it to stop.
- When the pointer is over a solid window body, the nozzle snaps to the nearest available edge.
- Use the **💧 menu-bar icon** to pour continuously, pause, clear the water, or quit.
- Choose a monitor through **💧 → Display**. Names, sizes, and a checkmark identify the active display. Your choice is remembered for the next launch.
- **Use Mac tilt** changes gravity using laptop motion; **Reverse tilt left/right** flips the horizontal direction if needed.
- **Ctrl+C** also quits when launched from Terminal.

For continuous pouring from launch:

```sh
./examples/fluid-hose/run.sh --pour
```

The app has no Dock icon or visible window frame. Its menu-bar icon provides the controls.

Switching displays clears the water and moves the overlay, hose, and window collisions to that display. If the selected monitor disconnects, the app falls back to an available display and returns to the saved monitor when it reconnects. The menu also refreshes when monitors are added, rearranged, or resized. The first launch defaults to the main display.

You can also select a display from Terminal:

```sh
./examples/fluid-hose/run.sh --list-displays
./examples/fluid-hose/run.sh --display 2
```

Display numbers match the current menu order; the saved choice uses the monitor's macOS UUID, so a different native display ID on reconnect does not lose the selection. The overlay uses each screen's logical desktop coordinates, including screens to the left of or above the main display.

For gravity controlled by Mac tilt from launch:

```sh
./examples/fluid-hose/run.sh --tilt
```

Tilt is off by default. Its menu status shows whether fresh motion is available. Without fresh data the water uses downward gravity. This mode is intended for the MacBook's own display: the sensor tracks the laptop body, rather than an external monitor.

The CPU worker limit is selected with `--physics-workers N`. Zero selects the automatic limit, one selects serial physics, and larger values are bounded by the machine's logical CPU count:

```sh
./examples/fluid-hose/run.sh --physics-workers 1
./examples/fluid-hose/run.sh --physics-workers 4
```

Automatic selection uses the performance-core count, falls back to the physical CPU count, and caps the limit at eight. Pools below 1,024 particles use serial calculations. On the validated M1 Pro, the automatic limit is six workers.

## How it works

AppKit owns the borderless, non-focusing overlay window and the application event loop. [NSScreen](https://developer.apple.com/documentation/appkit/nsscreen) provides connected displays and their usable frames, excluding the menu bar and Dock. AppKit's bottom-left screen coordinates are converted into the top-left desktop coordinates used by Quartz and the physics. Display notifications and a periodic refresh handle monitor changes; the view updates its Metal drawable size when its backing scale changes.

The native bridge reads normal on-screen windows using [Quartz Window Services](https://developer.apple.com/documentation/coregraphics/cgwindowlistcopywindowinfo%28_%3A_%3A%29?language=_5). Their bounding rectangles become collision obstacles in the selected display's desktop coordinate system. Window bounds refresh eight times per second; the app does not capture window contents or read window titles.

The C++ simulation applies gravity and particle density constraints using a uniform spatial grid. Cells are 12 × 12 logical points, matching the interaction radius; each particle searches its cell and the eight adjacent cells. Accepted neighbors and their distances are cached for pressure calculations. Water is retained until cleared, with a capacity of 8,000 particles.

For larger pools, macOS's concurrent dispatch queue runs neighbor searches and pressure calculations across CPU workers. Workers claim chunks of 64 particles, read a fixed position snapshot, and write each particle's own contribution list. Each worker owns reusable scratch buffers aligned to 128 bytes. A completion barrier precedes correction accumulation, which runs in particle and neighbor order on the calling thread. This order preserves bit-identical serial and parallel trajectories. Grid construction, correction application, gravity, and window collisions run on the calling thread.

Native Metal renders in two passes: particle splats add into a density texture, then a surface shader merges and shades that field into water. Water opacity is capped at 78% after merging, with a softer edge transition, so the desktop shows through drops and settled pools. The final output uses premultiplied alpha. Three particle buffers allow GPU work to overlap safely with CPU updates.

The native view directly owns its [CAMetalLayer](https://developer.apple.com/documentation/quartzcore/cametallayer). [CAMetalDisplayLink](https://developer.apple.com/documentation/quartzcore/cametaldisplaylink) drives rendering on the AppKit main run loop, including while menus are open. Physics retains its fixed 120 Hz timestep. Signals, menu quit, and the duration limit are also checked by a small timer so shutdown works when no drawable is available. AppKit termination returns through C++ cleanup, which stops the motion helper.

The native window's [ignoresMouseEvents](https://developer.apple.com/documentation/appkit/nswindow/ignoresmouseevents) property keeps it click-through. The window, view, and Metal layer are non-opaque.

After rebuilding, quit any running copy through the **💧 menu → Quit Fluid Hose** or **Ctrl+C**, then relaunch. An already running process continues using its original code.

## Scope

This is a 2D prototype on **one selected display at a time**. Windows are solid, axis-aligned bounding rectangles; rounded corners, shadows, pop-up menus, and nonstandard shapes are not modeled. A window that covers the entire canvas is omitted because it would leave no space for the simulation. The desktop floor is the usable screen edge above the Dock.

## Build and checks

```sh
./examples/fluid-hose/build.sh
./examples/fluid-hose/run.sh --physics-check
./examples/fluid-hose/run.sh --display-check
./examples/fluid-hose/run.sh --probe
./examples/fluid-hose/run.sh --smoke-test
./examples/fluid-hose/run.sh --window-check
"./examples/fluid-hose/build/Fluid Hose.app/Contents/MacOS/fluid-hose" --window-check
```

- `--physics-check` checks gravity, pooling, collision, hose placement, displacement by moving and overlapping windows, and clearing. It also compares serial and parallel trajectories through moving windows, rotated gravity, worker-count changes, snapshot copying, clearing, and display-sized resets. These checks use the CPU and require no graphics access.
- `--motion-check` checks gravity projection, stale/invalid data, fluid moving toward side walls and the ceiling, and helper transport/shutdown using synthetic data without opening sensors.
- `--display-check` checks saved selection, disconnect fallback, reconnect with changed native IDs, and conversion between AppKit and Quartz coordinates without requiring graphics access.
- Interactive display switching requires a normal desktop session; `--list-displays` enumerates the screens without creating an overlay.
- `--probe` prints desktop bounds and the window collision rectangles without launching the overlay.
- `--smoke-test` checks Metal density accumulation, a translucent merged fluid surface, and transparent background pixels without creating a window.
- `--window-check` briefly shows a native overlay on each connected display. It checks actual WindowServer bounds, Metal drawable dimensions and frame delivery, display switching, menu actions, and transparent, click-through, non-focusing window settings. It requires a live desktop, does not save a display preference, and does not open sensors. Physical monitor disconnection and manual mouse interaction remain interactive checks.
- `--duration 10` runs the overlay for ten seconds and exits.

Requires macOS 14 or newer and full Xcode's C++ and Metal tools to rebuild. The root devenv shell supplies helper tools for the shared checks; the hose build does not read `SDL3_ROOT` or `SDL3_LIBRARY`.

The validated configuration as of October 9, 2026 is an Apple M1 Pro running macOS 15.7.7 and Xcode 16.3 with Apple Clang 17. Shared physics, display, motion helper, signing, and dependency checks passed. Serial and parallel trajectory checks passed under ThreadSanitizer with no reported races. Native Metal readback and packaged window checks passed across the built-in Retina display and two LG UltraFine monitors at 2× backing scale. Duration, worker-limit, signal shutdown, and LaunchServices/Finder launch checks passed. The packaged hose links macOS system libraries.

## Motion sensor probe

The M1 Pro MacBook exposes SPU accelerometer and gyroscope devices. To check live readings from a normal Terminal:

```sh
./examples/fluid-hose/tools/probe-motion.sh --duration 10
```

This rebuilds `build/probe-motion`, so the previously used binary path also works. The probe opens the IMU non-exclusively and enables its driver reporting, power, and report interval, following the [SPU sensor reader's activation sequence](https://github.com/olvvier/apple-silicon-accelerometer/blob/main/macimu/_spu.py). It borrows an existing reporting session when samples are already arriving. When it enables reporting itself, it touches only the accelerometer and gyroscope, restores prior published settings when it exits, and defaults unpublished power/reporting commands back to off.

The probe prints live axes, report counts, and sample age. `NO DATA` means no sample was decoded; it does not mean measured acceleration is zero. Zero gyroscope values can be normal while stationary. The acceleration magnitude is nominally around one g at rest, with its components changing as you tilt. Wake-up errors are printed individually; opening a device does not establish that reporting is enabled. The protocol is undocumented and its Q16 scale is nominal.

`--self-test` checks signed sample decoding, short reports, and callback errors without opening hardware. Device opening is blocked in the development sandbox; the user confirmed fresh live readings near one g after driver activation was added.

To observe existing reporting without enabling any driver settings:

```sh
./examples/fluid-hose/tools/probe-motion.sh --observe-only --duration 3
```

## Tilt gravity and sensor lifetime

The app uses a short-lived reader process only while **Use Mac tilt** is enabled. The reader sends motion packets over local pipes. Closing the app or disabling tilt closes its control pipe; the reader detects that closure and restores its sensor settings. It uses a separate process group so it can also clean up after the main app is force-killed. No startup service is installed.

These are runtime driver settings, not settings automatically limited to one process. The standalone probe restores them on timeout, Ctrl+C, or SIGTERM. Killing the reader itself with SIGKILL, or a driver refusing restoration, can leave changes active. Restoring a prior active state can also legitimately leave sensors reporting for another consumer. After the user's completed probe, both IMU report intervals were observed back at their original value of zero.

Gravity estimation combines gyro rotation with smoothed acceleration, rejecting large bumps. It projects the resulting 3D direction into the display plane. The reader optionally obtains lid angle without changing that sensor's reporting or power settings; when the lid angle is unavailable, projection assumes a 110° opening. The projection keeps its physical magnitude, so a nearly horizontal display has less gravity along its surface.

Checks from the workspace root:

```sh
./examples/fluid-hose/run.sh --motion-check
python3 examples/fluid-hose/tools/check-motion-lifetime.py
```

The lifecycle check force-kills a test owner and verifies the mock reader stops when its control pipe closes. It does not access sensors. Physics, projection, transport, packaged executable, lifecycle, and signing checks passed; live tilt rendering still needs the normal desktop session.

## CPU performance

The benchmark compares serial calculations with worker limits of one, two, four, automatic, and the logical CPU count. Every mode starts from identical snapshots and checks that the resulting particle vertices match bit for bit. Measurements use a 1440 × 900 point canvas, 120 settling steps, 12 warmup steps per run, and three runs of 180 timed steps. Mode order alternates across runs. Seeding, snapshot copies, warmup, window enumeration, and Metal rendering are excluded.

Results for the Apple M1 Pro with six performance and two efficiency cores, compiled with Apple Clang 17 at `-O2`, are recorded in [the worker benchmark dataset](benchmarks/2026-10-09-physics-parallel.csv). Each case contains 540 timed samples per worker limit. The dataset includes small pools, 2,000–8,000 particles, and one or twelve window obstacles.

Step times in milliseconds with one window obstacle:

| Particles | Serial median | Six-worker median | Six-worker p95 | Median speedup |
| --- | ---: | ---: | ---: | ---: |
| 2,000 | 1.645 | 0.809 | 1.343 | 2.03× |
| 4,000 | 4.196 | 1.760 | 1.933 | 2.38× |
| 8,000 | 9.844 | 4.085 | 4.631 | 2.41× |

With twelve window obstacles and 8,000 particles, the six-worker median is 4.850 ms and p95 is 5.239 ms, compared with a serial median of 10.421 ms.

A 120 Hz simulation has an **8.33 ms step budget**. CPU physics timings account for one part of the frame; input, window queries, rendering submission, GPU work, and scheduling add to the complete application cost. These measurements represent standalone wall-clock step times on the host system. CPU affinity and thermal conditions are uncontrolled, and GPU timings remain unmeasured.

Benchmark commands from the repository root:

```sh
./examples/fluid-hose/tools/benchmark-parallel-physics.sh
./examples/fluid-hose/tools/benchmark-parallel-physics.sh --quick
./examples/fluid-hose/tools/benchmark-parallel-physics.sh --workers 4
./examples/fluid-hose/tools/benchmark-parallel-physics.sh --check-only
./examples/fluid-hose/tools/benchmark-parallel-physics.sh --workers 6 --profile
```

`--quick` tests up to 2,000 particles with fewer samples. `--workers` compares the selected limit with serial execution. `--check-only` runs physics and trajectory checks. `--profile` adds a 15-second workload and reports process CPU time relative to wall time; 100% represents one CPU core.

A 15-second profile with 2,000 particles, twelve windows, and a six-worker limit recorded 242.1% aggregate process CPU usage. This confirms concurrent CPU execution while the solver's ordered accumulation and other serial phases remain on the calling thread.

The [single-mode benchmark](tools/benchmark-physics.sh) measures the automatic configuration. The [reference comparison](tools/benchmark-neighbor-cache.py) verifies current trajectories against the original solver at commit `ed0f543` and benchmarks either implementation; it requires Git history. Both tools support `--quick` and `--profile`.

## Code

- [src/main.cpp](src/main.cpp): command-line options and C++ fixed-step simulation callbacks.
- [src/fluid_sim.cpp](src/fluid_sim.cpp): gravity, density relaxation, collision, and nozzle placement.
- [src/desktop.mm](src/desktop.mm): AppKit application lifecycle and display link, native overlay window, display and window metadata, Option-key state, menu-bar controls, and Metal rendering.
- [src/fluid.metal](src/fluid.metal): density splats, water surface shading, and the nozzle.
- [src/fluid_types.h](src/fluid_types.h): shared CPU/GPU data layouts.
- [tools/probe_motion.cpp](tools/probe_motion.cpp): SPU motion sensor activation and live sample diagnostics.
- [src/motion.cpp](src/motion.cpp): gravity estimation, display projection, and ownership of the reader process.
- [src/displays.cpp](src/displays.cpp): selecting a connected display and recovering the saved monitor after reconnect.
- [tools/benchmark_parallel.cpp](tools/benchmark_parallel.cpp): worker scaling, matching snapshot checks, and CPU profiling.
