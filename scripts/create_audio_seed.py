# generate_audio_seed.py
import sys, numpy as np, librosa

# Usage: python generate_audio_seed.py input.mp3 output.txt 8 25 [--stretch_p 2 98] [--contrast 1.0] [--transient 0.0]
# - stretch_p: lower and upper percentiles for stretch (default 2..98)
# - contrast: contrast amount (<=1 keeps mild, >1 stronger). Default 1.0
# - transient: spectral flux boost amount (0.0 disabled, >0 adds transient emphasis). Default 0.0
infile = sys.argv[1]
outfile = sys.argv[2]
segments = int(sys.argv[3])  # number of LED segments/bands
fps = float(sys.argv[4])     # frames per second for the LED effect (default FXI4 ~25 FPS)

# Optional args parsing (very small, positional first 4 args expected)
def _get_opt(arg_name, default, cast=float):
    if arg_name in sys.argv:
        i = sys.argv.index(arg_name)
        if i + 1 < len(sys.argv):
            return cast(sys.argv[i+1])
    return default

# percentiles come as two ints after --stretch_p
stretch_p_low = 2
stretch_p_high = 98
if "--stretch_p" in sys.argv:
    i = sys.argv.index("--stretch_p")
    try:
        stretch_p_low = int(sys.argv[i+1])
        stretch_p_high = int(sys.argv[i+2])
    except Exception:
        pass

contrast_amount = _get_opt("--contrast", 1.0, float)
transient_boost = _get_opt("--transient", 0.0, float)
# Optional maximum audio length (minutes) to analyze. If omitted or <=0, process whole file.
max_minutes = _get_opt("--max_minutes", 0.0, float)
offset_minutes = _get_opt("--offset_minutes", 0.0, float)
offset_sec = offset_minutes * 60.0 if offset_minutes > 0.0 else 0.0

# Load mono audio (librosa handles mp3 via audioread)
if max_minutes and max_minutes > 0.0:
    duration_sec = float(max_minutes) * 60.0
    y, sr = librosa.load(infile, sr=None, mono=True, offset=offset_sec, duration=duration_sec)
else:
    y, sr = librosa.load(infile, sr=None, mono=True, offset=offset_sec)

# Hop length chosen to reach target fps
hop_length = max(1, int(sr / fps))

# Mel spectrogram with `segments` bands
S = librosa.feature.melspectrogram(y=y, sr=sr, n_mels=segments, hop_length=hop_length, n_fft=2048, power=2.0)

# Convert to dB and normalize to 0..1 per-band, then to 0..255
S_db = librosa.power_to_db(S, ref=np.max)
# Dynamic range compression & clipping (keep original behaviour)
S_db = np.clip(S_db, S_db.max() - 60.0, S_db.max())

# --- Contrast enhancement pipeline ---
# 1) Percentile-based stretch per band to reduce influence of outliers
lo_p = stretch_p_low
hi_p = stretch_p_high
# compute percentiles per band
band_lo = np.percentile(S_db, lo_p, axis=1, keepdims=True)
band_hi = np.percentile(S_db, hi_p, axis=1, keepdims=True)
# avoid zero range
range_p = np.maximum(1e-8, band_hi - band_lo)
# stretch
S_stretched = (S_db - band_lo) / range_p

# 2) Optional spectral transient (flux) boost: emphasize sudden increases in energy
if transient_boost and transient_boost > 0.0:
    # compute spectral flux on mel bands over frames (diff along time)
    # S_stretched shape: [bands, frames]
    flux = np.maximum(0.0, np.diff(S_stretched, axis=1))
    # pad to match frames
    flux = np.concatenate([np.zeros((flux.shape[0], 1)), flux], axis=1)
    # normalize flux per-band
    fmax = flux.max(axis=1, keepdims=True)
    fmax = np.maximum(fmax, 1e-8)
    flux_n = flux / fmax
    # boost: add scaled flux to stretched signal
    S_stretched = np.clip(S_stretched + transient_boost * flux_n, 0.0, 1.0)

# 3) Contrast curve (tanh-like) to increase mid-tone separation
if contrast_amount != 1.0:
    # map [0,1] to [-1,1], apply tanh scaling, map back
    a = contrast_amount
    x = (S_stretched * 2.0) - 1.0
    # scaled tanh preserves endpoints when a>0; use small epsilon to keep numeric stability
    S_contrast = (np.tanh(a * x) + 1.0) / 2.0
else:
    S_contrast = S_stretched

# fall back to simple min-max if the percentiles collapsed
band_min = S_db.min(axis=1, keepdims=True)
band_max = S_db.max(axis=1, keepdims=True)
if np.any(band_hi - band_lo < 1e-6):
    S_norm = (S_db - band_min) / np.maximum(1e-8, (band_max - band_min))
else:
    S_norm = np.clip(S_contrast, 0.0, 1.0)

# Transpose to frames×bands
frames_bands = S_norm.T  # shape: [num_frames, segments]
vals = np.round(frames_bands * 255.0).astype(np.uint8)

# Write row-major: frame0 all bands, frame1 all bands, ...
with open(outfile, "w") as f:
    for frame in vals:
        f.write(" ".join(str(int(v)) for v in frame) + "\n")

print(f"Wrote {vals.shape[0]} frames x {segments} bands to {outfile}")
