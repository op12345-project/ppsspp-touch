# Tag Force 3 menu snapshot workflow

This procedure is for the European **ULES01183** disc. It observes one stable menu and produces preliminary highlighted-index candidates. The tool sends no PSP input and has no memory-write command. Run the fork build with a fresh scratch memstick so PPSSPP's config, saves, and states stay separate from the original installation. Keep captures private: RAM snapshots can contain game and personal data.

## Start an isolated Windows session

Build the fork's Windows `PPSSPPWindows` project through `Windows/PPSSPP.sln` if the executable is absent (see [building.md](building.md)). This x64 solution currently places `PPSSPPDebug64.exe` at the repository root; verify its `FileVersion` matches the current commit before use. Use the corresponding ARM64 binary on an ARM64 host. A debugger-only launch was verified on 2026-11-04; game capture still requires the user's ISO.

In **PowerShell terminal 1**, from the repository root, set the path to your existing, unmodified TF3 image and run:

```powershell
$iso = 'D:\Games\Yu-Gi-Oh GX Tag Force 3 EU.iso'  # replace with your real path
$scratch = Join-Path $env:TEMP 'PPSSPP-Touch-TF3'
$memstick = Join-Path $scratch 'memstick'
New-Item -ItemType Directory -Force -Path $memstick | Out-Null
$exe = Join-Path (Get-Location) 'PPSSPPDebug64.exe'
& $exe --memstick="$memstick" --debugger=34567 "$iso"
```

Use a free debugger port. Keep the emulator window open and navigate by keyboard/controller to a menu with **at least three adjacent, clearly highlighted items**. Avoid list wraparound, transitions, and animated selection. Record the exact path to this menu and the visible item names in your investigation notes. Starting with an empty scratch memstick may require reaching the menu without an existing save; if a save is essential, copy it into scratch without modifying the source. Do not use the original PPSSPP executable or memstick.

## Capture and compare

In **PowerShell terminal 2**, from the repository root:

```powershell
$iso = 'D:\Games\Yu-Gi-Oh GX Tag Force 3 EU.iso'  # same existing image as terminal 1
python Tools/tf3_snapshot.py capture --port 34567 --iso "$iso"
```

The tool verifies the debugger's game image path, process ID, `ULES01183` ID, disc version, and primary user RAM mapping before reading. Its default output is under `%TEMP%\PPSSPP-Touch-TF3\TF3-<UTC timestamp>`, outside Git. You may give a different outside-repository folder with `--out`; never point it at game files or the source checkout.

1. Highlight item **A**, wait for the menu to settle, then press Enter in terminal 2. The tool pauses CPU, saves a screenshot and user RAM, and resumes.
2. Move **once** to adjacent item **B** with keyboard/controller. Wait, then press Enter.
3. Move **once** in the same direction to **C**. Wait, then press Enter.
4. Move **once** back to **B**. Wait, then press Enter.
5. Move **once** back to **A**. Wait, then press Enter.

Each numbered folder contains `screen.png`, `user_ram.bin`, and `meta.json` with the read range and SHA-256. A failed or short read removes that partial capture and attempts to resume the CPU. If capture fails, stop and start a new session; do not combine partial sequences. The screenshot is the last completed frame at the paused instant, so compare it visually with the intended highlight.

Run the printed command, for example:

```powershell
python Tools/tf3_snapshot.py compare --session "$env:TEMP\PPSSPP-Touch-TF3\TF3-YYYYMMDD-HHMMSS"
```

`candidates.csv` lists aligned little-endian 8, 16, and 32-bit values matching **0→1→2→1→0** or **1→2→3→2→1**. These are *pattern matches*, not verified selection addresses. The CSV is capped at 1,000 rows by default; totals still report all matches. `--base N` can target another known starting index. Comparison checks all five capture hashes and their game/version/process/range identity and refuses to overwrite an existing CSV.

## Validate a candidate

Repeat the sequence after a fresh boot, then after loading a **copy** of a pre-existing savestate in the scratch memstick. Keep the same disc version and menu path. Inspect the same candidate addresses in each run; the address may move, which suggests a pointer or signature is needed. Capture an idle A→A pair to reject animation counters and visit a different menu to find a screen-state guard. A usable highlighted index must agree with every visible selection, return to its original value at A, stay stable while idle, and be rejected outside the target menu. Do not write candidate values into PSP memory. Record any validated address together with exact disc version and executable fingerprint before considering an input prototype.

Run the tool's synthetic checks with `python -m unittest discover -s Tools/tests -p test_tf3_snapshot.py -v`. Live capture requires the user's disc and a running fork build.
