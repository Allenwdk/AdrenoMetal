# NativeMetal

## Scheduling and completion separation (offline, 2026-10-20)

The native frontend now forwards scheduled callbacks and `waitUntilScheduled`
to the runtime's actual scheduling transition. Previously it called scheduled
handlers from completion and implemented the scheduling wait as a completion
wait. That also serialized `commitAndWaitUntilSubmitted` with GPU completion.
Completed callbacks keep their existing completion-queue behavior; scheduled
callbacks finish before the runtime advances to completion. Frontend encoding
refusals and commits refused by the runtime unblock waits without announcing a
scheduling event. A refused commit releases its callback ownership reference.

`bridge/scheduled_host.mm` uses a bounded semaphore to hold a completed handler:
the scheduling wait must return independently, and the scheduled handler must
observe `Scheduled`, before completion. It covers the runtime thread, an explicit
serial completion queue, a frontend encoding refusal, and an open-encoder commit
refusal. The old frontend fails the successful-command case. Telemetry adds
separate scheduling/completion wait counts and elapsed times (`J` rows).

The host qualification and ARM64/iOS17 cross-build use metal2vulkan revision
`720ef1ce9ae2` with the umbrella's recorded patches. These checks
qualify API ordering over a host Vulkan driver, not Adreno timing, system UI
frame rate, panel presentation, or cold-start shader coverage. This is a
confirmed serialization defect, not proof of the sole cause of hardware lag.

The render pipeline creation entry points also forward descriptor
`binaryArchives` and the raw `MTLPipelineOption` bits to the runtime. Previously
the archive could be populated/serialized but creation always used default
options, silently compiling even when a listed empty archive should fail under
`FailOnBinaryArchiveMiss`. Foreign archive objects are refused. The C ABI keeps
the original default entry point and adds a counted archive list for creation.
`bridge/archive_host.mm` proves empty/changed-descriptor strict misses, then
serializes and reloads an archive in a second process with an independent fresh
shader-cache directory, and checks the rendered pixels over the host GPU.

These archives contain the translator's artifacts, not Adreno machine code.
The runtime does not consult them for later draw variants, and Vulkan driver
pipeline compilation still requires its separate, identity-matched cache.
This change repairs render archive consumption; it does not seed the system
image or qualify complete CoreAnimation shader/variant coverage or a cold-frame
latency bound. Compute descriptor archive consumption remains separate work.

Timing schema v2 attributes encoder API counts to the command buffer that
received them, rather than subtracting global counts at commit. Commit indices
are assigned under the ring lock so simultaneous commits cannot reuse an index.
The historical `gpu=` field is now `ctc=` (CPU commit-to-completion-callback).
The latest four buffers also report creation-to-commit age, which includes
caller idle time, and commit-to-scheduled-callback time (`-1` if not observed).
Pipeline/library/function and Vulkan/cache totals occupy separate rows to fit
the 90-character transport limit. The worst-case table stays within 48 rows.

`bridge/telemetry_host.mm` interleaves render/blit encoding on two queues and
commits in reverse order, then checks both the attributed work and actual host
GPU pixels. The previous global-delta implementation fails this test. Counts
describe API requests, not proof that the GPU executed every requested command.

## Resource-specific completion waits (offline, 2026-10-20)

The resource SPI `waitUntilComplete` no longer drains every in-flight command
buffer. It asks the runtime whether an already-committed buffer references the
resource's storage. Texture views resolve to their root or backing buffer;
heap resources conservatively share a dependency, and imported host ranges are
compared for overlap without reading their bytes. An unknown bridge result
preserves the conservative wait. The frontend has no live remap/rebind API;
this query is not a remapping lifetime protocol. Completion and actual GPU
results remain the runtime's.

`bridge/patches/0002-resource-wait-storage-dependencies.patch` records the exact
runtime extension. The umbrella builder verifies the complete source diff for
both patches. This changes the runtime source digest and therefore the shader
cache build identity; artifacts must be produced for this exact build.

The bounded `resource_wait_host.mm` test holds pending work behind an unsignaled
shared event. The previous frontend blocks even for a resource the work never
uses. The fixed frontend returns for that resource and keeps waiting for the
actual buffer, a buffer-backed texture view, nested shared/private texture
views, two imports of one IOSurface, heap storage and overlapping no-copy
buffers. It then signals the event and checks the actual host GPU bytes,
using GPU readback for private resources. J rows report resource wait time and
dependency checks/skips; the maximum table now occupies 47 of 48 rows.

This removes a reproduced serialization defect. Static target-code callsites
do not establish its per-frame frequency, hardware speedup, stable 120 FPS or
the sole cause of lag. No hardware boot or timing qualification is implied.

Native ARM64/iOS 17 Metal API frontend for the original iPadOS graphics stack.
The device is published through MTLAddDevice. The frontend translates AIR with
metal2vulkan and submits through Turnip and NativeAdreno. The desktop is
GPU-composited but slow; shader compilation is not fixed.

`tools/MetalVulkanProbe.rs` is an explicit offline consumer of metal2vulkan's
public compiler API. The umbrella pins the upstream source and locked Cargo
dependencies. The probe selects `exact_answers()` and separately selects
exact sampling; it still exports all precision notes, resource requirements,
specialization and runtime dependencies. Those switches are not an assertion
of numerical equivalence. The executable links no Vulkan loader and never
publishes a Metal device. It emits SPIR-V for independent validation, not
Adreno instructions or completion events.

The accompanying vertex layout is an authored diagnostic interleaving of
float4/float2/float4 attributes at locations 0/1/3, with a 48-byte stride.
It is supplied explicitly by the caller, not inferred as a system runtime
layout. Missing descriptors/constants and unsupported stages remain errors.
Fragment compilation requires an explicit attachment map. The coverage
attachment fixture enables R32Float at attachment 2; silently attaching only
target 0 would let these coverage shaders optimize into empty fragments.
The umbrella records actual SPIR-V Output-variable stores and can require
them for selected fragment tests. A store instruction is still not proof of
shader execution or correct pixel values.
Target parsing, pipeline translation and SPIR-V validation require separate
evidence from shader execution, native GPU4K translation, submission/fences,
IOSurface sharing and display presentation.

The partial device explicitly returns false for every GPU-family and feature-set
query. Its implemented subset does not qualify a complete family. This also
avoids the original base class dereferencing Apple capability tables which
this independent device does not initialize. The original RenderBox RBDevice
initializer, device getter and internal dispatch-queue getter now execute in
both actual original-runtime diagnostic workers; their subsequent original
IOSurface sharing and four real GPU completions still succeed. This qualifies
construction only, not original RenderBox rendering or system device discovery.

Queue foreground/background priority queries and setters forward to the actual
host queue after validating its real Objective-C method ABI. No priority or
successful change is synthesized. Host qualification changes and restores a
real foreground priority, preserves a peer queue, and refuses foreign/wrong
objects and invalid arguments. Original RenderBox executes both real
priority queries in each native diagnostic worker. No RenderBox frame is qualified.
The broker explicitly allows the two new bounded operation IDs; its
native-4K candidate retains all 29 original executable component hashes.

An interrupted backend session now retains its actual operation/completion
ledger with transport_healthy=false and session_finished=false. The caller
must return failure for such a report. A real disconnect control verifies
the incomplete report contains no fabricated command completion.

