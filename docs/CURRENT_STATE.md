# Current state

Updated: 2026-11-06. Branch: `develop`. Repository: `op12345-project/ppsspp-touch`.

## Checkpoint: TF3 Duel Interaction Foundation v0.1 (tag `tf3-duel-foundation-v0.1`)

Status at this checkpoint (ULES01183 v1.02 only; every feature is per-game and off by default):
- **Hand selection** (tap/click to select, tap selected for X, one-tap End Phase hand-limit
  discard): **physically verified by the user** with a Windows mouse and Huawei touch.
- **Field navigation** (tap/click an occupied field card to move the cursor, never X): passed unit
  tests, a fresh-boot read-only probe, and scripted Windows mouse messages on scratch states. It
  **still needs physical Windows mouse and Huawei touch validation.**
- **Not implemented:** field X confirmation (second tap/double tap), attack targeting, effect
  targeting, in-game menu interaction, piles, and the opponent's hand.
- Also included: Language Settings prototype and guard, read-only duel hand and field probes,
  shared mouse/touch pointer and pulse infrastructure, pointer navigation, `Tools/tf3_snapshot.py`.
  Details are in the dated sections below.

## Verified baseline

- Windows: user previously tested build and settings persistence. Installed executable:
  `C:\Apps\PPSSPP-Touch\PPSSPPWindows64.exe`; `installed.txt` points to external
  `C:\PPSSPP-Touch-Data`. This task built a separate Debug executable in the repository.
- Android: user previously installed the Normal APK alongside original PPSSPP on Huawei and
  tested storage isolation and saving. Normal uses `com.hesham.ppsspptouch`; its label is
  `PPSSPP Touch`. Gold, Legacy, and VR retain upstream names. Updated by adb on 2026-11-05; see
  "Windows mouse and Huawei install results".
- Original PPSSPP and Syncthing remain untouched. Baseline branding commit: `04e36e2abc`.

## Phase 1 implementation (uncommitted)

- `UI/TouchNavigation.h` recognizes tap and four swipe directions and queues short, separated
  PSP button pulses. `UI/EmuScreen.*` routes unclaimed in-game mouse/touch input and unmapped
  mouse-wheel steps; it resets pending/held pulses on focus loss, cancel, screen exit, or toggle off.
- `Core/Config.*` and `UI/Settings/ControlsPage.cpp` add a per-game, default-off pointer
  navigation toggle and Cross/Circle/Square/Triangle confirm choice.
- `Common/UI/Root.*`, `Common/UI/UIScreen.*`, and `UI/GamepadEmu.*` expose whether UI elements
  or existing virtual controls/gesture zones claimed a pointer. Existing gesture zones take
  precedence. `Core/ControlMapper.*` combines mapped and synthetic button state so releasing a
  navigation pulse does not release the same button held by keyboard/controller input.
- `unittest/UnitTest.cpp` adds gesture/pulse and mixed-input tests. No emulator CPU/GPU/HLE,
  Android identity/storage/signing, or Syncthing files changed.

## Validation and limits

- Windows x64 Debug `UnitTest` and `PPSSPPWindows` targets built successfully with MSBuild.
  The rebuilt binary passed focused `TouchNavigation`, `NavigationInputMix`, `InputMapping`,
  and `ComboSuppression` tests (4/4); the full unit run reported 98 tests passed.
- Android `assembleNormalDebug --offline` could not resolve the uncached
  `org.gradle.toolchains.foojay-resolver-convention:1.0.0` plugin. Gradle 9.8 and JBR 21 are
  present, but the restricted network prevents dependency retrieval. No Android APK was built.
- No manual Windows gameplay/setting or Android device test was performed. Tap/swipe thresholds,
  wheel behavior, virtual controls, hybrid input, pause/cancel, and toggle persistence need
  verification with the Windows build and a Huawei build. `pspautotests` are unavailable in
  this checkout (`tests/` and Headless binary are absent). `git diff --check` passed.
- The pre-existing untracked `android/build.gradle.kts.backup` and `ext/libchdr` content were
  left alone. The required recursive submodule update completed during the controls investigation.

## Controls investigation (research only)

- `Use Mouse Control` already converts relative mouse X/Y movement into independently mappable
  axes (`MouseDX`/`MouseDY`). In Control mapping, enable it, use each target button's `M` binding,
  and move in the corresponding direction. Four axis bindings can drive D-pad pairs diagonally.
  Mouse sensitivity and smoothing exist; conversion has a fixed 0.1 dead zone, and mapped mouse
  digital axes use a 0.01 threshold. Mouse wheel directions and buttons are mappable keys; wheel
  release delay is configurable independently of mouse movement. No default mouse D-pad bindings.
- Gesture mapping has per-game left/right touch zones with independent swipe directions, sensitivity,
  smoothing, double tap, and analog-stick gesture. It holds configured swipe buttons while motion
  exceeds its threshold and can hold two directions together. It requires on-screen touch controls.
  The on-screen D-pad already supports diagonals (unless disabled), sticky motion, and control layout.
  Analog-to-digital thresholds apply to gamepad sticks/triggers, not mouse axes.
- Uncommitted pointer navigation instead recognizes a released tap or one dominant-axis swipe,
  then emits an 80 ms PSP button pulse with a 20 ms gap. Its unmapped wheel fallback emits pulses.
  Mouse pointer navigation is inactive with `Use Mouse Control`; mapped mouse buttons/wheel and
  claimed UI/virtual-control/gesture touches take precedence. It has no mouse-hover movement,
  diagonal swipe, or game-object hit testing. Direct virtual touch controls and `PSPKey()` gesture
  outputs bypass the mapper's mapped-plus-navigation button merge; mixed-source release needs a
  dedicated regression check before expanding synthetic input. Also check release when `Use Mouse
  Control` is toggled off while an axis is held: the mapper skips mouse-axis events when disabled.
- Recommendation: keep existing key/axis mapping and gesture zones. For continuous movement, first
  validate four mouse-axis-to-D-pad bindings and their decay/release on Windows and Android. If
  tunable hold, dead zone, or joystick behavior is needed, add an opt-in per-game adapter using the
  existing relative axis stream, independent X/Y direction state, and source-aware button merging.
  Keep discrete menu pulses in pointer navigation; choose the mode explicitly so one motion cannot
  send both a held direction and a pulse. Later direct Tag Force selection needs game-specific
  screen/target evidence and should stay outside the emulation core.
- Existing tests: `InputMapping`, `TouchNavigation`, `NavigationInputMix`, `ComboSuppression`,
  `UIFocusNavigation`, `ConfigSettings`. Add mouse X/Y/diagonal and zero-motion release tests;
  wheel/key/button mapping precedence; gesture-zone diagonal and analog tests; pointer-claim and
  cancel tests; and simultaneous touch, keyboard, and controller hold/release tests. No runtime
  or device tests were run for this research task.

## Game-aware interaction investigation (research only)

- Sources inspected: PPSSPP Duo README and `docs/ppsspp-duo.md`, its mod-development guide;
  ygotfToolkit README and `FORMATS.md`; this fork's input, game-status, memory-debugger and
  threading code. GitHub did not serve Duo's individual Java/C++ source files, so its documented
  algorithm/API is verified from maintainer documentation, not independently from those files.
- Duo's Android second-screen mods have game ID/version status, bounded per-frame memory watches,
  u32 signature scans, disc reads, and queued PSP button/analog input. Its Metal Gear Ac!d US
  v1.00 mod locates the hand/Snake/status objects using code pointers, reads card records and
  battle phase, and taps a card by sending left/right only in the hand-selection phase, checking
  that each press advances the game's cursor. Its GX Tag Force EU v1.01 mod already reads a duel
  card, LP, turn side and phase from fixed addresses; it does not document direct card selection.
