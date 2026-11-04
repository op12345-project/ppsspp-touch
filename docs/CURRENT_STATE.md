# Current state

Updated: 2026-11-04. Branch: `develop`. Repository: `op12345-project/ppsspp-touch`.

## Verified baseline and findings

- Windows: the user tested the build and settings persistence. `C:\Apps\PPSSPP-Touch\PPSSPPWindows64.exe`
  exists; its `installed.txt` points to `C:\PPSSPP-Touch-Data`, which also exists. This task
  confirmed the paths, not the prior runtime test.
- Android: the user built and installed the APK alongside original PPSSPP on Huawei and tested
  storage isolation and saving. The uncommitted `android/build.gradle.kts` change sets the
  `normal` flavor application ID to `com.hesham.ppsspptouch`. Shared Android labels retain upstream
  values; `android/normal/res/values/strings.xml` gives only Normal the Touch app and shortcut
  labels. The namespace, default application ID, and other flavor IDs retain upstream values.
- Claude's baseline findings were supplied by the user; no separate Claude report is present in
  this checkout. The upstream Android test examples target `org.ppsspp.ppsspp`; the fork-specific
  install check now targets `com.hesham.ppsspptouch` in `docs/android-device-testing.md`.

## Changes and checks

- Added fork-specific Android test commands in `docs/android-device-testing.md`. This task moved
  Touch branding to Normal's resource overlay; no emulator code changed.
- Parsed the shared and flavor resource XML and verified app/shortcut labels: Normal `PPSSPP Touch` /
  `PPSSPP Touch game`; Gold `PPSSPP Gold` / `PPSSPP game`; Legacy `PPSSPP Legacy` / `PPSSPP game`;
  VR `PPSSPP` / `PPSSPP game`. Confirmed Normal's package ID in Gradle. `git diff --check` passed.
- Gradle resource-merge tasks could not run because the wrapper attempted to download Gradle 9.8.0
  and network access was denied. No APK or device test was run in this task.

## Limitations and next action

- Mouse/touch interaction work for Tag Force 3, 5, 6, and ARC-V Special has not begun here.
- The Android changes remain uncommitted. `android/build.gradle.kts.backup` is an untracked copy
  of the upstream Gradle file, and `ext/libchdr` reports untracked content; both were left alone.
- Preserve original PPSSPP and Syncthing. Next, establish a first game interaction scenario and
  baseline keyboard/controller behavior before implementing unified input outside the core.