Explicit command-buffer enqueue now reserves the real host Metal queue and
returns its actual Enqueued status. The synchronous wire permits one pending
reservation per queue; duplicate/foreign requests and unsupported predecessor
ordering refuse before entering a blocking host wait. Independent queues can
both reserve and execute. Host qualification completes four GPU commands and
verifies all returned pixels, including retrying a second reservation after
its real predecessor finishes. Actual original RenderBox enqueues both
native workers' real buffers. Pending reservations are never reported as GPU completions.

Registered completion and scheduled handlers execute once on a genuine
dispatch worker after verified GPU completion and original surface/buffer
synchronization. waitUntilCompleted also waits for those handlers with a
bounded timeout. Scheduled callbacks are late in this synchronous transport;
no early asynchronous scheduling timing is claimed. The original target's
BOOL-returning commitAndWaitUntilSubmitted interface derives success only
after a real successful commit. The two-process surface regression exercises
both public/private commit paths, real enqueue status, ordered once-only
callbacks with actual completion/timestamps, shared-surface seed coherence and
post-peer-exit GPU work. All five native broker/permission controls also pass
against the same new candidate. Original RenderBox's convenience image path
can return an image with only empty command buffers; that image is explicitly
not qualified as GPU rendering. Actual renderer resource use/output must still
be established before normal device discovery or any desktop claim.

The frontend dynamically derives from the supplied target's actual 536-byte
_MTLDevice class and adds only its own method implementations. Original
implementations are not overwritten. Actual original descriptor objects
are marshalled through bounded typed requests over NativeVirtioConsole's
administrator userclient. Host replies carry real object handles, GPU
completion/timing and texture bytes. Unsupported operations refuse.

Implemented operations create queues, known original libraries, named
functions, simple render pipelines, shared R32Float/BGRA8Unorm/RGBA8Unorm 2D textures, command
buffers and render encoders. Pipeline/cull/draw/end/commit operations call
real host Metal APIs. Commit waits for actual GPU completion; texture reads
copy actual getBytes output. Proxy deallocation drops its real host table
reference, while actual Metal command buffers retain their own resources.
Session finish drops remaining references. There is no CPU pixel renderer.

RGBA8Unorm now pairs with real original 32RGBA IOSurfaces as requested by
original RenderBox. Upload/readback retain actual channel bytes without CPU
swizzling. The native two-process sharing/callback regression uses RGBA:
four real GPU completions, five full readbacks, original seed coherence,
Mach authorization/refusal and post-peer-exit work pass with correct channel
order. The separate BGRA two-process/permission regression also passes on the
same source/candidate. Original RenderBox allocates both actual RGBA surface
textures before its next real private RG16Float resource refusal; no original
renderer frame is qualified by those allocations.

Private RG16Float textures and actual bounded texture blits are now implemented.
Private textures never use CPU getBytes/replaceRegion; shared staging is filled
by a genuine Metal blit. The native two-worker resource instrument clears each
private texture and a contrasting shared destination on the GPU, copies between
them, and checks every half-float result. Invalid bounds and direct private CPU
reads refuse in both workers. Together with the original RGBA surface sequence,
six actual commands, twelve ordered callbacks and seven complete readbacks pass,
with distinct live native-4K worker CPU contexts and no pending reservations.
All five broker/permission controls pass on the fresh texture-copy candidate.
Blit destinations backed by original IOSurfaces join the real post-completion
readback/seed path; no CPU pixel conversion or private-memory mapping is used.
Private Stencil8 is now supported by the attachment/state extension below.

Private Stencil8/Depth32Float resources, real depth/stencil states, reference
values and pipeline/render attachments are implemented. Both actual native
workers execute the unchanged original shader pair through stencil mismatch/
match and depth reject/pass controls, checking every returned pixel. Together
with the RGBA surface sequence, twelve real commands, twenty-four ordered
callbacks and thirteen complete readbacks pass; private stencil CPU reads and
wrong-type state bindings refuse. All five current-candidate broker controls
pass. Combined Depth32Float_Stencil8 also passes the corresponding real host
GPU tests, but has no separate native combined-attachment qualification yet.
Original RenderBox now reaches its inherited optional Apple pipeline-library
loader and faults on a null internal object. No original renderer frame is
qualified. An interrupted active encoder also exposes a host teardown assertion;
both are retained explicit failures, to fix before progressing the renderer.

Shared/default-cache buffers now have real host allocations and stable native
contents mirrors. Bound buffers are retained by their command; their actual
current bytes upload immediately before GPU commit, and actual buffer bytes
download after completion. Thus CPU writes after encoding remain visible to
the GPU. This is explicit copying, not shared physical GPU memory. Allocation
is bounded at 64 MiB and each diagnostic session at 8192 operations; arbitrary
large workloads/session combinations are not qualified. Other storage/options
refuse. Vertex/fragment bindings, inline byte bindings, indexed draws and
aligned nonoverlapping GPU buffer blits use actual corresponding Metal calls.

The native resource/presentation instrument qualifies 84-byte original
RenderBox uniforms and 12-byte indices. A real GPU blit replaces a distinct
destination pattern, and that actual destination becomes the original
accumulator_gradient_vertex input. Indexed triangles produce the right-half
image, then a uniform update after encoding produces a full image. All pixels
reach actual GOP and are read back. Exit 163 and absent/default controls pass;
this does not establish complete Metal capabilities, system UI or Adreno.

Original texture-read shaders now execute through real vertex/fragment
texture bindings. Commands retain referenced textures and recheck real
IOSurface generation seeds before commit. GPU blend pipelines carry actual
per-attachment factors/operations; viewport, bounded scissor and constant
blend color use real Metal state. Unsupported multisampling and invalid
clipping refuse before issuing the unsupported GPU operation.

The native composition instrument creates its two source images on the GPU,
then uses unchanged read_surf_vert/read_surf_frag to copy and blend them. It
qualifies full blue, clipped premultiplied composition and constant-factor
composition through actual IOSurface and GOP, checking every pixel. Exit 167
and both device/default controls pass. This is actual GPU composition, not a
CPU-generated image or the original system desktop. Filtered samplers,
function constants, complete resource/format/capability contracts, system
discovery and original framebuffer swap integration remain incomplete.

Basic nonplanar R32Float guest IOSurfaces now synchronize with host textures.
BGRA8Unorm also supports the original framework's actual 32BGRA surfaces;
descriptor/surface format mismatches and unsupported planes refuse. A genuine
GPU clear and the unchanged original attachment_clear_frag_lpf shader operate
on this format, with full native surface/readback hashes and actual ordered
GPU timing. The BGRA instrument exits 151. The bounded host session allows
8192 operations so full display-size surface uploads can fit; individual
request/reply and texture extent limits remain unchanged. This does not add
system device discovery, display presentation or physical Adreno execution.
The frontend uses the original IOSurface framework's actual locks, mapped
base/pitch/allocation and generation seed. Bounded byte uploads preserve
existing native surface contents; actual completed GPU output is copied back
before the frontend reports completion. This is explicit offline transfer,
not shared guest/host GPU physical memory or deployment zero-copy. Unsupported
planes/formats/extents refuse. Command buffers retain their surface targets.

The parent native IOSurface instrument actually creates a 64x64 surface,
exports/imports its real Mach port and verifies both references see the GPU
output. Initial float-two pixels upload unchanged; a real GPU clear returns
float-one to the surface, which becomes the next texture's actual upload.
An original shader draw then returns zeros through that same original surface.
The actual native exit is 149, with active CPU 4K and original AMFI/root
retained. Full IOSurface cross-process/presentation semantics remain open.

