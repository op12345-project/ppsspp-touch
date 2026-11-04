# PPSSPP Touch roadmap

1. **Establish the fork baseline.** Confirm Windows independent storage and Android coexistence
   with original PPSSPP. Record build and device results without changing the original app or
   existing Syncthing configuration.
2. **Define unified input behavior.** Map mouse and touch gestures to PSP actions, including
   focus, drag, click/tap, and simultaneous keyboard/controller use. Choose a small first game
   scenario and document expected behavior.
3. **Implement and validate the first slice.** Add interaction handling outside the emulation
   core; verify mouse, touch, controller, keyboard, and hybrid input on Windows and Android.
4. **Add game profiles where needed.** Cover Tag Force 3, 5, 6, and ARC-V Special with targeted
   mappings and regression checks while retaining general PPSSPP behavior.
5. **Maintain upstream compatibility.** Reconcile upstream changes, rerun relevant builds and
   tests, and keep fork-specific behavior isolated.

Update this roadmap only when priorities or scope change. Track immediate work in
[CURRENT_STATE.md](CURRENT_STATE.md).
