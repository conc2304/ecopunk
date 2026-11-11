## Stability (steady state; the first 4 s of each run are dropped)

jitter = mean CIE76 dE between successive samples; switch = share of successive samples whose winning 8x8x8 bin changed.

| scene | n | avg jitter | dom160 jitter / switch | dom160 EMA jitter | dom160 hyst jitter / switch | dom16 jitter / switch | vib160 valid / jitter / switch | vib16 valid / jitter |
|---|---|---|---|---|---|---|---|---|
| blob | 150 | 3.56 | 3.87 / 14% | 1.60 | 2.68 / 8% | 3.91 / 12% | 89% / 16.66 / 40% | 100% / 22.74 |
| temporal | 151 | 2.62 | 2.72 / 11% | 1.67 | 2.54 / 10% | 4.46 / 22% | 99% / 3.24 / 22% | 99% / 4.73 |
| blueprint | 160 | 0.63 | 0.11 / 0% | 0.07 | 0.11 / 0% | 0.13 / 0% | 100% / 0.91 / 3% | 100% / 2.06 |
| fragment | 160 | 2.71 | 0.22 / 0% | 0.09 | 0.22 / 0% | 4.22 / 31% | 100% / 5.01 / 26% | 100% / 6.49 |
| quadrant | 159 | 2.66 | 5.05 / 17% | 3.06 | 4.43 / 12% | 10.90 / 31% | 100% / 4.45 / 26% | 100% / 3.97 |

## Disagreement between measures (mean CIE76 dE)

| scene | avg vs dom160 | avg vs vib160 | dom160 vs vib160 | dom16 vs dom160 | vib16 vs vib160 | avg16 vs avg160 | mixed 16x9 cells |
|---|---|---|---|---|---|---|---|
| blob | 17.37 | 39.50 | 46.25 | 3.15 | 18.58 | 0.008 | 29% |
| temporal | 10.82 | 7.62 | 9.30 | 2.92 | 3.32 | 0.008 | 5% |
| blueprint | 41.64 | 21.16 | 61.19 | 0.51 | 2.07 | 0.007 | 25% |
| fragment | 17.58 | 8.82 | 17.49 | 7.74 | 6.46 | 0.006 | 17% |
| quadrant | 34.04 | 12.22 | 32.53 | 18.97 | 3.39 | 0.007 | 22% |

## Incremental cost (this machine; p50 / p95 / max)

| scene | submit ms | 16x9 read ms | 160x90 read ms (extra) | analysis 16x9 us | analysis 160x90 us |
|---|---|---|---|---|---|
| blob | 0.092 / 0.238 / 0.544 | 3.171 / 6.710 / 19.357 | 0.315 / 2.670 / 7.989 | 12.3 / 34.4 / 135.9 | 507.7 / 861.6 / 1197.8 |
| temporal | 0.068 / 0.100 / 0.225 | 1.195 / 4.861 / 11.492 | 0.203 / 0.301 / 0.377 | 7.5 / 10.1 / 18.5 | 365.1 / 510.3 / 554.8 |
| blueprint | 0.071 / 0.127 / 0.188 | 1.225 / 3.895 / 13.527 | 0.207 / 0.344 / 6.198 | 7.3 / 10.6 / 26.9 | 298.9 / 458.3 / 654.7 |
| fragment | 0.070 / 0.128 / 0.226 | 1.216 / 4.126 / 8.821 | 0.205 / 0.366 / 1.329 | 7.1 / 11.1 / 38.3 | 305.1 / 465.8 / 601.8 |
| quadrant | 0.069 / 0.115 / 0.220 | 1.200 / 3.654 / 7.614 | 0.199 / 0.321 / 5.170 | 7.7 / 12.5 / 33.4 | 303.2 / 475.7 / 649.1 |