Libraries use a bounded content-addressed cache of the exact original
QuartzCore and RenderBox files supplied to the host instrument. The guest
hashes complete actual newLibraryWithData input. Unknown identities refuse;
known input selects the already hash-qualified unchanged AIR slice of that
same original file. Arbitrary named functions are obtained from the real
host library. No function body or target metadata is rewritten. General
library uploads, specialization constants and source compilation remain open.

The parent native instrument performs an actual clear and an original
QuartzCore shader draw using normal Metal API calls, checks every returned
pixel and GPU timing, and exits 119. Unknown library/function controls refuse.
Automatic original MTLCreateSystemDefaultDevice still returns nil: this
explicit partial object is not advertised as a completed system driver.
Active CPU translations remain native 4 KiB; original AMFI/root checks
remain active. No active GPU SMMU geometry is inferred from host Metal.

Not yet: full Metal feature/device capabilities, general texture formats,
general render descriptors, cross-process resource sharing, normal
bundle/service packaging, events/fences, heaps, argument buffers, indirect
commands, drawables and presentation, or a native Adreno backend.
Keep binary inputs, extracted shader files and observation artifacts outside
this source repository.

The separate version-3 offline session protocol adds independent real host
object tables and per-session request ordering. Host resource identifiers
are unique across sessions, but lookup/release/bind operations resolve only
inside the selected table. Closing one session drops only its independent
references; another session can still submit real GPU work and read its own
output. Existing version-2 framing remains available without multiplexing.
The bounded host instrument opens exactly two sessions and checks seven
actual cross-session, stale/unknown-session and replay refusals, three real
GPU completions and complete texture readbacks. Run the umbrella
check-host-metal-originals.py followed by check-host-metal-sessions.py.

Session selection belongs to the owned kernel broker, never caller-supplied
Metal request bytes. The host-only checkpoint remains distinct from the
native runtime evidence below. It establishes no guest display, active
Adreno SMMU mapping, hardware execution or original desktop.

NativeOfflineGPU.cpp is the explicit private kernel component of this
partial offline stack. It holds a real exclusive byte-transport open,
serializes each complete host request/reply and buffers replies separately
for each actual native IOUserClient. The kernel selects an independent host
namespace; public request bytes contain no selectable session identity.
Framing, ordering, reply bounds, unread-reply ownership and teardown have
real failure paths. A transport failure prevents reuse of a partial stream
by another client. Closing a namespace releases only its own actual object
references. This component registers only its private test class, not an
IOAcceleratorES service or a system Metal device.

The umbrella native instrument actually spawns two original-runtime child
processes. Each uses the real Metal object/command frontend and a distinct
original IOSurface. Three actual M1 GPU submissions complete, all surface
pixels are checked, a foreign texture handle is refused, and one process
continues GPU execution after the peer exits. At each actual userclient
creation the broker records live TCR/TTBR0/TTBR1/SCTLR and the actual task/PID,
and refuses nonnative-4K CPU contexts. Both workers report a 4096-byte ABI;
their actual TTBR0/task identities differ. These are CPU observations, not
active native-4K Adreno SMMU qualification.

Only the own signed test process has the exact service/IOKit permissions
required by the original Sandbox: no-container and an allowlist containing
NativeOfflineGPUClient and IOSurfaceRootUserClient. Removing either
permission genuinely refuses spawn or device access. Original Sandbox,
AMFI, code signatures and both original trust modules remain unchanged.
Run the umbrella build-offline-gpu-broker.py, Metal build/stage/probe helpers
with --broker-processes, and check-offline-metal-processes.py. Authorized
general concurrent IOSurface sharing/coherence, normal system discovery,
complete Metal/presentation contracts, the original desktop and native Adreno
execution remain open. Scoped synchronous sharing and explicit own bundle
packaging are separately qualified below.

## Explicit own bundle factory and normal-path offline view

`OfflineMetalPlugin.mm` packages this partial frontend as an own signed bundle.
Its real principal class derives from the target's original `_MTLDevice`.
`initWithAcceleratorPort:` rejects null and unrelated services, requires the
actual unique private NativeOfflineGPU registry entry, and opens that same
service through the real frontend. The returned device reports the actual
provided guest service registry ID; host metadata retains the separate backend
identity. No original class implementation is replaced. This factory still
uses the private diagnostic broker, not an IOAcceleratorES implementation.

The umbrella addon instrument mounts an existing source-built XNU bindfs
read-only union view at `/System/Library/Extensions`. Only own signed code is
added; actual lower-volume AGX metadata remains accessible. Its backing is
mutable own RAM Data, so it is not an immutable authenticated production addon
volume. Original NSBundle loads the own bundle in two real original-runtime
children, accepts its principal class and invokes the factory with real ports.
Both children then perform genuine Mach-authorized original IOSurface sharing,
seed-driven refresh, four actual GPU completions and peer execution after the
creator exits. The own helper has only its scoped no-container/IOKit permissions
and the original bindfs-allow permission; original applications retain theirs.

`python3 scripts/check-offline-metal-addon.py` in the umbrella checks this
checkpoint and two real controls. Identical own machine instructions lose
mount access without bindfs-allow. A one-byte change to the own factory's code,
retaining its original signature and explicitly trusted hash, triggers an
actual bad code-page count and cs_invalid_page before any GPU access. Both
original trust modules and every original executable remain unchanged.

This is explicit offline factory/packaging qualification. It does not publish
a normal Metal device, invoke normal Metal registry discovery, qualify ordinary
application access, asynchronous shared fences or a desktop. Original default
Metal remains nil. The complete Adreno backend and its active native-4K SMMU
remain unqualified.


## Actual file libraries and interrupted encoders

The explicit device loads actual local file/URL/default-bundle libraries through
its existing exact-input hash qualification. Successful loads clear stale errors;
unreadable files, non-file URLs and readable non-library inputs fail honestly.
The optional Apple-family pipeline-library directory is explicitly unsupported.
Unchanged original RenderBox handles that error and genuinely loads its original
portable AIR library, then requests actual function-constant specialization. That
next interface remains absent at this checkpoint; no renderer frame is claimed.

The host tracks actual open render/blit encoders. Ending an encoder invalidates
further encoder calls; duplicate end, post-end draw/state/copy, concurrent encoder
creation and commit with active encoding refuse before host Metal APIs. On actual
session closure or transport loss, cleanup only ends abandoned encoding, without
submitting the interrupted command or manufacturing GPU completion. The complete
partial ledger survives with explicit failure. Host unfinished/ended/disconnect
controls and native two-worker library/depth/stencil regressions pass. These are
explicit offline results, not normal system discovery, desktop or Adreno proof.


## Original function constants and RenderBox GPU drawing

The explicit frontend reads a copy of the actual iOS17 constant-values object
through its original typed array/data/type/index/name methods, with exact runtime
class and ABI checks. It transfers bounded original bytes to actual host Metal
specialization; it never substitutes an unspecialized function for that request.
Scalar and two/four-component types have explicit bounded sizes; matrices,
three-component alignment profiles and unsupported values refuse. The extension
uses a distinct bounded payload on the existing function operation. The host
checks exact lengths, counts, identities, types, sizes and duplicate keys before
calling real Metal, and records actual values and compiler outcomes.

