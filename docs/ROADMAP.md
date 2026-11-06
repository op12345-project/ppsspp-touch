# PPSSPP Touch roadmap

1. **Establish the fork baseline.** Confirm Windows independent storage and Android coexistence
   with original PPSSPP. Record build and device results without changing the original app or
   existing Syncthing configuration.
2. **Validate generic input and choose a first game screen.** Test existing mouse-axis mapping,
   touch gestures and uncommitted pointer navigation, including diagonal and hybrid input. Capture
   one exact Tag Force menu/card-list screen with synchronized screenshots and read-only RAM dumps.
3. **Prove game-state observations.** For that exact disc ID/version, validate screen mode,
   highlighted item and list bounds across two sessions and a savestate reload. Keep unsupported
   versions on ordinary controls; measure scan/watch cost before enabling them in normal builds.
4. **Build a shared interaction engine.** Use platform-independent pointer events, validated
   read-only snapshots, a game profile, target hit testing and a button-action planner that checks
   each selection change. Preserve keyboard/controller/virtual-touch inputs and cancel pending
   actions on pause, focus, game and savestate transitions. Add Windows/Android regression tests.
5. **Expand profiles and interface actions.** Add contextual face-button menus, right-click/long
   press, scrolling, optional overlays, and verified profiles for Tag Force 3, 5, 6 and ARC-V
   Special. Reuse the toolkit's documented card metadata/art formats only where useful; do not
   edit discs or saves.
6. **Maintain upstream compatibility.** Keep fork-specific behavior opt-in and isolated, reconcile
   upstream changes, and rerun relevant builds and tests. Obtain separate authorization before
   adding any emulation-core memory hook.

Update this roadmap only when priorities or scope change. Track immediate work in
[CURRENT_STATE.md](CURRENT_STATE.md).
