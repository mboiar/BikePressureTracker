"""
Parse program logs to find dominant frequencies and clipped values counts from each axis of ADXL345 accelerometer over a 5s window.

Usage: parser.py <log_file>

Example output:
frame 10  fs=800  clip=(0, 0, 0)
   X [(0.72, 0.421), (6.9, 0.511), (3.56, 0.399), (6.0, 0.185), (10.51, 0.17), (13.97, 0.139), (1.41, 0.13), (9.68, 0.124)]
   Y [(3.47, 2.038), (2.68, 0.675), (0.72, 0.292), (10.52, 0.288), (1.87, 0.212), (7.12, 0.192), (4.6, 0.18), (13.64, 0.145)]
   Z [(0.73, 0.434), (7.02, 0.54), (6.19, 0.213), (2.68, 0.2), (4.06, 0.18), (10.42, 0.161), (5.25, 0.12), (13.03, 0.113)]

"""

import re, struct, sys
import numpy as np
from itertools import zip_longest
import pandas as pd

MAGIC   = b"FFT1"
HDR     = struct.Struct("<4sIH3H")          # magic, frame, fs_hz, clip[3] = 16 bytes
NPEAKS  = 8
PKT_LEN = HDR.size + 3 * NPEAKS * 8         # 208 bytes

def load_bytes(path):
    hex_str = ""
    with open(path) as f:
        for line in f:
            line = re.sub(r"^\d\d:\d\d:\d\d\.\d{3}\s*", "", line)
            hex_str += re.sub(r"\s+", "", line)
    return bytes.fromhex(hex_str)

def parse(data):
    out = []
    for m in re.finditer(re.escape(MAGIC), data):
        a = m.start()
        if a + PKT_LEN > len(data):
            break                                           # truncated last packet
        _, frame, fs, cx, cy, cz = HDR.unpack_from(data, a)
        pk = np.frombuffer(data, "<f4", 3 * NPEAKS * 2, a + HDR.size).reshape(3, NPEAKS, 2)
        if not (0 < fs <= 3200 and np.isfinite(pk).all()
                and (pk[..., 0] >= 0).all() and (pk[..., 0] <= fs / 2 + 1).all()):
            continue                                        # false magic or corrupted packet
        out.append(dict(frame=frame, fs=fs, clip=(cx, cy, cz), peaks=pk))
    return out

if __name__ == "__main__":
    rows = []
    prev = None

    for f in parse(load_bytes('serial_20261107_152354.txt')):
        gap = "" if prev is None or f["frame"] == prev + 1 else f"  (gap: {f['frame'] - prev - 1} missing)"
        prev = f["frame"]
        print(f"frame {f['frame']}  fs={f['fs']}  clip={f['clip']}{gap}")

        # Pobranie list pików (częstotliwość, amplituda) dla każdej z osi
        peaks_x = [(round(float(fr), 2), round(float(g), 3)) for fr, g in f["peaks"][0] if g > 0]
        peaks_y = [(round(float(fr), 2), round(float(g), 3)) for fr, g in f["peaks"][1] if g > 0]
        peaks_z = [(round(float(fr), 2), round(float(g), 3)) for fr, g in f["peaks"][2] if g > 0]

        # Jeśli w danej ramce nie wykryto żadnych pików na żadnej osi
        if not peaks_x and not peaks_y and not peaks_z:
            rows.append({
                'frame': f['frame'],
                'fs': f['fs'],
                'clip_x': f['clip'][0],
                'clip_y': f['clip'][1],
                'clip_z': f['clip'][2],
                'freq_x': None, 'freq_y': None, 'freq_z': None,
                'amp_x': None,  'amp_y': None,  'amp_z': None
            })
        else:
            for px, py, pz in zip_longest(peaks_x, peaks_y, peaks_z, fillvalue=(None, None)):
                rows.append({
                    'frame': f['frame'],
                    'fs': f['fs'],
                    'clip_x': f['clip'][0],
                    'clip_y': f['clip'][1],
                    'clip_z': f['clip'][2],
                    'freq_x': px[0],
                    'freq_y': py[0],
                    'freq_z': pz[0],
                    'amp_x': px[1],
                    'amp_y': py[1],
                    'amp_z': pz[1]
                })

    # Utworzenie DataFrame ze zdefiniowaną kolejnością kolumn
    columns_order = ['frame', 'fs', 'clip_x', 'clip_y', 'clip_z', 'freq_x', 'freq_y', 'freq_z', 'amp_x', 'amp_y', 'amp_z']
    df = pd.DataFrame(rows, columns=columns_order)
    df=df.dropna(subset=['freq_x','freq_y','freq_z'],how='all')
    df.to_csv('zestaw_nr_1.csv',index=None)