Both actual native workers now run the unchanged original RenderBox renderer,
which specializes original plane__vertex/plane_color_fragment for both shapes,
binds real original buffer offsets and executes GPU indexed draws. Every pixel
of each returned original image matches left-red/right-blue, independently of
the complete host GPU readback hashes. Original surface uploads are all zero,
so the shapes do not originate in CPU-painted texture data. Six real commands
complete including the separate four-command surface-sharing/lifetime sequence.
This qualifies the explicit original renderer path, not the system desktop.

The native depth/stencil fixture additionally qualifies both original named and
indexed constant accessors, source-object preservation, stale-error clearing,
unknown-function refusal and unsupported-type refusal. An empty constants set
and its pipeline are genuinely accepted by this exact host backend; this does
not qualify unspecified shader-state semantics. Earlier incorrect negative-test
expectations and actual observations remain retained separately. The host-only
function-constants control compares indexed/named red output with white output
after one real UInt4 bit changes, verifies every pixel and checks nine genuine
argument/isolation/compiler refusals. Native buffer-offset methods retain the
actual previously bound resource and rebind its real offset through the existing
checked path. Complete normal discovery, asynchronous contracts, presentation,
the original desktop, Adreno execution and active native-4K GPU SMMU remain open.


## Unpublished original accelerator-base lifecycle

OriginalAcceleratorABI.h declares only the exact supplied IOAcceleratorES base:
136 bytes and the existing 168-entry IOService table, with no new operational
methods. The umbrella audit derives its real protected-compatible constructor
aliases, destructor and metaclass from the unchanged original IOGPU component.
NativeOfflineAccelerator uses those genuine bodies and its own compiler-produced
type metadata. No original class/vtable/method is replaced. This object is never
attached, started or registered as a system service.

The opt-in kernel lifecycle fixture actually allocates/initializes the own object,
confirms its actual original parent and rejects unrelated IOGPU/private-broker
ancestry, then releases it. Real instance counts go 0 -> 1 -> 0. Omitting original
IOGPUFamily load genuinely refuses before allocation; default-off creates no
object. The complete native table and exact base imports are independently audited.
Both controls share the same immutable candidate. This is early CPU-4K/kernel ABI
evidence before root acceptance or userland; no GPU client, backend response,
normal Metal publication, original desktop or Adreno execution is qualified.
Run the umbrella build-offline-gpu-broker.py --accelerator-abi and the dedicated
probe-accelerator-abi.py / check-accelerator-abi.py tools. The build option defaults
off. Complete normal device/client/resource/queue/surface/presentation contracts
remain required before original system discovery can publish this partial stack.

## Original CAMetalLayer drawable present requests

The explicit frontend now retains each actual drawable through standard
presentDrawable:, validates its protocol, method ABI, own-device IOSurface
texture and bounded identity, and refuses duplicate or completed submissions.
After real GPU completion and actual IOSurface downloads, commit invokes the
original drawable's present method. It preserves real GPU completion callbacks
if that original method raises. It creates no display timestamp or fake fence.

The target's unchanged CAMetalLayer creates an actual CAMetalDrawable on this
explicit device. Its original present implementation remains at its original
cache offset. Two actual GPU commands produce the original RenderBox RGBA image
and convert it to the drawable's BGRA texture. Every one of 4096 pixels and both
independent full GPU readback hashes agree; all initial texture uploads are zero.
Invalid-object, duplicate and post-completion requests refuse, and both actual
command callbacks execute once. The actual first drawable ID and presentedTime
are zero. This detached layer performs no framebuffer swap or retirement and
qualifies a genuine present request only, not display completion. Time/duration
presentation variants, normal system discovery, original desktop, Adreno and
active native-4K GPU SMMU remain open. The umbrella --metal-layer fixture and
check-original-metal-layer.py retain exact source, signed-input and CPU-4K evidence.

## Genuine GPU fences and explicit texture hazard modes

OfflineMetalFenceWire.h extends the bounded transport with real fence creation,
render stage updates/waits and blit updates/waits. The frontend implements the
complete MTLFence object protocol, owns the actual device identity and retains
fence proxies through commands. Object labels use atomic copy semantics. The
backend invokes real Metal fence APIs on actual per-session objects; it never
marks a fence signaled in CPU state. It refuses foreign/released handles, wrong
or ended encoders, unsupported stages, an update-then-wait cycle in one pass,
missing producers and unsent cross-command dependencies. This synchronous subset
accepts an earlier ended producer pass in the same command or a genuinely
completed producer on another queue; asynchronous pending producers remain open.
Producer bookkeeping releases with the independent fence handle or namespace.

The texture extension preserves old request layouts and explicitly accepts
Default/Tracked/Untracked descriptors. It queries and returns the actual backend
mode and refuses an explicit mode mismatch. Texture resourceOptions reflect the
actual storage/cache/hazard attributes. Default really resolves to Tracked on
the observed backend. Neither resource attributes nor host Metal fences qualify
Adreno residency, interrupts, fences or an active native-4K GPU SMMU context.

The host-only check-host-metal-fences.py executes four explicitly Untracked
resources through real render -> blit -> original QuartzCore shader rendering,
then a real completed-producer dependency on another queue. Both full readbacks
match all 8192 pixels, with no texture uploads. Eight actual fence calls and two
GPU completions pass; twelve argument, namespace, lifetime and dependency controls
refuse. An encoded but unsubmitted producer/control is never submitted by cleanup.
The existing original function-specialization and ended-encoder regressions pass.

The native --fences fixture uses an actual private Untracked GPU target, a real
render-update -> blit-wait/update -> render-wait chain and unchanged original
QuartzCore shaders. The private result is GPU-copied to the actual original
CAMetalDrawable, then GPU-sampled in its final render pass. Both full readback
hashes and all 4096 drawable pixels agree. Five native fence refusals, real
protocol/copy-label behavior and existing drawable/callback controls pass.
Normal present invokes the original implementation only after real completion
and IOSurface visibility. The detached layer still has presentedTime zero and
no framebuffer swap. This is explicit offline synchronization qualification,
not a system desktop, asynchronous general queue contract or native Adreno proof.

## Asynchronous host submission and real cross-queue GPU events

OfflineMetalCommandWire.h adds separate submit/status operations. Submit invokes
actual Metal commit and returns without waitUntilCompleted. Status reads the real
command state; completion times remain unavailable until genuine completion.
Completion records are unique even across repeated queries. Namespace close and
transport-loss cleanup wait for already submitted work and record its real GPU
result before releasing references. They never submit merely encoded controls.
Transport loss remains unhealthy/unfinished even when its actual GPU work completes.

An initial pending-producer cross-queue MTLFence experiment returned an all-zero
copy while the original shader output was red; both buffers reported Completed.
The exact failed source, executable, GPU readback and timing ledger are retained.
This is a failed qualification, not successful asynchronous resource visibility.
Cross-queue fence waits therefore still require a genuinely completed producer.
Same-queue waits can consume actually committed producers. The supplied host
profile uses actual MTLEvent timeline signals/waits for overlapping queues.

OfflineMetalEventWire.h creates genuine per-session GPU events and encodes real
command-buffer signals/waits. CPU state records only encoded producers; it never
reports a signaled value. Missing, foreign, unsubmitted, same-command, zero/future
and nonmonotonic dependencies refuse. No CPU fence or precomputed completion is
substituted. The current bounded profile requires the producer already committed
before encoding its consumer wait; future unsubmitted producers remain unsupported.

