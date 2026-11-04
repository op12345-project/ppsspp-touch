# PPSSPP Touch decisions and constraints

- **Fork identity:** `op12345-project/ppsspp-touch` uses `develop` for development. Android's
  intended package ID is `com.hesham.ppsspptouch` so it can coexist with original PPSSPP.
- **Isolation:** Keep the original PPSSPP installation and existing Syncthing configuration
  untouched. Maintain independent fork storage across supported platforms.
- **Input direction:** Unify mouse and touchscreen interactions for PSP games, starting with
  Tag Force 3, 5, 6, and ARC-V Special. Preserve keyboard/controller support and hybrid input.
  Use game-specific interaction profiles when general behavior is insufficient.
- **Core boundary:** Do not modify the emulation core without explicit authorization. Preserve
  existing PPSSPP functionality and upstream compatibility.
- **Evidence:** Treat current code and Git history as the source of truth. Record observed test
  results in [CURRENT_STATE.md](CURRENT_STATE.md); distinguish them from reported status.

Record new architectural decisions here only when they constrain future work.
