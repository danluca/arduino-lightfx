"""Validate upload format, repeatability and band separation after FXI4 smoothing."""

import itertools
import statistics
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from create_synthetic_seed import BARS, GROOVES, MAX_FILE_BYTES, encode, generate


class SyntheticSeedTests(unittest.TestCase):
    def test_loop_format_and_budget(self):
        for variant, (_, frames_per_beat, _, _) in enumerate(GROOVES, 1):
            with self.subTest(variant=variant):
                rows = generate(variant)
                data = encode(rows)
                self.assertEqual(len(rows), BARS * 4 * frames_per_beat)
                self.assertLessEqual(len(data), MAX_FILE_BYTES)
                self.assertEqual(rows, [list(map(int, line.split())) for line in data.splitlines()])
                self.assertEqual(data, encode(generate(variant)))
                # Last-bar decays carry across the seam; no blank reset frame.
                self.assertGreater(rows[0][1], 0)

    def test_visible_dynamics_after_firmware_filter(self):
        for variant in range(1, len(GROOVES) + 1):
            with self.subTest(variant=variant):
                rows = generate(variant)
                history = [0] * 4
                smoothed = []
                # Warm up one loop before measuring the cyclic steady state.
                for row in rows * 2:
                    history = [(old * 59 + raw * 196) // 255 for old, raw in zip(history, row)]
                    smoothed.append(history)
                smoothed = smoothed[-len(rows):]
                bands = list(zip(*smoothed))
                for band in bands:
                    self.assertGreater(max(band), 175)
                    self.assertLess(min(band), 15)
                for a, b in itertools.combinations(bands, 2):
                    self.assertLess(abs(statistics.correlation(a, b)), .6)
                self.assertGreater(statistics.mean(max(row) - min(row) for row in smoothed), 95)
                # Regular bass attacks survive smoothing in the opening bar.
                fpb = GROOVES[variant - 1][1]
                for beat in GROOVES[variant - 1][2]:
                    index = int(beat * fpb + .5)
                    self.assertGreater(smoothed[index][0] - smoothed[index - 1][0], 100)

    def test_genre_articulation(self):
        funk = generate(5)
        # Slap accent, softer ghost, and a rest before the next phrase.
        self.assertGreater(funk[5][1], 230)
        self.assertLess(funk[8][1], 110)
        self.assertLess(funk[19][1], 15)
        # Swung offbeat hat occurs one frame later than the straight eighth.
        self.assertGreater(funk[6][3], funk[5][3] + 150)
        trap = generate(6)
        # 808 remains high while the short kick has already fallen away.
        self.assertGreater(trap[4][0], 150)
        self.assertLess(trap[4][1], 15)
        self.assertGreater(trap[14][2], 230)  # snare on beat 3
        # Second bar's roll has separately articulated frame-level accents.
        roll = [trap[i][3] for i in range(49, 53)]
        self.assertGreater(roll[1], roll[0] + 40)
        self.assertLess(roll[2], roll[1] - 30)
        self.assertGreater(roll[3], roll[2] + 80)

    def test_slot_mapping(self):
        with tempfile.TemporaryDirectory() as directory:
            subprocess.run([sys.executable, str(Path(__file__).with_name("create_synthetic_seed.py")),
                            "--output-dir", directory, "--slots", "5", "6", "1", "3"],
                           check=True, capture_output=True)
            for slot, variant in enumerate((5, 6, 1, 3), 1):
                self.assertEqual((Path(directory) / f"fsi4_seed{slot}.txt").read_bytes(), encode(generate(variant)))
            self.assertFalse((Path(directory) / "fsi4_seed5.txt").exists())
            self.assertIn("Funk / slap bass", (Path(directory) / "preview.html").read_text())

    def test_invalid_output_rejected(self):
        for rows in ([], [[1, 2, 3]], [[0, 0, 0, 256]], [[-1, 0, 0, 0]], [[255] * 4] * 2000):
            with self.subTest(rows=rows[:1]):
                with self.assertRaises(ValueError):
                    encode(rows)


if __name__ == "__main__":
    unittest.main()