check-host-metal-async.py --api-validation and its --disconnect-control execute
three genuinely nonblocking submissions, six fence calls and five GPU timeline
event calls on two actual queues. Initial real states are Committed; queries also
observe Scheduled and Completed with actual GPU times. Every one of 8192 pixels
matches after the original shader and cross-queue event dependency. Thirty-one
argument, namespace, state and lifetime controls refuse. An independently released
submitted command finishes during real cleanup; unsubmitted controls never gain
GPU submissions. The disconnect case exits with failure and keeps its truthful
incomplete transport ledger. Eight additional bounded API-validation runs pass
24 GPU commands and 65536 pixels, including eight waits on actually in-flight
producers and four disconnect cases, with no unaccounted validation messages.
The original synchronous fence regression also passes.

That initial checkpoint was host-only. The separate opt-in native integration
below now qualifies its own visibility/callback/lifetime subset. It does not
qualify system discovery, original desktop, native Adreno events/fences or active
native-4K GPU SMMU state.

## Opt-in original-runtime asynchronous commands

The private broker now accepts the exact bounded command/event extensions. The
frontend explicitly opts in through offline_metal_frontend_enable_async; default
contexts retain synchronous commit. Each new command captures that mode. Actual
submission returns independently of a dispatch worker that observes real GPU
status, synchronizes actual IOSurface/buffer data, invokes the original drawable
present and delivers callbacks. Event/fence/resource proxies stay retained through
completion. Proxy seed/data access is synchronized across workers.

Scheduling, resource visibility and callback completion have independent groups.
waitUntilScheduled only waits for actually observed scheduling; it cannot wait
for a callback while an original caller holds its lock. The original private
commitAndWaitUntilSubmitted B16 ABI returns actual accepted submission in async
mode. Successful completion waits include resource visibility and callback
execution; callback reentrant waits use their actual worker identity. Scheduled
callbacks remain deliberately late. Real transport/GPU failures retain errors;
presentation/callback exceptions remain separate from real GPU completion.

Context finish refuses an active callback caller, waits pending work and all its
callbacks, then closes the actual namespace/userclient. Atomic lifecycle flags,
a recursive transport lock and release/close handoff keep a proxy destructor
from sending a stale release after namespace close. Finish never submits merely
encoded work. It permits the real completion callback's readback while closing.

The --async-commands native fixture enables the own device before unchanged
original RenderBox initialization/drawing. All four actual GPU commands use
nonblocking submit, including that original renderer. Native workers deliver
five own callbacks away from the caller thread, two reentrant completion waits
and one genuine callback-close refusal. An explicit caller-held lock blocks its
scheduled callback while waitUntilScheduled still returns, qualifying the
scheduling/callback separation. A final GPU clear has its caller reference
released and no explicit wait; context finish waits its callback, which checks
every one of 4096 real green pixels before close. Original drawable/peer images
also match all pixels and independent GPU hashes. Both GPU event values execute
on genuine commands; no CPU signal is substituted.

The same kernel passes the existing default synchronous drawable/fence path and
real default-off original launchd main, preserving actual root, original AMFI/
signatures/trust and active native CPU 4K. Use the umbrella dedicated async probe
and checker, including --default-off and --synchronous-control. This is explicit
offline original-runtime integration, not normal device/client authorization,
window-server composition, display timing, original desktop or native Adreno.
The actual detached drawable presentedTime remains zero and no framebuffer swap
occurs. Shared-event Mach export/import and complete concurrent/system contracts
remain separate requirements before normal discovery publication.

## Actual host shared events, listeners and exported Metal handles

OfflineMetalSharedEventWire.h adds actual newSharedEvent, value access, labels,
real listener registrations/polls and internal export/import/drop operations.
Only original Metal callbacks mark a notification ready. Pending registrations
are cancelled at namespace teardown without fabricating delivery. A separate CPU
setter control is explicitly recorded and never drives the rendered dependencies.
Shared-event aliases retain actual Metal handles and common producer metadata;
closing an owner retires its producer references. Imported waits can use the real
hardware counter or an actually committed producer, never a CPU signaled flag.

The backend imports with actual newSharedEventWithHandle after the original
namespace closes. Its imported object continues GPU work after the actual export
handle is released. Export references keep the private transport alive until
released. Internal control framing is distinct from client sequence accounting;
a forbidden client control request refuses without desynchronizing its sequence.
Public native raw submission must exclude these internal integer export IDs and
mediate them through actual native Mach capabilities before this feature connects.
At that host-only checkpoint the native broker still ended at ordinary GPU event operations.

check-host-metal-shared-events.py --api-validation executes three GPU tasks,
five shared GPU signals/waits, all 12288 original-shader/peer pixels and eighteen
refusals. Three actual listener callbacks and two genuinely unfired cancellations
pass. The hardware timeline reaches 1/2/3 from GPU commands; only an unrelated
control event uses a real CPU setter to 7. Actual shared-event device metadata
reports this owner device on the tested host; nil is never assumed from a type
name. Its actual exported handle copies the label and has a real supported trace
getter returning zero. Previous async and disconnect regressions still pass.

The umbrella audit derives the unchanged target's original 16-byte secure handle,
24-byte private data, typed eventPort/label/labelTraceID getters, label copy and
real mach_port_mod_refs send-right increment. Its original listener construction
ABI is also pinned. That is source analysis only: native Mach transfer, original
handle construction/coding, recipient import and process-exit lifetime must be
qualified separately. No raw host export ID is a native permission or completion.
System desktop, native Adreno and active native-4K GPU SMMU remain unqualified.

The native private broker now mediates shared-event export/import through real
IKOT_IOKIT_IDENT OSObject Mach capabilities, with independent internal host and
public request sequences. Raw host export identifiers are never accepted. Token
no-senders destruction drops the actual retained host Metal handle exactly once,
independently of creator userclient lifetime. Both custom selectors verify the
actual owning task, genuine token class/controller and bounded metadata buffers.

The frontend calls the unchanged original MTLSharedEventHandle constructor, whose
real send-right reference count rises and falls 2 -> 3 -> 2. Host-derived event
metadata preserves a genuine nil or owner device (which varies with actual host
API validation). Counter reads, bounded waits, copied labels and GPU wait/signal
encoding are backed by actual Metal operations. At this capability-only checkpoint native listeners explicitly
refuse until independently qualified. No original code/methods are replaced.

Run the umbrella build/stage --shared-events, probe-native-metal-shared-events.py
and check-native-metal-shared-events.py. Two real original-runtime native-4K
workers prove recipient import after creator waitpid/exit, three actual GPU tasks,
all 12288 pixels with no texture uploads, and continued imported GPU execution
after the final original handle/Mach right is destroyed. Wrong/stale Mach rights
and guessed raw host imports refuse without breaking subsequent real requests.
One export/import/drop and zero live tokens remain at finish. The own received-
right adapter uses the actual original constructor; it does not qualify original
NSXPC/NSSecureCoding serialization. System Metal/desktop and Adreno remain open.

The native shared-notification extension now validates the unchanged original
MTLSharedEventListenerInternal 24-byte class and real typed dispatchQueue getter.
Only genuine host Metal listener ready records can schedule a native user block,
on that original listener's actual queue, with exact event identity/counter.
Bounded workers record failures separately. Pending/queued/running callbacks have
distinct states under the lifecycle lock. Context finish cancels unfired or queued
registrations, waits genuinely running callbacks and rejects reentrant finish from
one of those callbacks. It never waits a queued block behind a caller-held queue.
After close a cancelled queued block skips delivery; no new transport call escapes
past namespace close. A cancelled or timed-out registration is never successful.