- ygotfToolkit documents Tag Force 3/5/6/Special disc IDs and shared EHP/card database/CIP
  formats, card-number stability and per-title layout differences. This can inform read-only card
  metadata/art and version fingerprints. Its offline file/save editing is outside this feature;
  it does not establish runtime cursor addresses for the target games. Duo is GPL v2 or later;
  no toolkit license was confirmed, so reuse its documented findings rather than copying code.
  Duo's Android Java views and second-display lifecycle are not a cross-platform engine to copy.
- Proposed design: one disabled-by-default C++ interaction coordinator for Windows/Android,
  fed by normalized mouse/touch events and immutable game snapshots; game profiles recognize
  `DISC_ID`, `DISC_VERSION`, and a verified executable/module fingerprint, validate bounded
  read-only watches/signatures, and publish semantic screen/selection/action state. A shared
  action planner maps hit targets, wheel/swipe, and context menus to existing PSP input paths,
  verifies each cursor move, and cancels on state mismatch, pause, focus loss or savestate load.
  Unknown versions fall back to ordinary PPSSPP controls. No direct guest-memory writes.
- First prototype: one observed, repeatable card-list/menu screen in an exact target Tag Force
  build. Use the existing WebSocket debugger to capture paired screenshots and paused RAM across
  selection changes; identify and validate screen mode, highlighted index and item count in at
  least two sessions. Then prototype pointer hit rectangles and bounded left/right/up/down pulses
  with highlight feedback. The exact screen and memory fields remain unverified hypotheses.
- Risks: stale/false signature matches, pointer races and torn reads, scans on every frame,
  coordinate drift from rendering/layout changes, synthetic/touch/controller release conflicts,
  savestate restoration, and exposing memory or disc reads to untrusted content. Bound scans and
  watches, validate each field and game version, keep data internal and read-only, and clear all
  pending actions/caches on game or state transitions. Any new CPU/memory access hook requires
  separate core authorization and the debugger threading rules. No build or gameplay test ran.

External references: https://github.com/mkaflowski/PPSSPP-Duo/blob/main/docs/ppsspp-duo.md ;
https://github.com/mkaflowski/PPSSPP-Duo/blob/main/.claude/skills/ppsspp-duo-mod/SKILL.md ;
https://github.com/eireetm/ygotfToolkit ;
https://github.com/eireetm/ygotfToolkit/blob/main/FORMATS.md .

## Tag Force 3 debugger preparation (research only)

- Verified in this tree: `--debugger=PORT` (or Settings > Tools > Developer Tools > Allow remote
  debugger) serves JSON WebSocket at `/debugger`, subprotocol `debugger.ppsspp.org`, and the web
  UI at `/debugger/`. `Tools/wsdbg` is the bundled Rust CLI source; Cargo is present, but no
  built `wsdbg` or repo Debug PPSSPP Windows/Headless executable was found at the documented x64
  paths. No game was launched or memory captured.
- Read-only commands: `version` (PID/path), `game.status` (disc ID/version), `memory.mapping`,
  `cpu.stepping` and the `cpu.stepping` broadcast (pause), `gpu.buffer.screenshot`,
  `memory.read` (base64 bytes), `memory.read_u8/u16/u32`, `memory.search` (bounded value/bytes),
  `hle.module.list`, `cpu.status`; `cpu.resume` continues. `input.buttons.press` can drive a
  timed D-pad press during a later controlled experiment. Memory reads are copied on the CPU
  thread; pause before pairing screenshot and RAM so they describe one visible selection.
- Duo's documented GX Tag Force integration is **ULES00600 v1.01**, not TF3. It uses fixed
  addresses `0x09E4D1A0` (cursor card), `0x09E6418C/0x09E64190` (LP),
  `0x09E641AC` (turn side), and `0x09E641B8` (phase), plus disc card/art files.
  The toolkit identifies European TF3 as **ULES01183** and documents its file formats;
  neither source supplies a verified TF3 live menu index, signature, or screen-mode address.
- Proposed experiment, not performed: in a separate repo-built instance and scratch memstick,
  verify `game.status` and record one stable menu path; at visible selections A/B/C/B/A, stop
  CPU, save a screenshot and chunked user-RAM read, then resume and move exactly once. Diff
  u8/u16/u32 candidate sequences and reject timers/animation values with idle and other-menu
  controls. Repeat from a new boot and a pre-existing copied savestate; check candidate bounds,
  module/heap ownership, and exact visual agreement. Avoid full-RAM `memory.search` per frame.
  Only after a validated index and screen guard exist, prototype clicking one visible target by
  bounded D-pad presses, confirming every highlight change. No guest-memory or file writes.

## First TF3 live capture

The first live TF3 run used the repo-root `PPSSPPDebug64.exe` (build
`v1.20.4-4374-g04e36e2abc`) and the user's existing European ISO read-only, with a separate
OS-temp memstick. The debugger confirmed **ULES01183 v1.02**, PID 7112. On its Language Settings
screen, debugger button presses moved the highlight English → Deutsch → Français → Deutsch →
English; the capture tool itself sent no input. Five paused 25,165,824-byte user-RAM snapshots,
screenshots, and metadata are complete in the private OS-temp session
`PPSSPP-Touch-TF3-Live-20261104/captures/TF3-20261104-133023`. Saved screenshot pixels confirm
each highlighted row. Hash/identity comparison passed and produced one 8-bit candidate at
**0x08EEACD9**, values **0/1/2/1/0**; no aligned 16- or 32-bit candidate matched the tested
0/1 starting values. A later paused read at English still returned 0. This is a **candidate for
this menu only**, not a verified game-wide selection index or screen guard.
`gpu.buffer.screenshot` rejected an unpaused preflight request; pausing first worked. A one-vblank
Down press did not move this menu, while three-vblank presses moved exactly one item.

The first capture attempt stopped after A because `tempfile.mkdtemp` gave its stage folder
permissions that another process could not read. `Tools/tf3_snapshot.py` now creates an inherited-
permission scratch stage; the completed second session is readable. The incomplete first session
remains in OS temp and was excluded from analysis. Six synthetic Python tests and the cross-process
scratch-read check passed; no new emulator build, Android test, or separate-boot/savestate test ran.
No debugger memory-write request, ISO/save modification, commit, or push was made. Existing
uncommitted work was preserved.

## TF3 candidate validation

- In PID 7112, three additional idle English captures retained `0x08EEACD9 = 0`; the same
  17-byte neighborhood stayed identical while the animated background changed. On the distinct
  New Game / Continue menu, paused screenshots and RAM showed `0 → 1 → 0` at that address.
- A fresh boot with a new scratch memstick (PID 11620) confirmed ULES01183 v1.02 and repeated
  Language Settings English → Deutsch → Français → Deutsch → English. All five screenshots
  matched their highlights; the same address again held `0/1/2/1/0` and was the sole aligned
  8-bit 0-based match in the full-RAM search (no 16- or 32-bit match). The address is **validated
  for these two menus in this exact disc/build**, not for arbitrary TF3 screens or versions.
- `memory.info.list` places the byte inside the game's `ELF/modtitle` allocation, at offset
  `0x502D9` from that allocation's start. Within a small local block, `0x08EEACD2` was 7 on
  Language Settings and 1 on New Game / Continue; `0x08EEACDC` was 1 vs 0; `0x08EEACDE` was
  1 vs 4. These bytes were stable through selection changes and both Language Settings boots.
  They are **possible screen-state fields**, not decoded or validated screen identifiers.
- All 16 retained captures (first sequence 5, idle 3, second menu 3, fresh sequence 5) had
  readable PNGs, matching ULES01183 v1.02 metadata, expected process identity, and valid RAM
  hashes. Both scratch memsticks had no savestate or save to copy, so savestate reload remains
  pending. No signature scan is needed yet for this exact stable address; module relocation,
  other builds, and savestate behavior remain unknown. Six snapshot-tool synthetic tests and
  `git diff --check` passed. No emulator code, ISO, original installation, or Syncthing files
  changed; no commit or push. The isolated second-boot process was stopped before the next build.

## Experimental TF3 language selection infrastructure

