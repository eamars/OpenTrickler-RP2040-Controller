"""Rifling (starts, lead), belt teeth and wall openings of a rotary tube STL. Pure Python.

Usage: python -I stl_features.py <file.stl>
"""
import cmath
import math
import struct
import sys
from collections import defaultdict


def load(path):
    with open(path, "rb") as f:
        data = f.read()
    count = struct.unpack_from("<I", data, 80)[0]
    return [struct.unpack_from("<12f", data, 84 + 50 * i) for i in range(count)]


def samples(tris, per_edge=4):
    """Points spread over each triangle with its unit normal: (x, y, z, nx, ny, nz, area_share)."""
    for t in tris:
        a, b, c = t[3:6], t[6:9], t[9:12]
        e1 = [b[i] - a[i] for i in range(3)]
        e2 = [c[i] - a[i] for i in range(3)]
        n = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
        ln = math.sqrt(n[0] ** 2 + n[1] ** 2 + n[2] ** 2)
        if ln == 0:
            continue
        n = (n[0] / ln, n[1] / ln, n[2] / ln)
        area = ln / 2.0
        # denser sampling for larger triangles
        k = max(1, min(40, int(math.sqrt(area) * per_edge)))
        w = area / (k * (k + 1) / 2)
        for i in range(k):
            for j in range(k - i):
                s = (i + 1 / 3) / k
                u = (j + 1 / 3) / k
                yield (a[0] + s * e1[0] + u * e2[0], a[1] + s * e1[1] + u * e2[1], a[2] + s * e1[2] + u * e2[2],
                       n[0], n[1], n[2], w)


def main():
    tris = load(sys.argv[1])
    inward = defaultdict(list)    # z bin (0.5 mm) -> (r, theta, w)
    outward = defaultdict(list)
    flat = defaultdict(list)      # walls of openings, steps: (r, theta, nz, w)
    for x, y, z, nx, ny, nz, w in samples(tris):
        r = math.hypot(x, y)
        if r < 1e-6:
            continue
        radial = (nx * x + ny * y) / r
        th = math.atan2(y, x)
        key = int(z // 0.5)
        if radial < -0.6:
            inward[key].append((r, th, w))
        elif radial > 0.6:
            outward[key].append((r, th, w))
        elif abs(nz) < 0.5:
            flat[key].append((r, th, nz, w))

    def harmonics(points, rmin, rmax, top=3, mmax=60):
        sel = [(th, w) for r, th, w in points if rmin <= r <= rmax]
        tot = sum(w for _, w in sel)
        if tot == 0:
            return []
        out = []
        for m in range(1, mmax + 1):
            c = sum(w * cmath.exp(1j * m * th) for th, w in sel) / tot
            out.append((abs(c), m, cmath.phase(c)))
        out.sort(reverse=True)
        return out[:top]

    allr_in = sorted(r for pts in inward.values() for r, _, _ in pts)
    allr_out = sorted(r for pts in outward.values() for r, _, _ in pts)
    print("inward r range", round(allr_in[0], 3), round(allr_in[-1], 3), " outward r range", round(allr_out[0], 3), round(allr_out[-1], 3))

    # 1. rifling: groove-bottom points of the bore (outer 15% of the bore's radial band), harmonic vs z
    keys = sorted(inward)
    mid = [k for k in keys if len(inward[k]) > 200]
    body = mid[len(mid) // 4: 3 * len(mid) // 4]
    rs = sorted(r for k in body for r, _, _ in inward[k])
    crest, groove = rs[int(0.01 * len(rs))], rs[int(0.99 * len(rs))]
    print(f"bore: crest r {crest:.3f}  groove r {groove:.3f}  depth {groove - crest:.3f}")
    band_lo = groove - 0.15 * (groove - crest)
    votes = defaultdict(float)
    for k in body:
        for mag, m, _ in harmonics(inward[k], band_lo, groove + 0.01, top=1, mmax=24):
            votes[m] += mag
    starts = max(votes, key=votes.get)
    print("groove harmonic votes", sorted(((round(v, 2), m) for m, v in votes.items()), reverse=True)[:4])
    phases = []
    for k in mid:
        sel = [(th, w) for r, th, w in inward[k] if band_lo <= r <= groove + 0.01]
        tot = sum(w for _, w in sel)
        if tot == 0:
            continue
        c = sum(w * cmath.exp(1j * starts * th) for th, w in sel) / tot
        if abs(c) > 0.2:
            phases.append((k * 0.5 + 0.25, cmath.phase(c), abs(c)))
    # unwrap
    un = []
    prev = None
    off = 0.0
    for z, ph, mag in phases:
        if prev is not None:
            d = ph - prev
            if d > math.pi:
                off -= 2 * math.pi
            elif d < -math.pi:
                off += 2 * math.pi
        prev = ph
        un.append((z, ph + off))
    if len(un) > 4:
        n = len(un)
        mz = sum(z for z, _ in un) / n
        mp = sum(p for _, p in un) / n
        slope = sum((z - mz) * (p - mp) for z, p in un) / sum((z - mz) ** 2 for z, _ in un)
        resid = math.sqrt(sum((p - mp - slope * (z - mz)) ** 2 for z, p in un) / n)
        if abs(slope) > 1e-6:
            lead = 2 * math.pi * starts / slope
            print(f"rifling: {starts} start(s), lead {lead:.2f} mm/turn (sign = hand), groove spacing {abs(lead) / starts:.2f} mm, "
                  f"z {un[0][0]:.1f}..{un[-1][0]:.1f}, phase fit residual {resid:.3f} rad")
            helix_angle = math.degrees(math.atan(abs(lead) / (2 * math.pi * groove)))
            print(f"         lead angle at groove radius {helix_angle:.1f} deg")
        else:
            print(f"grooves: {starts}-fold, straight (no twist)")

    # 2. per-mm table: angular coverage of the bore wall and of the outer skin, teeth harmonic, flat walls
    print("z_mm  bore_cov  skin_cov  r_in(min..max)  r_out(min..max)  top outward harmonic (m,mag)  flat r(min..max) n")
    zmax = max(keys)
    for zmm in range(0, zmax // 2 + 1):
        pin = [p for k in (2 * zmm, 2 * zmm + 1) for p in inward.get(k, [])]
        pout = [p for k in (2 * zmm, 2 * zmm + 1) for p in outward.get(k, [])]
        pfl = [p for k in (2 * zmm, 2 * zmm + 1) for p in flat.get(k, [])]

        def cov(points):
            return len({int(math.degrees(th) // 5) for _, th, _ in points}) / 72.0

        hin = ""
        if pout:
            ro = sorted(r for r, _, _ in pout)
            h = harmonics(pout, ro[0], ro[-1], top=1)
            hin = f"{h[0][1]:3d} {h[0][0]:.2f}" if h else ""
        rin = sorted(r for r, _, _ in pin) or [float('nan')]
        rout = sorted(r for r, _, _ in pout) or [float('nan')]
        rfl = sorted(p[0] for p in pfl) or [float('nan')]
        print(f"{zmm:4d}  {cov(pin):7.2f}  {cov(pout):7.2f}  {rin[0]:6.2f}..{rin[-1]:6.2f}  {rout[0]:6.2f}..{rout[-1]:6.2f}   {hin:10s}   "
              f"{rfl[0]:6.2f}..{rfl[-1]:6.2f} {len(pfl):6d}")


if __name__ == "__main__":
    main()
