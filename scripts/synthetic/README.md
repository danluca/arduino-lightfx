# Synthetic FXI4 rhythms

These six grooves use the existing firmware format: four integers (0..255) per
row, played at **60 ms per row**. No firmware rebuild or Python packages needed.
The original audio seeds remain in the parent directory.

Open [preview.html](preview.html) in a browser to compare all six synthetic
loops with the original files. It runs offline and models FXI4's integer IIR
filter and peak hold, with illustrative colors and 32 pixels per band. The
physical strip's palette, length and task scheduling can change the appearance.

| File | Rhythm | BPM | Loop | Text size |
| --- | --- | ---: | ---: | ---: |
| fsi4_seed1.txt | Four on the floor | 125 | 30.72 s | 5,842 B |
| fsi4_seed2.txt | Broken beat | 111.1 | 34.56 s | 6,286 B |
| fsi4_seed3.txt | Half-time | 100 | 38.40 s | 6,696 B |
| fsi4_seed4.txt | Driving syncopation | 142.9 | 26.88 s | 5,149 B |
| fsi4_seed5.txt | Funk / slap bass | 100 | 38.40 s | 6,392 B |
| fsi4_seed6.txt | Hip-hop / trap | 142.9 (half-time feel) | 26.88 s | 4,885 B |

Groove 5 uses sparse kicks, accented slap notes with short decays and rests,
ghost snares, and hats whose offbeats swing one frame late. Alternating bars
include a longer bass note. Groove 6 puts long 808 envelopes in the bass band
and independent short kick punches in low-mid, with a half-time snare and
frame-spaced hat rolls. Both retain quieter breakdowns and phrase variations.

Each band has a musical role: bass kick, syncopated low-mid notes, high-mid
backbeats, and treble hats. Fast attacks, independent decay times, quieter
breakdown bars, and short fills create rhythmic separation. Decay tails wrap
around the loop boundary. These are designed visual envelopes, not an audio
spectrum or a soundtrack synchronization feature.

## Generate and validate

From the repository root:

```powershell
python scripts/create_synthetic_seed.py
python -m unittest discover -s scripts -p test_synthetic_seed.py
```

Generation deterministically overwrites the six files and preview in this
directory. Use `--output-dir PATH` to write elsewhere. Tune the patterns and
decays in `create_synthetic_seed.py`. Keep `FRAME_MS` matched to FXI4's `frameMs`;
changing it only in the script will change the playback tempo. Files are checked
against the existing 22 KiB upload budget, without dropping rows.

## Try on the board

**FXI4 only selects slots 1 through 4.** The default export includes grooves 5
and 6 for preview, but uploading files with those numbers alone will not make
the firmware select them. Map any four groove IDs to the four supported slots:

```powershell
python scripts/create_synthetic_seed.py --slots 5 6 1 3 --output-dir scripts/synthetic/selected
```

This writes funk to slot 1, trap to slot 2, four-on-the-floor to slot 3 and
half-time to slot 4, plus a preview of that selection. Repeated groove IDs are
allowed, e.g. `--slots 5 5 5 5` to always show the funk groove on selection.
The six default exports remain available in the parent directory.

From the selected directory, the existing PowerShell uploader uses these files and skips
audio generation (requires the PowerShell version used by the project scripts):

```powershell
Push-Location scripts/synthetic/selected  # run from the repository root
1..4 | ForEach-Object { ../../upload_audio_seed.ps1 -Variant $_ -Board Dev }
Pop-Location
```

This replaces all four seed slots on the selected board. Use `Tree` instead of
`Dev` for that configured board. FXI4 randomly selects a slot during setup;
switch away from FXI4 and back after uploading. The local `fsi4` spelling is
intentional: the uploader maps it to `fx/fxi4_seedN.txt`, which the firmware reads
as `/ext/fx/fxi4_seedN.txt`. To restore the audio seeds, run the same uploader
from `scripts` instead of the selected output directory. To upload the original
four synthetic grooves, run from `scripts/synthetic` using `../upload_audio_seed.ps1`.

## Why the originals can feel alike

The current audio wrappers generate at 30 FPS but FXI4 consumes one row every
60 ms (16.67 FPS; its `~25 FPS` comment is inaccurate). Without thinning, this
plays the envelope at about 56% of the source tempo. Keeping every third row
and still playing at 60 ms instead plays it at about 167% of source tempo and
can discard brief attacks. The plain text format carries no timing metadata.

The original seeds also have strongly correlated band levels: the largest
pairwise Pearson correlations after the firmware's smoothing are about
0.81, 0.92, 0.97 and 0.95. For synthetic grooves 1 through 4 they are approximately
0.42, 0.09, 0.24 and 0.27. This quantifies band separation, not subjective visual
quality. The firmware filter passes about 77% of each new sample, so it is
relatively responsive; the 60 ms sampling interval and correlated inputs are
better initial targets than simply removing smoothing.

Peak markers still use the firmware's existing slow fall: two level units per
frame after a 250 ms hold (up to roughly eight seconds from full scale). They
can hover above the quicker bars. The preview includes that behavior so you
can judge the seeds with the current renderer before tuning it.
