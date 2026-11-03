# AdrenoMetal

Metal frontend through metal2vulkan and Turnip.

The frontend uses metal2vulkan and Turnip with the kernel GPU interface and selected OS ABI. GPU composition has been verified on sheng with A740.

All clients use single-sample rendering by default. `NATIVE_METAL_SINGLE_SAMPLE=0` preserves a process's requested sample counts; `1` forces single-sample rendering. Texture types, pipeline sample counts and resolve outputs follow the selected policy. Same-user clients share writable shader caches. Resource aliasing, swizzles, range copies, function constants, specialized link names and allocation queries reach the runtime. Queue priorities aggregate current live requests without a process-name policy.

Build and interface details: [documentation](https://unixnu.com/drivers/adreno-metal). Component sources and dependencies: [catalog](https://unixnu.com/reference/components).

## Memory reuse

Unused owned image backing can serve later pooled or dedicated images. Extra image spares share a 256 MiB device budget and expire after ten seconds. Plain unused backing of at least 256 KiB can be released by the existing room worker, with at most 128 MiB queued. Allocation failure reclaims queued backing before retrying. Imported memory and owner callbacks retain their release ordering.

## Frame diagnostics

With `sheng-gpu-trace=1` the render server writes its frame trace (command phases, encoding batches, memory decisions, completion stages, attachments and Turnip pass and submission records) to its own file. Only event 63, the render, encode and completion thread identities, reaches the kernel ring. See [GPU frame diagnostics](https://unixnu.com/porting/gpu-diagnostics) for fields and decoding.

`sheng-wallpaper-freeze=1` enables wallpaper isolation. A Poster renderer freezes GPU submission after a successful command draws into and stores an IOSurface of at least two million pixels. The completed output stays retained. Later commands remain committed and unscheduled, with their resources and queue leases retained. Already submitted work completes normally. PosterBoard and other processes keep submitting. The switch defaults to off; `NATIVE_METAL_FREEZE_WALLPAPER=1` selects the same mode directly in a renderer process.

Slow encoding operations retain source lines and enqueue times. Completion handlers and resource destructors carry individual timing and identities. Kernel sampling of the render, encode and completion threads and blocked-thread waits provide the cross-thread wait record.

`python3 scripts/check-wallpaper-freeze-host.py` in the resolved tree checks real pixels, callbacks, concurrent parked submissions and process selection.


## iOS 27 Core Image resource loading

The frontend retains Core Image's original `internalLibraryWithName:device:` result. If it is nil for this frontend's device, the iOS 27 adapter retries only the known `ci_stdlib`, `ci_filters`, `ci_raw` and stitchable variants at their standard framework paths through `newLibraryWithURL:error:`. These are the original OS libraries, parsed by the normal runtime. An absent or refused library remains nil with its real error recorded. No system binary, global VFS view or compiler pin changes. Hardware qualification is pending.


## Uke 24A446 hardware result (2026-11-03)

On release `2026-11-03-ipados27-gpu-coreimage-resources` (kernel CRC `90B6BC701210D9DB`), the user confirms prior boot, desktop/Settings icon and widget loading, and Settings/Dock UI-restart symptoms resolved. The scoped original-nil Core Image library retry uses original system resources through the normal loader; the earlier VFS source-union experiment is reverted. No instrumented latency, long-duration stress or repeated-cold-boot cache result is inferred.