- Added a default-off, per-game **Experimental TF3 language probe** setting. On the emulator's
  CPU thread it reads four range-checked bytes only for ULES01183 v1.02 while the game is running
  and no state operation is busy. It logs the observed Language Settings memory pattern once on
  entry. The neighboring bytes distinguish the two captured menus, but do **not** yet establish
  unique screen identity. No click or touch action is dispatched by this mode.
- Added a pure bounded navigation planner for the five language indices. It models separate
  D-pad presses and releases, verifies each observed index, and cancels on invalid context,
  unexpected movement, pause, focus loss, game change, state-load serial change, or timeout.
  Runtime input is intentionally disconnected until a reliable menu guard and target coordinates
  are verified. No PSP selection value is written.
- Windows x64 Debug `PPSSPPWindows`, `PPSSPPHeadless`, and `UnitTest` builds passed. Full C++
  unit tests, targeted TF3/pointer/input/config tests, and graphics pspautotests passed
  (`488 passed, 0 failed, 0 missing`). The isolated runtime launch identified ULES01183 v1.02;
  it did not reach the Language Settings screen, so the opt-in probe's live read remains untested.
  Android build was blocked before compilation because the installed Gradle wrapper distribution
  is absent and the offline environment cannot fetch it. No ISO, original installation, or
  Syncthing file changed; no commit or push.

## Validation pause (2026-11-05; diagnostics only)

- Synced submodules and built Windows x64 Debug `PPSSPPWindows` and `UnitTest` through the solution.
  Full C++ unit run exited 0; six focused TF3, pointer, mapper, and config tests passed. Six
  snapshot-tool tests and `git diff --check` passed. CI-style graphics pspautotests finished
  `488 passed, 0 failed, 0 missing`, with both ad hoc modes `9/9`.
- Launched the repo Debug build against the existing ULES01183 v1.02 ISO with a new OS-temp
  memstick and opt-in probe enabled. Runtime probe logs, paused read-only debugger captures,
  and screenshots agreed for English, Deutsch, Français, Deutsch, English: `0/1/2/1/0`;
  the neighboring fields stayed `7/1/1`.
- **Guard failure:** after selecting English and leaving Language Settings, the title screen and
  opening scene still held `7/0/1/1`. The current observed pattern therefore matches unsupported
  screens. The planner remains disconnected and no game-aware click or direct PSP memory write
  occurred. Unsupported versions were rejected with zero reads in unit tests; no other game
  version was available for a live run. Native mouse/keyboard UI checks and savestate reload are
  unverified because the Windows Computer Use helper could not connect.

## TF3 Language Settings guard (2026-11-05)

- Paused full-RAM snapshots and screenshots in private OS-temp scratch sessions show that the
  existing `ELF/modtitle` menu fields stay unchanged after leaving Language Settings. A separate
  32-bit word at `0x089218B4` belongs to `ELF/modehsys` and tracks the observed game mode:
  `7` on Language Settings across selections 0/1/2 and idle, `14` in the opening video, `15`
  on the title and New Game / Continue screen, `16` at name entry, and `5` in a duel. Mode `7`
  repeated in three fresh boots and after loading a scratch savestate at French selection 2.
  The nearby `ELF/modtitle` pointer at `0x08EEAD00` also stayed at `0x08EC60A0` through the
  opening and title, so it cannot serve as the screen guard.
- The opt-in read-only probe now requires mode `7` before reading the existing four menu bytes.
  Unknown disc versions, invalid mode reads, stale title-menu fields under another mode, invalid
  menu fields, and out-of-range selections are rejected in unit tests. The rebuilt app logged
  Language Settings selections 0/1 and restored 2; it logged no language match during the
  opening video or title despite the stale index. No direct selection or PSP memory write was
  added. This is an observed guard for ULES01183 v1.02, not a decoded game-wide mode table;
  other builds and all possible transitions remain unverified.
- Windows x64 Debug app and unit-test builds passed; full C++ run passed 99 tests, six focused
  TF3/input/config tests and six snapshot-tool tests passed. CI-style graphics pspautotests
  passed `488/488` with both ad hoc modes `9/9`; `git diff --check` passed.
  No ISO, original installation, Syncthing folder, or Huawei installation changed; no commit
  or push.

## TF3 Language Settings selection prototype (2026-11-05)

- Real 1440x816 and 978x714 captures place the five row bounds at PSP x=140..339,
  y=128..149, 154..175, 180..201, 206..227, and 232..253. The latter capture has a
  978x554 game viewport starting at y=79, confirming the display scale and letterboxing.
  Hit-testing converts pointer coordinates through PPSSPP's calculated output rectangle;
  row gaps and bars have no game-aware target. Rotation other than normal horizontal is excluded.
- The default-off experimental setting now routes left-click and touch taps on those rows
  to the bounded D-pad planner for ULES01183 v1.02 while mode 7 and the menu fields match.
  It reads the highlighted index after each press, releases each direction, and cancels on
  invalid state, timeout, pause, lost focus, state-operation activity, another pointer
  down, or competing keyboard/controller input. It never confirms or writes PSP memory.
  Ordinary input mapping retains priority.
- Windows x64 Debug app and unit-test solution builds passed. Six focused TF3/input tests
  and the full C++ unit suite passed. A rebuilt app booted the exact game in a scratch
  memstick; a scratch savestate restored Language Settings with mode 7 and index 2, and
  debugger D-pad Down moved the index to 3. The rebuilt app produced a 978x714 screenshot
  with the expected letterboxing and highlighted Italiano row. Native UI automation was
  unavailable, so an actual mouse click or touchscreen tap on this build remains unverified.
  Android compilation could not begin because Gradle 9.8 is absent locally and the wrapper
  download was denied. CI-style graphics pspautotests passed 488/488 with both ad hoc
  modes 9/9; six snapshot-tool tests and `git diff --check` passed. No commit or push.

### Manual validation

1. Use the fork's Debug Windows build with a separate scratch memstick and the unmodified
   ULES01183 v1.02 ISO. Enable **Experimental TF3 language probe** in per-game controls.
2. Open Language Settings. Click English, Espanol, Francais, and Deutsch in a mixed order.
   Each click should highlight exactly that row without choosing a language or leaving the
   screen. Repeat with touch if available, then resize the window and test the same rows.
3. Click between rows and in the black bars; no game-aware D-pad movement should occur.
   Start a multi-row click, then press a keyboard/controller direction, pause, switch focus,
   and reload a scratch savestate in separate trials. The pointer navigation should cancel
   and leave physical controls usable. Verify ordinary mouse navigation when enabled.
4. At the title, opening scene, and unrelated menus, clicks must not start game-aware
   navigation. With the setting off, Language Settings clicks must not use this prototype.

## TF3 duel hand-selection research (2026-11-05; research only)

No source, emulator core, ISO, original install, normal save, or Syncthing file changed; no REA
used or needed. New interaction stays unimplemented. ULES01183 v1.02 on Windows x64 Debug only.
Evidence: scripts, savestates, and screenshots in
`%TEMP%\PPSSPP-Touch-TF3-Duel-20261105` (`states\hand6_idx0.ppst`, `hand7_mp1`, `discard_prompt`,
`equip_target`, `tag_control`, `freeduel_menu`...). Route from a new game needs no save: Main Menu
> Free Duel > Normal Duel. 55 paused RAM captures came from two processes and two memsticks.

**Verified addresses** (Normal Free Duel vs Jaden and vs Blair, Tag Duel, savestate reloads, fresh
boot; module bases identical: `modduel_draw 0x0983FB00`, `modduel_eng 0x09BE1400`):