Build/stage --shared-notifications, probe-native-metal-shared-events.py
--notifications --api-validation, then the matching checker flags. Two callbacks
run on distinct original listener queues away from the caller, reenter real
counter access and refuse callback-local finish. Two value-99 registrations never
fire. A third real host callback reaches a deliberately blocked original queue:
finish cancels it without deadlock, and draining the queue confirms no user block.
The host independently records three actual GPU-triggered notifications/two
unfired ones; native counters record two delivered/three cancelled/zero failed.
The existing creator-exit/Mach rights/12288-pixel GPU controls still pass. The new
frontend and shared-event kernel also pass the full original RenderBox async
primary path (four actual GPU tasks, original images, event/fence/callback controls).
Original secure coding, normal system publication/authorization, desktop and
native Adreno/SMMU remain independent unqualified work.

The umbrella --shared-coding instrument independently qualifies genuine original
same-process NSXPCListener/NSXPCConnection shared-handle coding and original
non-NSXPC-coder refusal. --shared-cross-process additionally transports a genuine
anonymous endpoint via original libxpc serialization and the original endpoint
accessors, with real peer PID checks. It uses the supplied target's actual third
receive flags argument, not an obsolete two-argument declaration. The peer's
original NSXPCCoder decodes a real Mach send right; the native broker imports its
actual event. After creator waitpid/exit, that NSXPC event executes the real blue
GPU wait/signal task. A distinct genuine Mach import sees its hardware counter
and continues with green GPU work after final handle destruction. All 12288 real
GPU pixels, the original handle reference controls, wrong/stale/raw refusals,
three real imports, exact final cleanup, native CPU 4K/root/AMFI and GPU API
Validation pass. No new bootstrap or deployment authorization is implied.
Normal system discovery/client policy, full compositor UI/desktop and actual
Adreno/SMMU qualification remain open.

The unpublished NativeIOGPUProviderProbe now derives from the actual abstract
736-byte/215-entry IOGPU. Its real original protected C2/D2 and all inherited
operational entry addresses remain intact. The ten required private hooks have
non-returning register captures; a separate publication guard also never returns.
Opaque tail declarations bind original entries directly without introducing C++
callers or claiming recovered source signatures. It is an introspection object,
not a system driver.

Native QEMU observes original construction/init/free/D2 restoring instance count
0 -> 1 -> 0. In the independent start observation, actual original IOGPU::start
reaches required slot 195 with genuine this/provider and the exact original
caller LR. The hook records live CPU translation registers, reports Unsupported
and deliberately stops; startup does not return, and this start object remains
live at the checkpoint. Missing original registration refuses before allocation;
default-off executes original launchd with no probe object. No normal publication,
successful provider startup, system Metal/UI, Adreno execution or active native
GPU SMMU is qualified.

## Original QuartzCore vertex attributes and real buffer geometry

OfflineMetalVertexWire.h adds a 2104-byte, size-discriminated extension of the
existing attachment-pipeline operation. No new kernel selector or permission is
needed. The actual original vertex descriptor is serialized into 31 attributes
and 31 static layouts; the host constructs real Metal descriptors and pipelines.
Known scalar/vector/packed formats are bounded explicitly; unused entries must
be zero. Dynamic strides, patch stepping and out-of-range formats/indices/extents
refuse. No unsupported vertex layout is silently replaced with an empty one.

The original unchanged QuartzCore std_vert0_lpf and attachment_clear_frag_lpf
now execute through actual float4/float2 attributes, a 40-byte vertex stride,
real MVP buffer and updated vertex bytes. Host API Validation qualifies two
GPU draws/all 8192 pixels plus ten framing/layout/foreign-namespace refusals.
Two actual native iPadOS workers then qualify four original-shader GPU draws,
all 16384 geometry-dependent pixels, descriptor mutation independence and real
shared-surface/foreign-resource lifetime controls. All 137 actual requests and
144974 reply bytes reconcile independently; seven GPU submissions complete,
and both workers exit zero with distinct native CPU 4K task/TTBR0 contexts.
New geometry targets never receive uploaded CPU pixels.

The original RenderBox asynchronous regression also passes on the same source
kernel/frontend/backend: original GPU output, genuine events/callback visibility
and cleanup remain intact. Nonempty static vertex input is newly supported;
ordinary system discovery, full display-server composition, original desktop,
Adreno execution and active native-4K GPU SMMU are still unqualified.

## Original client absent-descriptor refusal and cleanup

The unpublished original-provider instrument now declares slot170's actual
descriptor return and uint32 output pointer. The supplied original concrete
method writes a uint32 through x1 and returns its real descriptor through x0;
the actual original client retains it and invokes native createMappingInTask.
This register contract is derived from this target's unchanged code. Other
opaque tail source signatures remain unqualified.

The naked slot170 entry saves the same actual register frame before C++ code.
Its normal observation remains non-returning. Only the explicit own
client-refusal instrument returns NULL for the genuinely absent backend
descriptor, restoring the original LR/SP. All other required callbacks and
publication remain non-returning captures. No descriptor or device success is
supplied, and no original method or class identity is replaced.

The original client factory, custom owning-task/subtype initializer and client
start now execute together. An absent descriptor returns through original error
handling, cleanup, detach and release. QEMU checks actual type1 and high16
subtype controls: the original client count rises by one at the real callback
and returns to its baseline; the own provider count also returns to baseline.
The original factory returns its actual failure with NULL output, no live
attachment/publication and zero GPU protocol traffic. The read-only observer
never dereferences freed client storage. Separate non-returning observations
still inspect the real live original 304-byte client and its stored subtype.

This is genuine refusal/lifetime qualification for the unpublished instrument.
Real shared descriptors, device/resource/queue implementations, successful
original startup, normal system discovery, desktop and native Adreno/SMMU
remain required.

## Native frontend over the bridge

