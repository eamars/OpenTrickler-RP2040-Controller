"""Measure a rotary tube from its binary STL: axis, bore, wall, internal features.

Pure Python (the project's venv has no numpy). Usage: python -I stl_profile.py <file.stl> [bin_mm]
"""
import math
import struct
import sys
from collections import defaultdict


def load(path):
    with open(path, "rb") as f:
        data = f.read()
    count = struct.unpack_from("<I", data, 80)[0]
    tris = []
    off = 84
    for _ in range(count):
        v = struct.unpack_from("<12fH", data, off)
        off += 50
        tris.append((v[0:3], v[3:6], v[6:9], v[9:12]))
    return tris


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def norm(a):
    return math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])


def main():
    path = sys.argv[1]
    bin_mm = float(sys.argv[2]) if len(sys.argv) > 2 else 2.0
    tris = load(path)
    lo = [1e9] * 3
    hi = [-1e9] * 3
    for _, a, b, c in tris:
        for p in (a, b, c):
            for i in range(3):
                lo[i] = min(lo[i], p[i])
                hi[i] = max(hi[i], p[i])
    ext = [hi[i] - lo[i] for i in range(3)]
    print(f"triangles {len(tris)}")
    print("bbox lo", [round(x, 3) for x in lo], "hi", [round(x, 3) for x in hi], "extent", [round(x, 3) for x in ext])

    # signed volume and area
    vol = 0.0
    area = 0.0
    for _, a, b, c in tris:
        vol += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6.0
        area += norm(cross(sub(b, a), sub(c, a))) / 2.0
    print(f"solid volume {vol:.1f} mm3, surface area {area:.1f} mm2")

    ax = ext.index(max(ext))
    u, v = [i for i in range(3) if i != ax]
    # axis centre: fit from the outer surface points (area weighted centroid of the cross coordinates is biased by
    # features); use the bbox centre of the cross dims, then report the centroid for comparison
    cu = (lo[u] + hi[u]) / 2.0
    cv = (lo[v] + hi[v]) / 2.0
    print(f"axis dim {ax} (0=x,1=y,2=z); centre in cross plane ({cu:.3f}, {cv:.3f})")

    bins = defaultdict(lambda: {"in": [], "out": [], "flat": []})
    for n, a, b, c in tris:
        nn = cross(sub(b, a), sub(c, a))
        ln = norm(nn)
        if ln == 0:
            continue
        nn = (nn[0] / ln, nn[1] / ln, nn[2] / ln)
        pts = [a, b, c,
               tuple((a[i] + b[i] + c[i]) / 3.0 for i in range(3)),
               tuple((a[i] + b[i]) / 2.0 for i in range(3)),
               tuple((b[i] + c[i]) / 2.0 for i in range(3)),
               tuple((a[i] + c[i]) / 2.0 for i in range(3))]
        for p in pts:
            du = p[u] - cu
            dv = p[v] - cv
            r = math.hypot(du, dv)
            if r < 1e-6:
                continue
            radial = (nn[u] * du + nn[v] * dv) / r
            z = p[ax] - lo[ax]
            key = int(z // bin_mm)
            th = math.degrees(math.atan2(dv, du))
            if radial < -0.5:
                bins[key]["in"].append((r, th))
            elif radial > 0.5:
                bins[key]["out"].append((r, th))
            else:
                bins[key]["flat"].append((r, th, nn[ax]))

    def pct(vals, q):
        vals = sorted(vals)
        if not vals:
            return float("nan")
        return vals[min(len(vals) - 1, int(q * len(vals)))]

    print("z_from_mm  inward-facing r: min p10 p50 p90 max | outward-facing r: min p50 max | n_in n_out n_flat")
    for key in sorted(bins):
        b = bins[key]
        ri = [x[0] for x in b["in"]]
        ro = [x[0] for x in b["out"]]
        print(f"{key * bin_mm:7.1f}   {pct(ri, 0):6.2f} {pct(ri, .1):6.2f} {pct(ri, .5):6.2f} {pct(ri, .9):6.2f} {pct(ri, .999):6.2f} | "
              f"{pct(ro, 0):6.2f} {pct(ro, .5):6.2f} {pct(ro, .999):6.2f} | {len(ri):5d} {len(ro):5d} {len(b['flat']):5d}")


if __name__ == "__main__":
    main()