| Address | Meaning | Evidence |
|---|---|---|
| `0x098D5D40` u32 | cursor code: `zone = v & 0x3F`, `index = v >> 6` | player hand `0x16`, opponent hand `0x17`; all 32 zones walked: even = player, odd = opponent; monsters `00-09`, S/T `0A-13`; piles `14-15`, `18-1F` |
| `0x098D5D20` f32 | selected item world X | hand: `floor((i-(n-1)/2)*65)` |
| `0x09E2A534` u32 | player's hand count | 4/5/6/7 match; LP at `-0x0C`, deck count at `+0x04` |
| `0x09BDD0B4` u32 | overlay state | 1 none; 4 game menu/dialog; 6 cut-in/banner; 7 field check; 0 post-duel |
| `0x09BDD37E` u16 | selected menu item | Summon/Set/Details 0/1/2; phase selector values differ |
| `0x09E2D254` u32 | player command ready | 1 on player control, but flickers to 1 for about 33 ms while a confirmed action starts (needs the stability gate) |
| `0x098E94D0` u32 | prompt flags | `0x87` normal; `0xC7` discard and target prompts (bit `0x40`); `0x8F` cut-in |
| `0x09E2D2E0` / `0x09E2D234` u32 | discard / target discriminators | use only with `0xC7`; `D234` was also 1 during an opponent turn |

**Verified state rule:** normal hand selection is duel modules loaded, zone `0x16`, overlay 1,
ready 1, prompt `0x87`, `index < n`. Rejected lookalikes: opponent Draw Phase (`0x16`, overlay 1,
ready 0), discard prompt and equip-target mode (ready 1, `0xC7`), menu open (code unchanged,
overlay 4), results screen (ready 0). After unload, code reads junk and the others read 0.
**Cross discards in the discard prompt**, so "tap selected card = X" must be a deliberate per-state
decision. Target mode lets the cursor walk onto the hand.

**Rejected fields:** `0x089218B4` is NOT a screen ID: 7 during a duel intro, climbs 8-13 in one
duel, rock-paper-scissors read 5 and 16 in separate runs. It looks like a progress counter, so the
"mode" naming and Language Settings guard in `UI/TagForce3LanguageSelection.h` rest on a wrong
reading. That guard held in the duel only because `0x08EEACD2` differed, and `modtitle` is
unloaded there, so those bytes are stale. `0x08FFC11C`, stack values near `0x09FE911C`, and
draw-list words near `0x089BCxxx` are unusable (those moved between boots).

**Hand geometry (PSP px):** `cx = 0.7342*X + 313.80`, max error 0.37 px over 21 measured frames
at n=4/5/6/7, including a letterboxed 5:4 window. Slot spacing 47.75 px with no compression through
n=7; the seventh card overhangs the right edge. Selected frame is 53 wide, top y~201; unselected
top y~219. Cards reflow on hand-size change: X read 115 briefly before settling at 130.

**Navigation:** closed-loop left/right wraps at both ends. 260 random targets, 409 moves at
n=5/6/7, Tag included: 0 wrong moves, 0 hand escapes, X formula matched in all 100 targets where it
was checked. 3-vblank pulses, about 67 ms per step, target at most 0.63 s. 1-vblank pulses dropped
inputs, 2 and above never did (6 samples per length, 3 runs). Verified select then immediate Cross
opened the menu on the target 24/24 (about 67 ms). Up, R, and Circle leave or overlay the hand. The
D-pad still moves the cursor while ready is 0, so wait for ready. Ack loss from
`input.buttons.press` occurred intermittently in the harness; polling memory was unaffected.

**Hypotheses/unverified:** n<=3 and n>=8 layout; Story, Match, Network, rule-variant duels; other
hand-choosing effects and chain prompts; other builds; Android; real pointer input in a duel; field
meanings beyond observed behavior. The first-session debugger window was 150% DPI: its PNGs were
2161x1224 for a 1440x816 request, so use PPSSPP's output rect as the language prototype does.

**Proposed hit test:** viewport to PSP coordinates (reuse existing transform), then
`slot = round((x - cx0) / 47.75)` with `cx0` from n and the formula; accept `y >= 219`, or
`y >= 201` in the selected slot; clamp at x=480; reject n outside 4..7 until validated, and require
live X to match the formula before acting. Reuse: pointer dispatch, tap recognition, cancel paths,
state-serial, and pulse pipeline. Generalize `Planner` to left/right, wrap, and dynamic n; its
4-step cap and direction set are language-specific. Do not reuse the "mode" guard.

**Estimate:** read-only duel profile/classifier + unit tests: small. Selection on Windows: small to
medium, as most pipeline exists. Tap-again/double-tap: small mechanically, medium for prompt policy.
Popup items and description scrolling: unresearched. Android: touch shares the path but is untested.

Tests run here: 6 snapshot-tool tests, `git diff --check`, capture hash/identity checks. Full C++
and pspautotests not rerun (no source change). Scratch memstick 2 received a game save from an
auto-pressed post-duel prompt; it is scratch only.

## TF3 duel hand probe (2026-11-05; default off, read-only, uncommitted)

- `UI/TagForce3Duel.h`: pure detector. Gates, in order: ULES01183 v1.02 (zero reads otherwise);
  `modduel_mgr/draw/eng` active in `g_symbolMap` at measured bases, with their name strings present
  in data memory (not code, which the JIT patches); then fields. Slot formula, viewport-aware hit
  test, 4-7 cards only. `StabilityGate` holds `HAND_SELECTION` until it lasts 12 guest vblanks,
  restarting on any other state, a changed hand size, or a backward counter. No input or write path.
- **New finding:** per-frame reads show `ready` flicker to 1 for about 33 ms, twice, while a confirmed
  summon starts, with every other field looking like hand selection. So one reading proves nothing;
  any later selection work must use the gate. 12 vblanks is a margin over 2 observed, not a proof.
- `UI/EmuScreen.*`: setting `TF3DuelProbe` (per-game, off; Controls > Experimental game interaction)
  logs state changes and, on pointer down, the viewport, PSP position, and slot. It never consumes
  the event. Shared `TF3GameViewport`, `PointerToGameFrame` (in `UI/TouchNavigation.h`), and bounded
  read helpers replace the language prototype's copies. New strings are English-only for now.
- **Language guard:** still default off, not expanded. `Observe` now also requires `modtitle` active
  at `0x08E9AA00` (confirmed live), and the misnamed mode constants became `BOOT_PROGRESS_*`. This
  closes the stale-byte case. It still relies on a progress word reading 7, which held at first boot
  only in testing, so it remains a gate rather than a real screen ID. Language Settings opened
  from Main Menu is untested and should simply fail closed.
- **Tests run:** Windows x64 Debug `PPSSPPWindows` + `UnitTest`; `TagForce3Duel`,
  `TagForce3LanguageSelection`, `TouchNavigation`, `NavigationInputMix`, `InputMapping`,
  `ComboSuppression`, `UIFocusNavigation`, `ConfigSettings`; full unit run (100 passed); 6 snapshot-tool
  tests; `git diff --check`. pspautotests not run (no HLE, CPU, or GPU change). Android not built
  (Gradle wrapper was unavailable previously; not retried).
- **Live, debugger-driven with scratch memsticks:** saved states classified correctly (hand 4/5/6,
  hand 7 on deck, discard, equip target, Tag). 7-card walk logged each slot. Menu reads overlay; summon
  reads settling/not ready until reflow ends; none during an opponent turn or a whole duel to the
  post-duel screens (52 transitions). Real Windows mouse messages at 6 card centers in a letterboxed
  portrait window logged the right slots; raised area, field, panel, and black bar gave -1; discard
  and target gave -1; game state unchanged by clicks. Language clicks still selected rows and gaps
  did nothing; with default config, no logs and no movement. Clicks were posted messages, not a
  physical mouse.
- **Manual tests remaining:** physical mouse in a duel with the probe on, in 16:9 and resized windows
  (read the log via `--log=FILE --loglevel=4`); Huawei touch; Story, Match, and rule-
  variant duels; hand-choosing effects and chain prompts; hands outside 4-7 (must stay rejected);
  Language Settings from Main Menu; rotated layouts (must stay rejected).
- Harness and states: `%TEMP%\PPSSPP-Touch-TF3-Duel-20261105`.