`NativeBridgeMetalFrontend.mm` is the Metal API frontend of the native path:
the metal2vulkan runtime (`bridge/NativeMetalObjects.rs`, a C ABI of the
runtime's device, resources, states, queues, command buffers and encoders)
on Turnip and NativeAdreno. The device is a dynamic subclass of the target's
_MTLDevice adding only its own methods, as in the offline frontend. Implemented:
buffers (also no-copy), textures (also over buffers and over an IOSurface's
own pages, without a copy), metallib and AIR libraries, function-constant
specialization through the target's own accessors, compute and render
pipelines with vertex descriptors and blending, samplers, depth/stencil states,
queues, command buffers with scheduled/completed handlers and GPU times, and
compute, render and blit encoders. An encoder call the runtime refuses fails
its command buffer (status Error, handlers still run); an unimplemented
selector is reported through a hook before the usual exception. Not yet:
events/fences, heaps, argument buffers, indirect commands, drawables and
presentation. No GPU family or feature set is claimed.

`scripts/check-native-metal-frontend-host.py` (umbrella) qualifies it on the
Mac through the public Metal API only, over MoltenVK as the runtime's Vulkan
driver: compute, render with blit readback, rendering into a real IOSurface
read by the CPU, sampling that surface into a second surface, texture compute,
handler order, a refusal, function constants, and translation of the target's
QuartzCore library. That is a host observation of the frontend and runtime;
it does not qualify Turnip, the Adreno 740, its SMMU context or presentation.

Compute descriptor creation and archive population use the same supported
descriptor fields: function constants, thread limit/promise, indirect/binary
function support, call stack depth and buffer mutability. Explicit archive
lists and pipeline option bits reach the runtime for both compute and render
creation. Nonempty compute stage inputs and linked/dynamic libraries are
refused here; empty descriptors materialized by Metal retain their default
meaning. A separate-process, empty-cache archive regression checks strict
hits/misses, descriptor/function-constant identity and actual host GPU output.
The archives contain translator artifacts, not Adreno machine code; later
draw/dispatch variants and a complete cold UI shader pack remain unqualified.

The default compute-function path is separately checked over Turnip and the
software kernel peer in `bridge/driver_cache_host.cpp`. A cold process writes
translation artifacts and a real driver-pipeline record. Another process with
the same directory must report artifact/driver hits and no new misses. The C
bridge exposes the two layers' separate counters for this check. Mesa disk
shader caching is disabled. Commands retire through the software peer, and
their output is unchecked; actual host GPU pixels have separate regressions.
Matching these host caches does not qualify them for a different iOS driver
build or cover every target UI function/constant/dispatch variant.

Bound buffer/texture owners are retained separately from the runtime's GPU
storage handles. The former owns no-copy deallocators, IOSurface references
and buffer/view/heap ancestry. Compute/render bindings, blits, index buffers,
pass attachments and visibility buffers retain those owners until real
completion (or a local refusal which submitted no work). Completion also drops
the IOSurface visibility set, so keeping a completed command does not keep its
old frame storage pinned. Discarding an uncommitted command releases its owners.

`bridge/resource_lifetime_host.mm` drops all caller-owned references while
actual host GPU work is gated on an unsignaled event. It checks raw-buffer
copies/fills, nested buffer-backed views, compute input and a clear-only render
attachment, plus local refusal and uncommitted discard. The no-copy callback
must remain pending and then run exactly once; successful work checks actual
GPU output. The old frontend invokes the callback before submission in all
seven cases. The test owns its mapping separately so that the old failure
never submits a dangling pointer. This qualifies host ownership, not safe
native GPU kernel resource leases, scanout-map reuse or 120 FPS.

IOSurface cache maintenance deduplicates exact live CPU address/length pairs
within each command phase. Independent imports and nested texture views of
one surface previously repeated the same full-range operation before commit
and after completion. Range sorting removes those duplicates, keeping every
distinct range's maintenance and carrying no address identity across phases
or frames. The command's retained owners keep all mappings live during each
phase. No hardware coherence assumption or cross-frame mapping cache is added.

`bridge/surface_flush_host.mm` forwards every intercepted maintenance call to
libSystem and checks real host GPU writes, independent alias reads and shared
surface bytes. For 128 aliases of one allocation the old code makes 256 calls
covering 4 MiB; the new code makes two covering 32 KiB. Two distinct allocations
require four calls. A single alias still requires both phases. Telemetry reports
actual maintenance calls/bytes/time and removed duplicates; the plugin streams
all 48 supported rows. Actual native workload frequency and hardware frame-time
benefit remain unqualified.

IOSurface texture creation now reuses a still-owned no-copy buffer for an exact
surface ID, CPU base and rounded allocation size. The buffer retains the
surface through its own release. A mutex covers lookup and first import; the
cache has weak values and at most 256 associations, so it cannot keep an old
frame's GPU allocation alive. An evicted association may be recreated while
its original resources remain live. General caller-owned no-copy buffers keep
their own objects and deallocators; no remap or rebind API is added.

`bridge/surface_import_host.mm` counts actual frontend import/release calls
while forwarding every call to the unmodified bridge. The separate host-test
`import_count_loader.cpp` forwards all Vulkan requests to the real driver and
counts successful external-host-memory allocations. These are distinct counts:
the runtime already shares sequential live imports. For 128 aliases of one
surface, frontend calls fall from 128 to one while actual Vulkan imports stay
one. A 64-way concurrent first creation instead reproduced seven actual imports
and stale host GPU texels on the previous frontend; the fixed path makes one
and reads the correct GPU output. This also avoids the runtime's documented
concurrent first-import window for this frontend's IOSurface path. The common
native bridge also covers raw no-copy callers, as described below; direct
runtime clients outside that C ABI still need their own synchronization.

Distinct surfaces retain distinct imports. Parallel creation, owner expiry,
work gated behind a real unsignaled GPU event and dropping the caller's last
surface reference all pass. Completed commands release their owners even while
retained. A 514-alias/257-surface case crosses the metadata bound: 258 frontend
objects and 257 Vulkan imports, all GPU bytes correct and all owners released.
The tests do not qualify Adreno timing, target UI concurrency, repeated-blur
frame deadlines, or active native-4K GPU translation.

The common `nmb_buffer_new_no_copy` entry now gates its runtime construction per
device. This covers both `newBufferWithBytesNoCopy:` and IOSurface textures,
including concurrent mixtures, while returning a separate buffer object for
every raw call. The gate covers the live-import lookup, allocation and join;
it ends before returning an object/error, and resource release, GPU work,
completion waits and other APIs do not take it. The C ABI supplies neither a
dma-buf source nor a user runtime deallocator. Frontend deallocators still run
outside the bridge. No upstream runtime patch or shader cache identity change
is introduced by this boundary fix. Distinct no-copy constructors on the same
device share the CPU construction gate; this is not a claim about their timing.

`bridge/no_copy_import_host.mm` starts sixteen simultaneous public Metal calls
in each fresh process. The forwarding loader inserts a one-shot 25 ms delay
before the first actual host-memory allocation to expose the lookup window,
then calls the real driver unchanged. Old code makes sixteen imports for
sixteen raw aliases and nine for eight raw buffers mixed with eight surface
textures. Both now make one. The GPU producer completes before alias readback,
so the check assumes no implicit synchronization for arbitrary raw Metal
aliases. All GPU bytes match, every raw object remains distinct, and each
deallocator runs exactly once. A separate callback creates a buffer from
another thread and waits for it, qualifying release-time reentry without the
construction gate held. These are host allocation/ownership controls, not
Adreno timing or stable UI frame evidence.

`bridge/quartzcore_blur_host.mm` separates bare-descriptor failures from valid
pipeline coverage in the unchanged target library. Its 167 entries comprise
41 vertex, 111 fragment and 15 kernel functions. Four ordinary compute entries
compile; eleven report a tile-attachment requirement when incorrectly requested
as ordinary compute. Of 22 name-matched render pairs, the bare probe's eight
failures are six missing vertex descriptions and two missing MRT attachments.
Compatible authored layouts/formats make all 22 pipelines, without changing
the source library or advertising tile support. These are selected descriptor
recipes, not the target UI's complete function-constant/format/state inventory.

The same control executes the original float/half narrow-blur functions on the
host GPU. An authored seven-tap binomial stencil provides an independent
reference; horizontal and vertical passes run six times for each precision,
checking every output component. No further pipeline making or cache miss is
allowed during the repeated draws. The native frontend exposes structured
work/cache counters for this qualification; cumulative creation diagnostics
must also be positive so disabled logging cannot masquerade as reuse.

The umbrella's `check-quartzcore-blur-host.py` runs cold producer, archive-only
and provisioned-cache processes, with strict empty/changed-descriptor refusals.
The archive contains 22 compatible pairs and the two blur recipes. Archive-only
reload avoids translation but still reports 20 driver misses in the observed
host run. Copying the matching producer cache reports 24 driver feedback hits
and no translation or driver miss. All 24 pipeline objects still have to be
created, and MoltenVK's feedback may describe cached stages; this is not proof
of a native Adreno first-frame deadline. Archives, driver records and raw logs
remain ignored. A different iOS/Adreno profile needs its own artifacts; the
complete initial-image UI pack and stable 120 FPS remain unqualified.

`bridge/quartzcore_cache.mm` runs the same 24 authored descriptor recipes over
actual Turnip/IR3 and the explicitly required software peer. Its offline-only
`graphics_cache_loader.cpp` forwards real calls, enables the queried Vulkan
cache-control feature and can forbid compilation after device initialization.
It independently records real compiler results and feedback. The negative
UUID control mutates a private copy of loaded data; producer files stay intact.
No draw is encoded. Device entry-point priming submits barriers/timestamps
to the software peer, which executes no shader. This is compiler evidence,
separate from the original-blur host GPU pixel control.

The creating API can return a state even when a Vulkan prewarming attempt
failed. Therefore nonnil states cannot qualify driver-cache readiness. In the
observed Turnip graphics-library path, 24 states use 65 actual graphics
creations, 33 with shader application caches. The strict consumer succeeds in
all 65 and reports valid application hits in all 33; empty and UUID-mismatched
caches produce actual compile-required results. The runtime's aggregate
shader-cache hit statistics do not report this graphics-library branch, so the
independent call counters are required. The umbrella records a separate Mesa
feedback fix: its graphics hit flag's AND reduction previously began false,
concealing successful cache hits.

Device initialization itself makes a graphics pipeline before the strict UI
scope and submits initialization work. It is explicitly outside the consumer's
compile-forbidden claim. These checks do not qualify every actual draw variant,
initial-image completeness, native iOS/Adreno shader execution, active GPU 4K
translation or any frame deadline. The control loader is never deployed.

`bridge/quartzcore_cache_ios.cpp` is an own QEMU-only PID 1 harness for this
compiler instrument. It finds the exact own RAM Data UUID and NativeRamBlock
ancestry, establishes the existing readonly union extension view, and spawns
four separate normally linked workers. Actual original dyld/AMFI and 4K
CodeDirectories remain in force. Parent and workers require the genuine 4096
byte ABI; no page-size shim is used. The parent hashes the source library,
archive and every sorted cache file, calls fsync/sync and stops at its own EL0
checkpoint. Its small manifest and complete worker log fit the existing bounded
saved-register captures. Existing bounded own-Data export supplies the full
cache bytes without widening a user-memory read.

The iOS Turnip build passes the same 24-recipe strict compiler controls: 65
successful graphics creations and 33 real application shader-cache hits, with
UUID/empty-cache refusals in separate workers. The actual selected pack has
153 files; readonly extraction matches every byte against the paused iOS
parent's independent manifest. The backing is mutable test Data, and these
workers produce the pack during that boot. A fresh boot provisioned from these
bytes, complete UI/draw variants, authenticated production packaging, Adreno
execution/active GPU 4K translation and display deadlines remain unqualified.

`bridge/quartzcore_cache_provisioned_ios.cpp` reuses the exact volume, manifest
and checkpoint helpers with a consumer-only main. The staged image already
holds the exported iOS archive/cache at the normal consumer paths. Before any
worker starts, the parent hashes that initial pack. Only consumer, wrong-UUID
and empty-driver workers run, in distinct processes; their final pack manifest
must equal the initial one byte for byte. Actual iOS first-consumer checks pass
all 65 compile-forbidden UI graphics creations and all 33 shader cache hits,
with zero translation misses/writes or driver record saves and no boot producer.
Original signing/AMFI/trust, native CPU4K and real worker page ABI checks pass.
This qualifies the selected recipes in an owned test image only.

The separately observed device initialization is exit-guard priming in the
runtime's `device/prime.rs`: it calls the driver entry points before registering
the teardown hook, makes an initially empty pipeline cache, and discards that
cache's bytes. It includes compute and graphics pipeline creation and a
barrier/timestamp submission. The render/compute recording is not submitted;
this is not a semantic GPU probe or a source of measured shader output.
Caching that startup work must preserve its complete priming and teardown
ordering. Device initialization remains outside the UI consumer's strict scope;
complete variant coverage, production authenticated packaging and native
Adreno frame deadlines remain unqualified.

`bridge/patches/0003-cache-device-entry-point-priming.patch` now keeps that
startup cache after all priming succeeds and reloads it in later devices.
Its raw disk/cache path creates no worker and enters no exit-guard section
before priming. Every primed call, the barrier/timestamp batch, waits, failure
cleanup and post-priming hook registration remain. Raw loader words are restored
before each added cache entry point. The optional generated helper also uses
the shared priming cache; generated-command execution is not qualified by the
current controls.

Records reuse the existing bounded, checksummed, atomic driver-cache format.
Their keys include the driver/build UUID, compile profile, instance settings,
priming implementation bytes and relevant feature/queue choices. Invalid or
refused data starts empty; failed priming never persists partial cache data.
An unchanged loaded record is not rewritten. This startup store has separate
counters from the ordinary shader-cache store, so independent driver calls are
used to qualify startup hits.

`bridge/priming_cache_loader.cpp` extends the offline graphics-cache forwarding
loader without changing its previously qualified source. It adds actual compute
feedback and a compiler-forbidden device-open policy. `priming_cache_host.cpp`
opens/closes the device without authored shader work. Both compute and graphics
helpers now report real application-cache hits with compilation forbidden.
Cold, wrong-UUID, truncated and bad-checksum controls refuse and leave zero live
software-peer objects. An exact preserved pre-fix executable still fails during
device open with the same populated cache. Every case retains the one real
priming barrier/timestamp submission; no shader execution is claimed.

The full host GPU/frontend and Turnip controls pass. The instrument and complete
plugin cross-build for ordinary ARM64/iOS17 with own 4K signatures. New priming
cache execution under iOS and a fresh initial image still need qualification.
Earlier iOS UI packs remain evidence for their earlier runtime; regenerate and
verify artifacts for this build rather than silently treating them as current.
These results do not qualify all UI variants, native Adreno execution/4K SMMU,
physical display deadlines or stable 120 FPS. The test loader is never deployed.

`bridge/priming_cache_ios.cpp` now runs the exact priming instrument in actual
iOS/QEMU workers. Four cold processes qualify empty-cache refusal, production,
strict import and wrong-UUID refusal. A second fresh image prepopulates the
actual native record and selects a consumer-only main path: its first worker
hits both compute and graphics helpers while compilation is forbidden; no
producer runs in that boot. The same bounded manifest helpers independently
hash the initial/final record and the readonly exported Data snapshot. The
8,977-byte native record has its actual iOS driver identity, not a relabelled
host header. Original AMFI/signatures/trust/root, active CPU4K and real worker
page ABI checks pass. Both positive and refused workers retain priming and
release all software-peer resources. The peer executes no shader.

This proves the runtime's public priming helper path only. A separate source
audit finds Turnip device creation initializes fifteen clear/blit shader
variants and its empty shaders before these public pipeline calls. Its IR3
compiler is explicitly created with `disable_cache=true`, independent of the
runtime's new cache. That path needs compiler-level measurement and separate
optimization; public Vulkan compile-forbidden flags cannot qualify it. No
complete startup-compilation, UI-variant, native GPU4K or frame-rate claim follows.
