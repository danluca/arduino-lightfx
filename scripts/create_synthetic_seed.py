"""Generate musical FXI4 envelopes using only the Python standard library.

Run from any directory: python scripts/create_synthetic_seed.py
One row is one 60 ms firmware frame; there is no resampling or row thinning.
"""

import argparse
import json
import math
from pathlib import Path

FRAME_MS = 60
BARS = 16
MAX_FILE_BYTES = 22528
# Integer frames per beat make both the beat and the loop exact at 60 ms/frame.
GROOVES = (
    ("Four on the floor", 8, (0, 1, 2, 3), (.5, 1.75, 2.5, 3.5)),
    ("Broken beat", 9, (0, 1.5, 2.75), (.75, 1.75, 2.5, 3.75)),
    ("Half-time", 10, (0, 1.75, 3), (.5, 1.25, 2.75, 3.5)),
    ("Driving syncopation", 7, (0, 1, 2, 3.5), (.5, 1.5, 2.75)),
    ("Funk / slap bass", 10, (0, 1.75, 2.5), ()),
    ("Hip-hop / trap", 7, (), ()),
)


def generate(variant):
    """Return looped frames x four bands, ordered bass through treble."""
    if not 1 <= variant <= len(GROOVES):
        raise ValueError("Unknown groove")
    _, frames_per_beat, kicks, notes = GROOVES[variant - 1]
    count = BARS * 4 * frames_per_beat
    frames = [[0] * 4 for _ in range(count)]

    def hit(band, beat, amplitude, decay_ms, hold=1):
        # A full-frame attack preserves short hits at this modest frame rate.
        # Wrap tails into the start so the phrase loops without a reset to silence.
        start = int(beat * frames_per_beat + .5)
        for age in range(hold + math.ceil(6 * decay_ms / FRAME_MS)):
            value = round(amplitude * math.exp(-max(0, age - hold + 1) * FRAME_MS / decay_ms))
            slot = frames[(start + age) % count]
            slot[band] = max(slot[band], value)

    for bar in range(BARS):
        origin = bar * 4
        breakdown = bar in (6, 7, 14)
        if variant == 5:
            # Sparse kick, with bass answering it in accented sixteenth notes.
            for i, beat in enumerate(kicks):
                if not breakdown or i == 0:
                    hit(0, origin + beat, 245 if i == 0 else 220, 85)
            slaps = ((.5, 245), (.8, 90), (1.25, 175), (2.25, 235),
                     (2.8, 85), (3.5, 210), (3.8, 120))
            for i, (beat, amplitude) in enumerate(slaps):
                if breakdown and i not in (0, 3):
                    continue
                # Every other bar ends with a longer bass note.
                sustain = bar % 2 == 1 and i == 5
                hit(1, origin + beat, amplitude, 130 if sustain else 55,
                    hold=2 if sustain else 1)
            for beat in (1, 3):
                hit(2, origin + beat, 180 if breakdown else 240, 65)
            if not breakdown:
                for beat in (1.75, 2.75):
                    hit(2, origin + beat, 75, 40)  # ghost snares
            for step in range(8):
                if breakdown and step % 2 == 0:
                    continue
                # One frame of swing on the offbeat at 100 BPM.
                beat = step / 2 + (.1 if step % 2 else 0)
                hit(3, origin + beat, 230 if step % 2 else 95, 40)
            if bar % 4 == 3:
                hit(2, origin + 3.75, 165, 55)
            continue
        if variant == 6:
            # Slow 808 envelope below independent short kick punches.
            hit(0, origin, 250 if not breakdown else 185, 360, hold=3)
            if not breakdown:
                hit(0, origin + (2.75 if bar % 2 else 3.25), 220, 220, hold=2)
            for i, beat in enumerate((0, 1.5, 2.75, 3.5)):
                if not breakdown or i == 0:
                    hit(1, origin + beat, 245 if i == 0 else 210, 55)
            hit(2, origin + 2, 240, 75)  # half-time snare
            if bar % 4 == 3:
                hit(2, origin + 3.75, 105, 45)
            for step in range(8):
                if step == 6 or (breakdown and step % 2 == 0):
                    continue  # space before the end-of-bar roll
                hit(3, origin + step / 2, 210 if step % 2 else 100, 35)
            if bar % 2 == 1 and not breakdown:
                # Frame-spaced roll: no sub-frame events lost to quantization.
                for offset, amplitude in enumerate((115, 185, 135, 240)):
                    hit(3, origin + 3 + offset / frames_per_beat, amplitude, 25)
            continue
        # Deliberate arrangement: quieter bars, returns, and a fill every fourth bar.
        for i, beat in enumerate(kicks):
            if breakdown and i > 0:
                continue
            hit(0, origin + beat, 245 if i == 0 else 218, 105)
        for i, beat in enumerate(notes):
            if breakdown and i % 2:
                continue
            hit(1, origin + beat, (190, 230, 165, 210)[(i + bar) % 4], 145, hold=2)
        for beat in ((2,) if variant == 3 else (1, 3)):
            hit(2, origin + beat, 145 if breakdown else 240, 85)
        for step in range(8):
            if breakdown and step % 2 == 0:
                continue
            # Offbeat hats dominate; downbeat hats stay small to separate the bands.
            hit(3, origin + step / 2, 205 if step % 2 else 85, 55)
        if bar % 4 == 3:
            for i, beat in enumerate((3.25, 3.5, 3.75)):
                hit(2, origin + beat, 130 + i * 45, 65)
            hit(3, origin + 3.5, 235, 100)
    return frames


