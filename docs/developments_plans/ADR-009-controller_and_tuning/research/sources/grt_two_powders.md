# Gordon's Reloading Tool: its pages for ADI AR 2209 and ADI BM 2

From two screenshots of the program's propellant window, sent by the owner on 2026-11-21, of a copy of GRT 2021.2030 (nightly). The program was not run here and its database, which is encrypted, was not opened.

| Property | ADI AR 2209 | ADI BM 2 |
|---|---|---|
| Data state | 2021-03-16 | 2020-04-30 |
| Specific explosive heat (Qex) | 3900 kJ/kg | 3670 kJ/kg |
| Isentropic exponent (k) | 1.2322 | 1.2267 |
| Combustion coefficient (Ba) | 0.4834 | 0.7182 |
| Progressive/degressive coefficient (a0) | 0.5013 | 0.5755 |
| Transition limit z1 | 0.5295 | 0.3942 |
| Transition limit z2 | 0.8036 | 0.8071 |
| Covolume (eta) | 1.0000 cm3/g | 1.0000 cm3/g |
| Material density (Pc) | 1560 kg/m3 | 1540 kg/m3 |
| Bulk density (Pcd) | 990 kg/m3 | 980 kg/m3 |

**What a trickler can use of it:** the two densities. Nothing else on the page describes the powder as a thing that pours.

**What is not in it:** a kernel's size or mass. The program's own documentation lists the fields of a propellant (`doku/en/doku/file_propellant.txt` and `inputfields-propellant.txt` in the copy): the maker, the name, the lot, the burn coefficients, the two limits, the temperature, the heat, the material density and the bulk density. The window has two more tabs, "Picture" and "Detail Picture", not seen here.

**Worked from it, with this research's own measurements** (`../../progress.md`):
- A kernel's volume, from its mass over the material density: AR 2209, 0.021 gr (1.36 mg), 0.87 mm3; BM 2, 0.0117 gr (0.76 mg), 0.49 mm3.
- The powder the low-flow fine tube pours a turn, as a volume at the bulk density: AR 2209, 0.757 gr, 0.050 cc; BM 2, 0.548 gr, 0.036 cc. The bulk densities are 1% apart and the pour is 28% apart, so the bulk density does not give the pour.
