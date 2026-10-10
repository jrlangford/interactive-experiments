# Interactive experiments

Native macOS examples for interactive graphics and desktop fluid simulation, built with C++17, Objective-C++, and Metal. Each example includes source code, a build script, command-line tools, and a locally signed app bundle. A shared [devenv](https://devenv.sh/getting-started/) environment manages development dependencies.

## Examples

| Example | Behavior | Implementation |
| --- | --- | --- |
| [Floating triangle](examples/floating-triangle/README.md) | A rotating colored triangle in a standard window or a transparent, draggable floating window. | SDL3 handles windows and events. Standard mode renders through SDL GPU's Metal backend; floating mode renders through native Metal. |
| [Fluid hose](examples/fluid-hose/README.md) | A transparent desktop overlay where water lands on window tops, flows around their edges, and pools above the Dock. | AppKit handles the window, menus, displays, and event loop. C++ runs parallel CPU neighbor and pressure calculations; native Metal renders the fluid. |

The hose operates on one selected display at a time. Collision geometry consists of axis-aligned window rectangles. Display selection is remembered by monitor UUID, and optional Mac tilt controls gravity on supported sensor hardware.

## Development setup

Build requirements:

- macOS 14 or later.
- Full Xcode with the macOS SDK, C++ compiler, and Metal tools, selected through `xcode-select`.
- Nix and devenv 2.2 or later for the shared development environment. Installation is described in the [devenv documentation](https://devenv.sh/getting-started/).

From the repository root:

```sh
devenv shell
build-examples
```

The same build is available as a single command:

```sh
devenv shell -- build-examples
```

Each example's `build.sh` builds that example individually. The triangle uses the SDL SDK supplied by devenv. The hose uses Xcode's tools and macOS system frameworks directly:

```sh
./examples/fluid-hose/build.sh
```

`project-doctor` is an optional diagnostic command available in the devenv shell. It checks native build tools and the triangle's SDL headers and library. Build scripts also check their prerequisites.

Optional direnv activation is configured in [.envrc](.envrc). With direnv installed and integrated into the shell, `direnv allow` enables automatic activation for this checkout.

## Running the examples

| Mode | Command from the repository root | Controls |
| --- | --- | --- |
| Floating triangle | `./examples/floating-triangle/run.sh --floating` | Drag to move; Space pauses; R resets; Escape quits. |
| Standard triangle | `./examples/floating-triangle/run.sh --windowed` | Space pauses; R resets; Escape quits. |
| Fluid hose | `./examples/fluid-hose/run.sh` | Hold Option to pour at the pointer. The 💧 menu controls pouring, pause, clear, display selection, tilt, and quit. |

The hose overlay passes mouse clicks through to desktop windows. Its display menu selects the active monitor; changing monitors clears the water. `--tilt` enables motion-controlled gravity at launch, and `--list-displays` lists available displays. Detailed controls, command-line options, and implementation limits are documented in each example's README.

The hose's automatic CPU worker limit follows the performance-core count, capped at eight; pools below 1,024 particles use serial calculations. `--physics-workers N` selects a worker limit, with zero for automatic selection and one for serial physics. Ordered correction accumulation preserves identical serial and parallel trajectories.

Build output includes the following app bundles, which can be opened in Finder:

- `examples/floating-triangle/build/Hello SDL GPU.app`
- `examples/fluid-hose/build/Fluid Hose.app`

Launch scripts and resource lookup use paths relative to the example. Both bundles contain their shaders and runtime resources and run independently of the development environment.

## Dependency management

| Dependency | How it is managed |
| --- | --- |
| SDL3 headers and library | The triangle uses matching SDL 3.4.10 headers and library from the Nixpkgs revision pinned in `devenv.lock`. |
| Bash, Git, and Python | Supplied by the same locked Nixpkgs revision. Python tools use the standard library. |
| C++ and Objective-C++ compiler, macOS SDK, and Metal compiler | Supplied by the active Xcode installation and accessed through `xcrun`. |
| macOS runtime frameworks | AppKit, Foundation, Metal, QuartzCore, CoreGraphics, and the motion helper's CoreFoundation and IOKit dependencies are supplied by macOS. |
| App resources and signatures | Build scripts compile tracked shader sources into `.metallib` files, assemble the app bundles, and apply local ad hoc code signatures. The triangle bundles a signed copy of SDL3 with an `@rpath` install name. |

The environment is defined by [devenv.nix](devenv.nix), [devenv.yaml](devenv.yaml), and [devenv.lock](devenv.lock). The lock records upstream revisions and content hashes; `devenv.yaml` specifies the devenv module revision. Generated build output, environment caches, and local overrides are excluded through [.gitignore](.gitignore).

For the triangle, `SDL3_ROOT` selects the SDK directory containing `include/SDL3`, and `SDL3_LIBRARY` selects the runtime library. Devenv exports both paths for Nix's separate header and library outputs. A custom SDK with `include/SDL3` and `lib/libSDL3.0.dylib` can be selected with `SDL3_ROOT` alone. The build copies the library before modifying its install name or signature.

Dependency lock updates use `devenv update`; the checks below validate builds against the resulting environment.

## Validation

The shared validation command builds both examples and runs headless checks using synthetic motion data:

```sh
devenv test
```

The equivalent shell command is `devenv shell -- check-examples`. Validation covers fluid physics, display selection and coordinate conversion, motion decoding and projection, helper transport and lifetime, app signatures, and runtime dependency paths. Bundle checks verify that dependencies resolve to bundled or macOS system libraries and identify embedded Nix or Homebrew paths.

GPU readback and native window checks run separately in a desktop session:

```sh
./examples/floating-triangle/run.sh --smoke-test
./examples/floating-triangle/run.sh --smoke-test --floating
./examples/fluid-hose/run.sh --smoke-test
./examples/fluid-hose/run.sh --window-check
"./examples/fluid-hose/build/Fluid Hose.app/Contents/MacOS/fluid-hose" --window-check
```

Readback checks verify rendered colors, fluid surface alpha, and cleared backgrounds. The hose's window check verifies native window placement, backing scale and drawable dimensions, frame delivery, display switching, menu actions, and click-through and focus settings across connected displays. Manual mouse interaction, physical monitor disconnection, and live tilt are interactive checks.

The validated configuration as of October 9, 2026 is an Apple M1 Pro running macOS 15.7.7, Xcode 16.3 with Apple Clang 17, devenv 2.2.2, and SDL 3.4.10 for the triangle. Shared checks, GPU readback, packaged launches, and hose duration and signal shutdown checks passed. Native hose window checks covered the built-in Retina display and two LG UltraFine monitors, each with a 2× backing scale. Parallel physics trajectory checks also passed under ThreadSanitizer with no reported races. Worker scaling measurements are documented in the [hose performance reference](examples/fluid-hose/README.md#cpu-performance).
