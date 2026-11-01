"""
Usage: parser.py <log_file>

"""

import re, struct, sys
import numpy as np

MAGICS   = (b"FFT1")
HDR      = struct.Struct("<4sIH3H")    # magic, frame, fs_hz, clip[3]  = 16 bytes
PEAKS_AT = 20                          
NPEAKS   = 8
PKT_LEN  = PEAKS_AT + 3 * NPEAKS * 8

def load_bytes(path):
    hex_str = ""
    with open(path) as f:
        for line in f:
            line = re.sub(r"^\d\d:\d\d:\d\d\.\d{3}\s*", "", line)
            hex_str += re.sub(r"\s+", "", line)
    return bytes.fromhex(hex_str)

def parse(data):
    out = []
    for m in re.finditer(b"|".join(re.escape(x) for x in MAGICS), data):
        a = m.start()
        if a + PKT_LEN > len(data):
            break
        _, frame, fs, *clip = HDR.unpack_from(data, a)
        pk = np.frombuffer(data, "<f4", 3 * NPEAKS * 2, a + PEAKS_AT).reshape(3, NPEAKS, 2)
        f, amp = pk[..., 0], pk[..., 1]
        if not (0 < fs <= 3200 and np.isfinite(pk).all()
                and (f >= 0).all() and (f <= fs / 2 + 1).all() and (amp >= 0).all()):
            continue                                   # false magic or corrupted packet
        out.append(dict(frame=frame, fs=fs, clip=clip, peaks=pk))
    return out

if __name__ == "__main__":
    frames = parse(load_bytes(sys.argv[1]))
    print(len(frames), "packets")
    for f in frames:
        print(f"frame {f['frame']}  fs={f['fs']}  clip={f['clip']}")
        for ax, name in enumerate("XYZ"):
            pk = [(round(float(fr), 1), round(float(g), 2)) for fr, g in f["peaks"][ax] if g > 0]
            print("  ", name, pk)