## TF3 duel hand input (2026-11-05; default off, uncommitted)

ULES01183 v1.02 only. Setting: `TF3DuelHandInput` (per-game, off), **Settings > Controls >
Experimental game interaction > Experimental TF3 duel hand selection**. Separate from
`TF3LanguageProbe` and the log-only `TF3DuelProbe`. No guest-memory writes; no emulator CPU, GPU, or
HLE change; duel menus, graphics, and game files untouched.

**Behavior**
- Tap/click an unselected card: verified D-pad pulses to exactly that card, no X. Wraps by the
  shortest path. With the cursor on the board (as at turn start), verified Downs enter the hand first.
- Tap/click the selected card: one X. The game's own menu opens, unchanged.
- Double tap/click another card: moves, verifies the index, then one X. There is no timing window:
  a second tap on the target before arrival adds X, and after arrival it is a tap on the selected card.
- Hand-limit discard only: one tap moves to the card and sends one X. Can't be redirected or doubled.
- A 30-vblank cooldown after X (or until the game reacts) drops further taps, so X can't repeat.

**Implementation**
- `UI/TouchNavigation.h`: new pure `CursorPlanner`, the closed-loop core. The language `Planner` is
  now a thin wrapper over it with the same behavior and tests.
- `UI/TagForce3Duel.h`: `HandActionPlanner`, plus hand-limit classification. Pulses are timed in guest
  vblanks, with a 3 s wall-clock limit. All actions read gated snapshots only, so any state change
  cancels and releases. `StabilityGate` (12 vblanks) now also gates the discard and on-board states.
- `UI/EmuScreen.*`: `TF3DuelTouch` routing, shared by Windows mouse and Android touch.
  `Core/ControlMapper.*`: `MappedButtons()`, so held physical buttons cancel or refuse actions.
- **Two platform conflicts found live, both fixed:**
  - `Windows/MainWindow.cpp`: double-click toggled fullscreen by default, moving the viewport mid
    gesture. It is now off while `TF3DuelHandInput` is on for that game; other fullscreen controls stay.
  - `UI/GamepadEmu.h`: `GamepadComponent` inherited `View::Touch` returning true, so with on-screen
    controls visible every touch looked claimed. This silently blocked card taps and pointer navigation.
    It now returns false; real button claims still come from `usedPointerMask`. `touchHandled_` is the
    only consumer of this value (the one exclusive-touch root, `DisplayLayoutScreen`, has no gamepad).

**Discard evidence.** Actionable only if: `0x098E94D0`=0xC7, overlay 1, ready 1, `0x09E2D2E0`=1,
`0x09E2D234`=0, `0x09E2D210`=4, `0x09E2D218`=1, `0x09E2D2E8`=1, `0x09E2D2F0`=0x800001, hand 7, cursor
in hand, X matches layout. A full-RAM search kept these: constant in 9 captures of the prompt (4
separate events in Normal vs Jaden and Blair, and Tag, other decks and turns), different in all 25
others. **Correction:** `0x09E2D2E0` is not a discard flag by itself: Lightning Vortex's cost shows the
same banner and flag, with `0x09E2D234`=1 and `0x09E2D210`/`218`=3/2.

| State | Result |
|---|---|
| End Phase hand-limit discard, 7 cards | one tap discards |
| Lightning Vortex cost (one sample) | rejected, stays manual |
| Special Summon from hand (Marauding Captain) | rejected |
| Equip target | rejected |
| Attack target, cursor on a hand card | rejected |
| Hand-limit prompt at any other hand size | rejected |

Field meanings are observed behavior, not decoded. Not observed: tribute selection (tried, monsters
kept dying), an AI or Tag partner's hand-limit discard (should fail on ready), other effect prompts.
Anything not matching all fields fails closed.

**Limits**
- 4-7 cards only. Rotated layouts rejected. Normal and Tag Free Duel only; Story, Match, Network unseen.
- Effect discards and other prompts need keyboard, controller, or on-screen buttons.
- On-screen controls keep what they cover. The default layout covers much of the hand row (buttons,
  Start, Select), so **move them in Control Layout on the Huawei**.
- Hit rectangles were measured on Windows; Huawei touch is untested.
- Field cards, piles, menu items, and description scrolling are not implemented.
- New English strings are untranslated.

**Tests run on the final build**
- Windows x64 Debug `PPSSPPWindows`, `UnitTest`, `PPSSPPHeadless` built. Full unit run: 101 passed,
  exit 0; `TagForce3Duel`, new `TagForce3DuelInput`, and the input, config, and language tests passed.
  CI-style pspautotests: 488 passed, 0 failed, 0 missing. `git diff --check` clean.
- Android `assembleNormalDebug --offline` **succeeded** (first time on this project). APK is
  `com.hesham.ppsspptouch`, label `PPSSPP Touch`; all three `libppsspp_jni.so` contain the new code.
  Later installed on the Huawei; see "Windows mouse and Huawei install results".
- Live on Windows, scratch memsticks, posted Windows mouse messages (not a physical mouse or touch):
  245/245 checks plus isolation runs.
  - Selection at 4-7 cards; second click; double click; from all 42 board starts; Normal vs Jaden and
    Blair, and Tag. 16:9, letterbox, pillarbox, portrait, and small windows.
  - Discards: right index in 9 cases over 3 duels, remaining-hand previews checked for two, one discard
    each. Lookalike prompts and open menus: every slot clicked, nothing changed.
  - 64 clicks through a summon's ready flicker and 15 s of opponent turn: none accepted.
  - Cancels: savestate load, CPU pause, off-card click, virtual button. Injected cursor moves: 10/10
    cancelled, no X on a wrong card.
  - Isolation: feature off does nothing; pointer navigation on gives one action per tap; mapped left
    mouse and Mouse Control win; Language Settings stays off; on-screen buttons keep their areas.
- Only simulated in unit tests: focus loss, held keyboard/controller buttons.

**Manual tests: Windows**
1. Run `PPSSPPDebug64.exe --memstick=<scratch folder> "<ISO>"` from the repo, or an installed copy of
   this build. Per-game settings: turn on **Experimental TF3 duel hand selection**. Optionally turn on
   the probe and add `--log=FILE --loglevel=4`. Main Menu > Free Duel > Normal Duel.
2. On your Main Phase, click cards: each moves there with no menu. Click the highlighted card: its
   normal menu opens. Press Circle. Double-click another card: it moves, then its menu opens once.
3. Click a card right at turn start, with the hand pointer on the Deck: it should still reach that card.
4. Keep 7 cards to the End Phase. At "Discard from your hand", click a card once: that card is
   discarded. Use a save state first if you want to retry.
5. These must do nothing when clicking the hand: an open menu, Lightning Vortex's discard, Marauding
   Captain's Special Summon, equip target, attack target, the opponent's turn, summon animations.
6. During a long move: press a key or controller button, open the pause menu, switch windows, load a
   state. The move should stop and controls stay free. Resize the window and repeat step 2.
7. Report: wrong card, more than one action per click, X or discard you didn't ask for, stuck
   direction. Include window size and log lines containing `TF3 duel`.

**Manual tests: Huawei**
1. `adb install -r -d android/build/outputs/apk/normal/debug/android-normal-debug.apk`, then check
   `adb shell dumpsys package com.hesham.ppsspptouch | grep versionName`. This debug build's signing
   may differ from the installed APK: if install is refused, stop and don't uninstall without
   backing up, since uninstalling can delete app data. Original PPSSPP is a separate package.
2. Use a scratch save or copy. Enable the setting per game. In **Control Layout**, move Start, Select,
   fast-forward, and face buttons off the hand row, or hide controls, then repeat Windows steps 2-6.
3. Tap with a fingertip on each card edge too, and report misses. Check on-screen buttons still work
   and that a virtual button stops a move in progress. Rotate only if the game layout allows it: rotated
   layouts must do nothing.

## Windows mouse and Huawei install results (2026-11-05)

