## Coverage, invalid runs and winner switching (steady state, first 4 s dropped)

| scene | n | valid | invalid runs (count / mean s / max s) | raw winner switches | hysteresis winner switches |
|---|---|---|---|---|---|
| blob | 213 | 82% | 22 / 0.9 / 2.0 | 39% | 34% |
| temporal | 200 | 100% | 0 / 0.0 / 0.0 | 19% | 10% |
| blueprint | 206 | 49% | 6 / 8.8 / 36.5 | 54% | 39% |
| fragment | 196 | 100% | 0 / 0.0 / 0.0 | 1% | 1% |
| quadrant | 186 | 96% | 3 / 1.2 / 2.0 | 34% | 23% |

## Successive-sample colour change (CIE76 dE: mean / p95 / max)

| scene | raw | EMA | hysteresis | EMA + hysteresis |
|---|---|---|---|---|
| blob | 14.26 / 68.03 / 89.8 | 6.75 / 26.71 / 41.6 | 12.89 / 70.68 / 89.5 | 5.92 / 26.66 / 41.0 |
| temporal | 2.55 / 12.85 / 46.3 | 1.42 / 6.01 / 14.6 | 1.83 / 12.70 / 46.3 | 1.23 / 5.80 / 14.4 |
| blueprint | 9.07 / 32.94 / 48.9 | 4.94 / 13.29 / 26.0 | 7.18 / 21.09 / 48.9 | 4.57 / 13.04 / 26.0 |
| fragment | 0.98 / 1.82 / 55.5 | 0.54 / 0.81 / 35.2 | 0.98 / 1.82 / 55.6 | 0.64 / 1.46 / 35.2 |
| quadrant | 10.01 / 43.54 / 84.6 | 5.16 / 14.91 / 56.1 | 7.81 / 38.50 / 84.6 | 4.62 / 14.34 / 56.1 |

## Responsiveness to persistent colour steps (a step = raw vibrant moves >= 20 dE and stays; latency = time until the variant is within 10 dE of the new level)

| scene | steps | raw reached / median s | EMA reached / median s | hysteresis reached / median s | EMA + hysteresis reached / median s |
|---|---|---|---|---|---|
| blob | 9 | 100% / 0.0 | 100% / 1.5 | 89% / 0.0 | 78% / 1.0 |
| temporal | 3 | 100% / 0.0 | 100% / 1.5 | 100% / 0.0 | 67% / 1.5 |
| blueprint | 4 | 100% / 0.0 | 100% / 1.0 | 100% / 0.0 | 100% / 1.0 |
| fragment | 1 | 100% / 0.0 | 100% / 1.5 | 100% / 0.5 | 100% / 3.0 |
| quadrant | 12 | 100% / 0.0 | 100% / 1.0 | 100% / 0.0 | 92% / 1.0 |

## Hold-last-valid policy (offline; what the ribbon would show while vibrant is invalid)

| scene | time showing a held colour | hold runs (count / mean s / max s) | mean dE held -> next valid | holds that were wrong (dE>20) | holds >= 3 s that were wrong |
|---|---|---|---|---|---|
| blob | 18% | 21 / 0.9 / 2.0 | 19.9 | 9 of 21 | 0 of 0 |
| temporal | 0% | 0 / 0.0 / 0.0 | n/a | 0 of 0 | 0 of 0 |
| blueprint | 50% | 5 / 10.3 / 36.5 | 30.6 | 3 of 4 | 1 of 2 |
| fragment | 0% | 0 / 0.0 / 0.0 | n/a | 0 of 0 | 0 of 0 |
| quadrant | 4% | 3 / 1.2 / 2.0 | 29.3 | 1 of 3 | 0 of 0 |

## Incremental cost (desktop; p50 / p95 / max)

| scene | submit ms (not needed in production) | 16x9 read ms (already paid by the series) | 160x90 read ms (EXTRA) | 160x90 histogram us | vibrant raw+EMA+hysteresis us |
|---|---|---|---|---|---|
| blob | 0.075 / 0.172 / 0.311 | 1.297 / 5.492 / 10.907 | 0.207 / 0.460 / 6.912 | 397.7 / 626.5 / 1555.1 | 1.1 / 2.0 / 2.8 |
| temporal | 0.079 / 0.186 / 0.250 | 3.103 / 6.335 / 11.082 | 0.242 / 4.850 / 5.508 | 393.9 / 638.6 / 794.6 | 1.3 / 2.4 / 4.6 |
| blueprint | 0.088 / 0.162 / 0.384 | 1.264 / 4.958 / 8.492 | 0.258 / 0.786 / 5.302 | 431.4 / 667.8 / 903.4 | 1.3 / 2.3 / 3.2 |
| fragment | 0.094 / 0.162 / 0.279 | 1.231 / 3.499 / 4.858 | 0.247 / 0.342 / 3.019 | 501.3 / 747.7 / 934.2 | 1.3 / 2.2 / 2.6 |
| quadrant | 0.137 / 0.240 / 0.474 | 1.269 / 3.750 / 9.868 | 0.314 / 1.142 / 2.269 | 684.1 / 985.5 / 1675.6 | 2.4 / 4.0 / 18.3 |
