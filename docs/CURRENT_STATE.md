# Current state

Updated: 2026-11-04. Branch: `develop`. Repository: `op12345-project/ppsspp-touch`.

## Progress

- User reports that the Windows build works with independent storage, and that an Android build
  was installed alongside original PPSSPP on a Huawei device. These results were not rechecked
  during this documentation task.
- Target Android package ID: `com.hesham.ppsspptouch`. The current uncommitted
  `android/build.gradle.kts` change sets this ID for the `normal` flavor; an uncommitted
  `android/res/values/strings.xml` change renames the app and shortcut.
- Unified mouse/touch interaction for Tag Force 3, 5, 6, and ARC-V Special remains the main
  development goal. No implementation status for those interactions was verified here.

## Changes in this task

- Added the shared handoff documents and linked both agent instruction files to this state.
- No emulator functionality changed.

## Tests and issues

- Documentation checks: required files exist; both agent files reference this state;
  the `AGENTS.md` diff contains only an added section; `git diff --check` passed.
- Emulator builds, unit tests, pspautotests, and Android device tests were not run for this
  documentation-only task.
- Uncommitted work seen at the first status check must be reviewed by its owner: `android/build.gradle.kts`,
  `android/res/values/strings.xml`, `android/build.gradle.kts.backup`, and `ext/libchdr`.

## Next action

Review the existing Android changes and baseline the current Windows/Android behavior before
implementing a first mouse/touch interaction slice. Preserve the original app and Syncthing setup.