**Windows, physical mouse (user-run):** 7 of 8 duel hand tests passed. Test #6, the menu-blocking
test, was not completed, so it is neither passed nor failed. Individual results were not recorded here.

**Huawei MatePad install (TGR-W09, Android 12; serial omitted):**
- Certificates match (SHA-256 `f3fb5ac9...72813f16`, the repo's `android/debug.keystore`). Upgrade from
  `v1.20.4-4372-develop-dirty` (`120044372`) to `v1.20.4-4374-develop-dirty` (`120044374`).
- `adb install -r` succeeded with no `-d`. `firstInstallTime` kept, so this was an update.
- App-private files had the same names, sizes, dates, and checksums before and after the install, and
  its `Android/data` folder held no files before or after. Memstick stays at `/storage/emulated/0/ ppsspp touch`.
- Launched by `am start`: cold start OK, main menu shows `1.20.4-4374`, Tag Force 3 is listed, process
  alive and focused after 8 s, no crash or ANR in logcat.
- After launch: `ppsspp.ini` and `controls.ini` not rewritten; PPSSPP refreshed `.nomedia` markers in
  the Touch memstick. No other new file under `SAVEDATA`.
- `org.ppsspp.ppsspp` (v1.20.4) and Syncthing: version and install times unchanged, neither running.

**Not tested on Huawei:** starting a game, enabling the setting, or any touch input. The APK is
untested beyond the main menu.

**Update 2026-11-06 (user-reported):** duel hand-card mouse and touch interaction passed on Windows
and on the Huawei. Per-test details were not supplied.

## TF3 duel field-card selection research (2026-11-06; research only)

No source, ISO, normal save, original install, or Syncthing file changed; no guest-memory write.
ULES01183 v1.02, Windows x64 Debug (`PPSSPPDebug64.exe`, built 2026-11-05 14:45), scratch memstick,
PID 6136 (stopped). Evidence in `%TEMP%\PPSSPP-Touch-TF3-Field-20261106`: `tools\` (`f.py`,
`fnav.py`, `rects.py`, `diff.py`...), 19 paused RAM captures, screenshots, and new scratch states
`attack_target`, `bp_start`, `opp_prompt`. Older states: `%TEMP%\PPSSPP-Touch-TF3-Duel-20261105\states`.
Input came from debugger presses only; all field data below came from savestates of Free Duel vs Jaden
and one Tag Duel (`tag_control`).

**Zones (cursor code `0x098D5D40`, `zone = v & 0x3F`).** `zone < 0x0A` monster, `0x0A..0x13` S/T;
`side = zone & 1` (1 = opponent); `col = zone >> 1` (monster) or `(zone - 0x0A) >> 1` (S/T), from
that player's own left. Screen column 1..5 = `col+1` for the player, `5-col` for the opponent. Rows wrap through
two piles each (screen column 0 / 6). Verified grid (left to right):

| Row (`0x098D5D48`) | Col 0 | 1 | 2 | 3 | 4 | 5 | Col 6 |
|---|---|---|---|---|---|---|---|
| 1 opp S/T | 1B | 13 | 11 | 0F | 0D | 0B | 19 |
| 2 opp monster | 1D | 09 | 07 | 05 | 03 | 01 | 15 |
| 3 middle | 1F | - | - | - | - | - | 1E |
| 4 player monster | 14 | 00 | 02 | 04 | 06 | 08 | 1C |
| 5 player S/T | 18 | 0A | 0C | 0E | 10 | 12 | 1A |

Row 0 is the opponent hand (`0x17`), row 6 the player hand (`0x16`). The game's own column word
`0x098D5D44` and row word `0x098D5D48` matched this table for all 30 zones reached. Pile meanings are
inferred from icons only (deck `1A/1B`, extra deck `18/19`, graveyard `1C/1D`, field `14/15`,
banished `1E/1F`) and were not needed.

**Navigation (demonstrated).** Left/Right moves one column and wraps across the 7-position row,
including piles; empty zones and face-down cards are never skipped. Up/Down move straight within
columns 1-5 through rows 1, 2, 4, 5; columns 0/6 also pass through row 3. Up from row 1 enters the
opponent hand, Down from row 5 the player hand, each at the card nearest the column's X; Up from
the hand enters row 5 at the column nearest the selected card's X. Down at the hand and Up at the
opponent hand do nothing. Left from `1E` went to `08`, so the planner should leave row 3 vertically.
The same grid applies in Main Phase, Battle Phase, attack-target, and card-target prompts (cursor is
**not** restricted to valid targets). Closed-loop model check (predict each next zone, verify the
code after every 3-vblank press): 260 random field targets, 741 moves over `two_monsters`,
`attack_target`, `equip_target`, `bp_start`, `next_turn_field`, `tag_control`: 0 mismatches,
0 failures; about 64 ms per step, at most 0.95 s per target. Starts included hand, deck, and field.

**Field table (read-only, demonstrated).** Player entries at `0x09E2A55C + 0x14*k`, opponent at
`0x09E2B098 + 0x14*k` (player struct `+0xB3C`; LP at `0x09E2A528`/`0x09E2B064`). `k` 0-4 monster
columns, 5-9 S/T columns, 10 field zone. Word 0 is the card (`& 0x3FFF` = card ID, upper bits
undecoded), 0 = empty. Matched every screenshot checked (face-up, face-down, defense, equip).

**Screen rectangles (PSP px, highlight-frame bounds; identical in Normal and Tag duels).** Tiles
are slanted, so adjacent outer boxes overlap by about 4 px; hit-test by row band, then nearest
centre. The bottom of row 5 (y 201-211) overlaps a raised hand card: give the hand slot priority there.

| Row | y | Column 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|---|
| Opp S/T | 76-97.5 | 221-261.5 | 258-296.5 | 295.5-332 | 331.5-368 | 366.5-404 |
| Opp monster | 98-123 | 214-258 | 254-295.5 | 294-333.5 | 332.5-372 | 370-410 |
| Player monster | 141-172.5 | 200.5-251 | 246-293 | 291-336 | 335-380 | 377-424 |
| Player S/T | 173.5-211 | 190-245.5 | 239.5-291 | 289-339 | 337-388.5 | 382.5-438 |

Board geometry never reflowed in any tested state (unlike the hand). Defense cards sit sideways in
the same tile. No camera movement seen; attack/summon animations run while ready = 0.

**Selection-mode block `0x098D5D00` (demonstrated in these captures, meanings inferred):**

| State | D00 | D04 | D08 | D10/D14 | prompt `98E94D0` | overlay | ready |
|---|---|---|---|---|---|---|---|
| Normal MP1/BP field or hand | 0 | 0 | 0 | 0 | 0x87 | 1 | 1 |
| Attack target (3 cursor positions) | **1** | 0 | 0 | 3 | 0xC7 | 1 | 1 |
| Equip target (3 positions) | 0 | **1** | 0 | 3 | 0xC7 | 1 | 1 |
| Lightning Vortex cost, Special Summon from hand | 0 | 1 | 0 | 3 | 0xC7 | 1 | 1 |
| End Phase hand-limit discard | 0 | 0 | 0 | 0 | 0xC7 | 1 | 1 |
| "Check card details?" Yes/No (opponent turn), neospace prompt | 0 | 0 | 1 | 0 | 0x87 | 4 | 0 |
| Field card menu open | 0 | 0 | 0 | 0 | 0x87 | 4 | 1 |

`0x09E2D234` is **not** a target flag: it is 0 in the attack-target prompt. After a valid target is
confirmed, ready drops and prompt returns to 0x87 within about 70 ms, then D10 steps 3, 4, 2, 0 over
about 0.6 s; after an attack, ready returned to 1 while D10 was still 4. Require D00/D04/D08/D10 = 0
for normal play, and keep the 12-vblank gate. Opponent turn: ready 0 throughout (25 s trace).

**X behaviour (demonstrated).** Normal play: X on a face-up card opens the game's own menu (Show
Details shown for own and opponent monsters); X on an empty zone or opponent face-down S/T did
nothing. Attack target: X on the target attacked; X on empty zones or own empty S/T did nothing; X on
own occupied monsters opened **Show Details** (overlay 4; Circle returns to the prompt). Equip target:
X on the valid monster equipped; empty zones did nothing; X on the equip card's own zone opened a menu,
and Circle returned to the same prompt. Lightning Vortex cost and Special Summon: X on an empty field
zone did nothing. Circle in attack target cancels to the attacker. No per-zone "valid target" field
was found (strided and pairwise RAM searches; render objects near `0x098D0A90` change but are
graphics pointers, unusable).

**Reuse.** `PointerToGameFrame`/viewport, tap/double-tap, cancel paths, state serial, guest-vblank
pulse timing, `MappedButtons()` refusal, cooldown, module/name gates, `StabilityGate`, and the
closed-loop `CursorPlanner` idea. Needed: a 2-D field planner (above model; verify zone after each
press, expected zone predicted), a field classifier that does not depend on hand size 4-7, a field
hit test, and occupancy from the field table. The hand `Observe` returns early for hand sizes
outside 4-7, which would block field input.

**Recommended sequence (each step default off, ULES01183 v1.02 only):**
1. Read-only field probe: classify normal/attack-target/card-target/other, log tapped zone,
   occupancy, and predicted rectangle; unit-test classifier and hit test with captured values.
2. Normal navigation to occupied field zones only (empty-zone taps ignored). Verify every step;
   cancel on any mismatch, state change, pause, focus loss, load, or held physical input.
3. Tap selected card or double tap: X after verifying the zone, with the existing cooldown.
4. Attack target: D00=1 only; one tap moves then X, but only for an occupied opponent monster.
5. Card target: D04=1, only for occupied field zones; accept that a non-target opens Show Details
   (game menu, Circle closes). Hand-limit discard (D04=0) stays hand-only.
6. Later: piles, opponent hand, tribute/multi-select, chain prompts, after more captures.

**Not verified / hypotheses:** fresh-boot repeat (all evidence is savestate-based, same process);
Story, Match, Network, rule variants; tribute and multi-target selection; trap/quick-play target
prompts during the opponent's turn; field-spell and pile taps; attacks on unattackable monsters;
upper bits of card words; meanings of D00-D10 beyond observed values; Android or real pointer input
on the field; other builds.

**Tests run:** focused `UnitTest.exe TagForce3Duel TagForce3DuelInput TouchNavigation
NavigationInputMix InputMapping` (5 passed; existing binary, `EmuScreen.cpp` is newer but not in
it); 6 snapshot-tool tests; `git diff --check`. No rebuild, full suite, or pspautotests (no source
change).

## TF3 duel field probe (2026-11-06; default off, read-only, uncommitted)

Step 1 of the field sequence. No input is sent, no guest memory written, no UI or setting added.
- `UI/TagForce3Duel.h`: `ObserveField` (game/version and module gates shared with the hand probe via
  `DuelModulesPresent`; no hand-size read), `FieldStabilityGate` (12 vblanks, restarts on state or
  occupancy change), zone map/rectangles, `FieldZoneAt`, `HitTestField`. States: dialog, overlay,
  not ready, transition, hand-only prompt, unknown prompt, invalid cursor, settling, then actionable
  `normal`, `attack target`, `card target`. The cursor must agree with the game's row/column words.
  Hand priority: a point the hand probe claims, or under the raised card (center from the cursor's
  own X, so any hand size), is never a field zone. `HitTestField` reports zone, side, card word,
  occupancy, and `accepted` (actionable state); nothing acts on it.
- `UI/EmuScreen.*`: with the existing `TF3DuelProbe` on, logs `TF3 field probe:` state changes and,
  per pointer down, the zone/occupancy/acceptance. Hand selection code paths are unchanged.
- **Findings that change the plan:** (1) target prompts **open** with anim = 1 for about 0.5 s while
  ready = 1 and prompt = 0xC7, and the two animation words (`0x098D5D10/14`) can briefly disagree
  while opening or closing. Both now classify as `transition` (first build logged them as `unknown
  prompt`, also rejected). (2) `card target` (D04) also covers hand-only choices (effect discard
  cost, Special Summon from hand), so it can't by itself authorize field X; step 5 needs another
  discriminator or must stay navigation-only. (3) Board occupancy flickers while cards move during
  the opponent's turn (ready 0); the gate restarts on any occupancy change.
- **Fresh boot (not a savestate):** new scratch memstick, Language > New Game (scratch save created
  there) > Free Duel > Normal vs **Syrus** (opponent not in earlier captures). Probe log: modules not
  loaded until the duel, then `overlay active`/`not ready` through the intro, then `normal` with
  occupancy `0x28020` matching the screenshot (opponent 0x05, 0x0F, 0x11). Summon: two ready
  flickers held as `settling`. Attack and equip (Divine Sword) prompts reached `attack target`/`card
  target` only after opening and gating; confirming each logged not ready/closing anim, then
  `normal`. Full opponent turn (~55 s, three "Check card details?" dialogs): no actionable state.
- **Windows mouse messages (posted, not a physical mouse):** 3 points per zone x 20 zones, sides,
  occupancy, 5 misses, black bars: 171/171 in each of 16:9, portrait letterbox, wide pillarbox,
  small window (normal), plus attack target and card target (1026 checks). Game state unchanged.
  Raised hand card (slot 5 of 6): hand claimed its area; field zones reported beside and above it.
- **Regression (rebuilt app, hand input + probe on):** tap from the deck moved to hand slot 2 without
  X; a field click only logged; tapping the selected card opened its menu.
- **Tests:** Windows x64 Debug `UnitTest` and `PPSSPPWindows` built; new `TagForce3DuelField` plus
  `TagForce3Duel`, `TagForce3DuelInput`, language, touch, input-mix, mapping, combo, UI focus, and
  config tests passed (10); full run 102 passed, exit 0; 6 snapshot-tool tests; `git diff --check`.
  pspautotests not run (no HLE/CPU/GPU change). Android not built.
- **Unverified:** physical mouse/touch on the field, Huawei, Tag Duel on the new build (layout was
  identical in research), Story/Match/Network, tribute/multi-select, piles, other versions.
- Evidence: `%TEMP%\PPSSPP-Touch-TF3-FieldProbe-20261106` (`probe.log`, `probe2.log`, shots, states
  `fb_turn1`, `fb_attack_target`, `fb_card_target`, tools `clicks.py`, `adv.py`, `step.py`).

## TF3 duel field selection (2026-11-06; default off, uncommitted)

ULES01183 v1.02 only. Setting `TF3DuelFieldInput` (per-game, off): **Settings > Controls >
Experimental game interaction > Experimental TF3 duel field selection** (English-only hint).
- **Behavior:** in a gated `normal` board (own turn, no menu, dialog, prompt or animation), a
  click/tap on an **occupied** monster or S/T zone of either player moves the cursor there with
  verified D-pad pulses. **Never presses X**: tapping the already-selected card does nothing.
  Empty zones are consumed and do nothing. Attack/card targets, dialogs, menus, the hand-limit
  prompt, transitions, and settling ignore field taps (they fall through to existing handlers).
- **Code:** `UI/TagForce3Duel.h` adds `PlanFieldMove` (grid model: horizontal first on the current
  row, then vertical; leaves hands and middle piles first; only the row is predicted when leaving
  a hand) and `FieldActionPlanner` (5-vblank press, wait up to 30 vblanks for the cursor to land on
  the predicted zone, 2-vblank gap; at most 12 steps, 3 s). It cancels and releases on focus,
  pause, savestate (serial), clock going back, held mapped buttons, timeout, any non-`normal`
  state, a board change, an unexpected cursor, or a new press. `UI/EmuScreen.*` routes taps
  through the same pointer, viewport, mapping, virtual-control and pointer-navigation guards as the
  hand; the hand wins where both could claim a point, and a field tap is refused while the hand's
  X cooldown runs. `Core/Config.*`, `UI/Settings/ControlsPage.cpp`: the setting.
  `Windows/MainWindow.cpp`: double-click fullscreen is also off while this setting is on.
- **Tests (Windows x64 Debug):** `UnitTest` and `PPSSPPWindows` built. New `TagForce3DuelFieldInput`
  (simulated cursor: all 20 targets from 30 starts incl. both hands, deck, piles, middle piles, 3
  hand columns; max 7 steps; rejections; every cancel reason releases the held direction; ignored,
  wrong-way and drifting cursor) passed with the field, hand, and input tests; full run 103 passed,
  exit 0. `git diff --check` clean. pspautotests not run (no HLE/CPU/GPU change).
- **Live (posted Windows mouse messages, not a physical mouse; scratch memstick and states):**
  - Navigation: every occupied zone, twice per state (in shuffled order, then reversed after a
    reload), across `fb_board` (fresh-boot duel vs Syrus: player monster + equip, opponent monster
    + two set S/T), `two_monsters`, `next_turn_field`, `fb_turn1`, and Tag Duel `tag_board`
    (face-down own monster and set trap), starting from hand, deck and field: all arrived, no X, no
    menu. Empty-zone clicks (up to 6 per state): no change, no tap accepted. Runs: 99 checks with
    12 failures, all from `dp_tag_0`, which turned out to be the hand-limit prompt where rejection
    is correct; then 40/40 on `tag_board` and `fb_board`.
  - Rejections: attack target, card target, "Check card details?" dialog, hand-limit discard
    prompt, open card menu: clicks on 0x05/0x04/0x0F changed nothing. Clicked the opponent monster
    every 50 ms through a summon: the only accepted tap came after the log reached `normal`.
  - Cancels: savestate load and CPU pause mid-move cancelled; a second click mid-move retargeted.
    **Key-press cancel did not run live**: posted `WM_KEYDOWN` does not reach PPSSPP's raw-input
    keyboard path, so the move completed. Covered only by unit tests and the `key()` hook.
  - Hand regression: deck to slot 2 without X; tap selected opens its menu; hand to opponent monster
    to hand slot 4; hand-limit discard still one tap.
  - With pointer navigation also on: occupied and empty field taps behaved as above; a tap on the
    phase bar (not a card) still went to pointer navigation, which sent its confirm button and
    opened the card menu. That is existing pointer-navigation behavior, unchanged.
- **Android:** `./gradlew assembleNormalDebug --offline` with `JAVA_HOME` set to Android Studio's
  `jbr` built `android/build/outputs/apk/normal/debug/android-normal-debug.apk` (2026-11-06 11:30);
  arm64-v8a, armeabi-v7a and x86_64 `libppsspp_jni.so` contain the new field-input strings. Not
  installed on the Huawei.
- **Not tested:** physical mouse, Huawei touch, focus loss and held
  keyboard/controller buttons live, virtual buttons over the field, rotated layouts (rejected by
  code), Story/Match/Network, piles and opponent hand (not targets), other versions.
- Evidence: `%TEMP%\PPSSPP-Touch-TF3-FieldProbe-20261106` (`field.log`, `field2.log`, states
  `fb_board`, `tag_board`, tools `fsel.py`, `live_field.py`, `live_field2.py`, `nav_conflict.py`).

**Manual tests: Windows**
1. Run `PPSSPPDebug64.exe --memstick=<scratch folder> "<ISO>"` from the repo (built 2026-11-06
   11:16). Per game, turn on **Experimental TF3 duel field selection** (and hand selection if you
   want both). Optional: the probe and `--log=FILE --loglevel=4` (`TF3 field input:` lines).
2. On your Main Phase with cards on both sides, click each card on the field, yours and the
   opponent's, monsters and Spell/Traps, face-up and face-down. Each click moves the cursor to that
   card and shows its details; no menu opens. Click the selected card again: nothing happens.
3. Click empty zones and the board edges: nothing happens. Click hand cards: hand behavior as before.
4. During a summon or attack animation, the opponent's turn, a card menu, a dialog, attack or
   effect targeting, and the End Phase discard: field clicks must do nothing.
5. During a long move (hand to the opponent's far Spell/Trap): press a key or controller button,
   open the pause menu, switch windows, load a state. The move stops and controls stay free.
6. Resize/letterbox the window and repeat step 2. Report wrong card, any X or menu from a field
   click, stuck direction, or movement in step 4, with the window size and `TF3 field` log lines.

**Manual tests: Huawei**
1. Install with `adb install -r` (check `dumpsys package com.hesham.ppsspptouch | grep versionName`).
   Same signing caution as before: if refused, stop; don't uninstall.
2. Enable the setting per game. Move on-screen controls off the field in Control Layout or hide them.
3. Repeat Windows steps 2-6 with fingertip taps, including card edges. Check that on-screen buttons
   still work and that pressing one stops a move in progress.

## Next action

Physical Windows mouse and Huawei touch runs of the field-selection steps above. Then decide how
to tell field card targets from hand-only card choices before any target confirmation work. Harness
and states: `%TEMP%\PPSSPP-Touch-TF3-FieldProbe-20261106`, `%TEMP%\PPSSPP-Touch-TF3-Field-20261106`,
and `%TEMP%\PPSSPP-Touch-TF3-Duel-20261105`. Stop only PIDs you started: other PPSSPP instances may
be the user's. `--log` opens a console window owned by the same process.

## Manual gameplay handoff (2026-11-05)

*Closed: this session is no longer running. The user reported that Windows mouse selection passed; touch and Android remain untested.*

- **Ready and running:** `C:\Dev\ppsspp-touch\PPSSPPDebug64.exe` (Windows x64 Debug,
  built 2026-11-05 08:56, PID 35348, debugger port 34796). It uses only
  `%TEMP%\PPSSPP-Touch-TF3-Prototype-Layout-20261105\memstick`
  and the user's existing read-only European ISO (outside the repository).
  A separate scratch state (`...\PPSSPP-Touch-TF3-Guard-Boot2-20261105\language_2.ppst`)
  was loaded. The app window is responsive; a debugger screenshot at
  `...\PPSSPP-Touch-TF3-Prototype-Layout-20261105\manual-ready.png` shows Language Settings
  with Français highlighted. Debugger confirms ULES01183 v1.02, mode 7, index 2; CPU resumed.
- **Setting:** `TF3LanguageProbe = True` is already set in the scratch
  `memstick\PSP\SYSTEM\ppsspp.ini`. In the UI it is **Settings > Controls > Experimental game
  interaction > Experimental TF3 language probe**. It defaults off in normal configs.
- **Manual test:** Left-click the middle of English, Deutsch, Italiano, and Español in a mixed
  order. Each click should move the highlight to exactly that row, then stop. No language
  should be confirmed and Language Settings should remain visible. Click row gaps and black
  bars (no game-aware movement), then test keyboard/controller D-pad and ordinary mouse
  navigation. Report no movement, wrong row, multiple movements, unexpected confirmation,
  stuck D-pad, or any action outside Language Settings as failures. Do not close this instance
  until the test is done.
- **Status and limits:** This build contains the measured hit rectangles, shared mouse/touch
  dispatch, guarded read-only selection probe, and bounded D-pad planner. Previous Windows
  Debug builds, full C++ unit suite, six focused TF3/input tests, six snapshot-tool tests,
  graphics pspautotests (488/488; ad hoc 9/9 in each mode), and `git diff --check` passed.
  This handoff only launched and verified the existing build; it reran no suites. Actual
  click/tap selection, cancellation during a move, and Android remain unverified. Native
  computer-use connection is unavailable, and Android lacks the cached Gradle 9.8 wrapper.
  The guard is limited to observed ULES01183 v1.02 Language Settings states; other menus,
  versions, rotated layouts, and direct confirmation are unsupported. No known click failure
  has yet been observed. No ISO, normal installation, saves, Syncthing, or Huawei files changed;
  no commit or push.
