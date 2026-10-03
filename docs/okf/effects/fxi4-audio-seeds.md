---
type: Playbook
title: FXI4 audio seeds
description: How the FXI4 VU-meter effect gets its rhythm envelopes. Text seed files are generated from MP3s or synthesized, uploaded to /ext/fx/ on the board, and loaded at random.
tags: [effects, audio, fxi4, seeds, upload]
status: stable
generated: { by: claude_code/claude-opus-5-5, at: 2026-10-03T22:20:00Z }
stale_after: 2027-01-03T00:00:00Z
sources:
  - id: fxi
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/src/fxI.cpp
    title: src/fxI.cpp (FxI4::loadSeedFromFile)
  - id: upload
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/scripts/upload_audio_seed.ps1
    title: scripts/upload_audio_seed.ps1
  - id: synth
    resource: https://github.com/danluca/arduino-lightfx/blob/73c7243/scripts/synthetic/README.md
    title: scripts/synthetic/README.md
---

# On the board

When it starts, FXI4 loads `/ext/fx/fxi4_seed<N>.txt`, where **N = `random8() % 4 + 1`** (only 1–4). The file holds rows of four integers from 0 to 255, one value per band (bass, low-mid, high-mid, treble), played at about 60 ms per row. If the file is missing or empty, the effect logs a warning and builds a pseudo-random mono envelope of 1024 entries.[^fxi]

FXI4 does **not** listen to the microphone. It is "audio-seeded": it replays precomputed envelopes. The live microphone path only triggers [effect bumps](/effects/effect-registry.md).

# Generating seeds

* **From music:** `scripts/make_audio_seed.ps1 <mp3> <variant> <offsetMin> <maxMin>` runs `create_audio_seed.py` (librosa, numpy; see `scripts/requirements.txt`) inside a Python venv (`.venv`, `venv`, `penv` or `lfxpy`). The repo includes sample MP3s and `fsi4_seed1-4.txt` outputs in `scripts/`.
* **Synthetic:** `scripts/synthetic/fsi4_seed1-6.txt` are six designed grooves (four-on-the-floor, broken beat, half-time, syncopation, funk, trap). `preview.html` simulates them offline. The README there refers to `create_synthetic_seed.py` and `test_synthetic_seed.py`, which are **not** in this repository; they came with the seed files borrowed from the RP2350 sibling project.[^synth]

# Uploading

```powershell
# generate (if missing) then upload variant 2 to the Dev board
scripts/upload_audio_seed.ps1 -File scripts/Natural.mp3 -Variant 2 -OffsetMinutes 0 -MaxMinutes 3 -Board Dev
```

The script computes the SHA-256 and sends `POST /upload` with headers `X-Token`, `X-Check` and `X-Path: fx/fxi4_seed<variant>.txt`. The firmware maps the path under `/ext/`. The script then prints `/files.json`.[^upload]

# Gotchas

* Local file names start with **`fsi4_`**, but board file names start with **`fxi4_`**. The upload script renames through `X-Path`.
* Variants 5 and 6 are never loaded unless you upload them under names 1–4.

[^fxi]: src/fxI.cpp (FxI4::loadSeedFromFile)
[^upload]: scripts/upload_audio_seed.ps1
[^synth]: scripts/synthetic/README.md