def encode(frames):
    if not frames or any(len(row) != 4 or any(not 0 <= v <= 255 for v in row) for row in frames):
        raise ValueError("Expected nonempty four-band frames with values in 0..255")
    data = "".join(" ".join(map(str, row)) + "\n" for row in frames).encode("ascii")
    if len(data) > MAX_FILE_BYTES:
        raise ValueError("Seed exceeds the upload budget; shorten the phrase, never thin rows")
    return data


def write_preview(path, datasets):
    # Fully offline preview. The IIR, integer rounding and peak logic match FXI4.
    # Colors and geometry are illustrative; the physical strip uses its own palette.
    page = '''<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>FXI4 rhythm preview</title>
<style>
body{background:#11151b;color:#e5edf6;font:16px system-ui;max-width:900px;margin:40px auto;padding:0 20px}
select,button{font:inherit;padding:8px;background:#263342;color:inherit;border:1px solid #68788c;border-radius:5px}
canvas{width:100%;background:#080b10;margin:20px 0}p{line-height:1.5;color:#b9c7d8}
</style>
<h1>FXI4 rhythm preview</h1>
<label>Sequence <select id="sequence"></select></label> <button id="pause">Pause</button>
<p id="details"></p><canvas id="meter" width="900" height="350"></canvas>
<p>Left to right: bass, low-mid, high-mid, treble. Top: four upright meters.
Bottom: the same levels laid out along a strip. Playback uses 60 ms per row,
firmware smoothing (196/255), and the existing 250 ms peak hold.
Colors are illustrative. Actual LED count, palette and scheduling affect the result.</p>
<script>
const datasets = __DATA__;
const select = document.getElementById('sequence'), canvas = document.getElementById('meter');
const ctx = canvas.getContext('2d'), colors = ['#fa6565','#ffc05c','#53dab5','#66aaff'];
datasets.forEach((d,i)=>select.add(new Option(d.name,i)));
let frame=0, hist=[0,0,0,0], peaks=[0,0,0,0], peakTs=[0,0,0,0], tick=0, paused=false;
function reset(){frame=0;hist.fill(0);peaks.fill(0);peakTs.fill(0);tick=0;
document.getElementById('details').textContent=datasets[select.value].detail;}
select.onchange=reset;
document.getElementById('pause').onclick=function(){paused=!paused;this.textContent=paused?'Play':'Pause';};
function draw(){
 if(paused)return;
 const rows=datasets[select.value].frames, row=rows[frame];
 ctx.clearRect(0,0,900,350);
 for(let b=0;b<4;b++){
  hist[b]=Math.floor((hist[b]*59+row[b]*196)/255);
  if(hist[b]>=peaks[b]){peaks[b]=hist[b];peakTs[b]=tick;}
  else if(tick-peakTs[b]>250)peaks[b]=Math.max(0,peaks[b]-2);
  const lit=Math.floor(hist[b]*32/255), peak=Math.floor(peaks[b]*32/255);
  for(let p=0;p<32;p++){
   ctx.fillStyle=p===peak?'#ffffff':p<lit?colors[b]:'#1c2633';
   ctx.fillRect(65+b*215,235-p*6,90,4);
   ctx.fillRect(15+b*220+p*6.2,285,4.8,18);
  }
  ctx.fillStyle=colors[b];ctx.font='15px system-ui';
  ctx.fillText(['Bass','Low-mid','High-mid','Treble'][b],65+b*215,263);
 }
 ctx.fillStyle='#b9c7d8';ctx.fillText(`${(frame*.06).toFixed(1)} / ${(rows.length*.06).toFixed(1)} seconds`,15,333);
 frame=(frame+1)%rows.length;tick+=60;
}
reset();draw();setInterval(draw,60);
</script></html>'''
    path.write_text(page.replace("__DATA__", json.dumps(datasets, separators=(",", ":"))), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=Path(__file__).resolve().parent / "synthetic")
    parser.add_argument("--slots", type=int, nargs=4, choices=range(1, len(GROOVES) + 1),
                        metavar="GROOVE", help="Map four groove IDs to firmware slots 1..4; default exports all grooves")
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    datasets = []
    variants = args.slots or range(1, len(GROOVES) + 1)
    for slot, variant in enumerate(variants, 1):
        name, frames_per_beat, _, _ = GROOVES[variant - 1]
        frames = generate(variant)
        data = encode(frames)
        path = args.output_dir / f"fsi4_seed{slot}.txt"
        path.write_bytes(data)
        detail = f"{60000 / (FRAME_MS * frames_per_beat):.1f} BPM; {BARS} bars; {len(frames) * FRAME_MS / 1000:.2f} seconds; {len(data)} bytes"
        print(f"{path.name}: {name}; {detail}")
        datasets.append(dict(name=f"Seed {slot}: {name}", detail=detail, frames=frames))
    for variant in range(1, 5):
        original = Path(__file__).resolve().parent / f"fsi4_seed{variant}.txt"
        if original.exists():
            rows = [list(map(int, line.split())) for line in original.read_text().splitlines() if line.strip()]
            if not rows or any(len(row) != 4 or any(not 0 <= v <= 255 for v in row) for row in rows):
                raise ValueError(f"Invalid original seed: {original}")
            datasets.append(dict(name=f"Original {variant}", detail="Original audio seed at actual firmware playback speed (60 ms per row)", frames=rows))
    write_preview(args.output_dir / "preview.html", datasets)


if __name__ == "__main__":
    main()
