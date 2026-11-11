# OC-003 sampling-frame attribution — campaign summary

media `media.auto.1ab5450d`, scene `blob-region-prototype`, 1280x720, 1 round(s) x 100 s, binary sha256 `89ec0f6e022ddfa741c79a6409602c54ba486219738efefd01b5d955cf11702d`
runs found 6/6; all event streams reconciled: **True**; GL-error lines: **0**

**SINGLE ROUND: there is no round-to-round spread, so no difference in this summary is a result (smoke/rehearsal data only)**

## Frame time (ms), pooled over rounds; steady state only (warm-up, media/scene change, transitions excluded)

| cond | frames | p50 | p95 | p99 | worst | long > 40 ms | mean draw CPU |
|---|---|---|---|---|---|---|---|
| P0 | 2034 | 33.35 | 34.29 | 34.65 | 37.98 | 0 | 3.01 |
| P1 | 2030 | 33.33 | 34.42 | 34.86 | 86.64 | 5 | 3.05 |
| P2 | 2055 | 33.33 | 34.64 | 35.28 | 36.47 | 0 | 3.12 |
| P3 | 2055 | 33.34 | 34.45 | 34.92 | 35.51 | 0 | 3.00 |
| P4 | 2054 | 33.33 | 35.17 | 35.98 | 38.21 | 0 | 3.01 |
| P5 | 2056 | 33.26 | 35.09 | 36.20 | 37.37 | 0 | 3.23 |

## Per round (p95 / p99 / worst ms)

| cond | r1 |
|---|---|
| P0 | 34.29 / 34.65 / 37.98 |
| P1 | 34.42 / 34.86 / 86.64 |
| P2 | 34.64 / 35.28 / 36.47 |
| P3 | 34.45 / 34.92 / 35.51 |
| P4 | 35.17 / 35.98 / 38.21 |
| P5 | 35.09 / 36.20 / 37.37 |

## Differences (pooled; per-round min..max; sign-consistent = every round agrees on the direction; n/a with fewer than 2 rounds)

| diff | metric | pooled | per-round min..max | sign-consistent |
|---|---|---|---|---|
| P2-P0 | p50 | -0.0233 | -0.0233..-0.0233 | n/a |
| P2-P0 | p95 | 0.3503 | 0.3503..0.3503 | n/a |
| P2-P0 | p99 | 0.6291 | 0.6291..0.6291 | n/a |
| P2-P0 | worst | -1.5162 | -1.5162..-1.5162 | n/a |
| P2-P0 | long_frames | 0 | 0..0 | n/a |
| P3-P0 | p50 | -0.0193 | -0.0193..-0.0193 | n/a |
| P3-P0 | p95 | 0.1627 | 0.1627..0.1627 | n/a |
| P3-P0 | p99 | 0.2645 | 0.2645..0.2645 | n/a |
| P3-P0 | worst | -2.4738 | -2.4738..-2.4738 | n/a |
| P3-P0 | long_frames | 0 | 0..0 | n/a |
| P4-P0 | p50 | -0.0292 | -0.0292..-0.0292 | n/a |
| P4-P0 | p95 | 0.8806 | 0.8806..0.8806 | n/a |
| P4-P0 | p99 | 1.3274 | 1.3274..1.3274 | n/a |
| P4-P0 | worst | 0.222 | 0.222..0.222 | n/a |
| P4-P0 | long_frames | 0 | 0..0 | n/a |
| P5-P0 | p50 | -0.0951 | -0.0951..-0.0951 | n/a |
| P5-P0 | p95 | 0.8004 | 0.8004..0.8004 | n/a |
| P5-P0 | p99 | 1.5448 | 1.5448..1.5448 | n/a |
| P5-P0 | worst | -0.6092 | -0.6092..-0.6092 | n/a |
| P5-P0 | long_frames | 0 | 0..0 | n/a |
| P4-P3 | p50 | -0.0099 | -0.0099..-0.0099 | n/a |
| P4-P3 | p95 | 0.7179 | 0.7179..0.7179 | n/a |
| P4-P3 | p99 | 1.0629 | 1.0629..1.0629 | n/a |
| P4-P3 | worst | 2.6958 | 2.6958..2.6958 | n/a |
| P4-P3 | long_frames | 0 | 0..0 | n/a |
| P5-P4 | p50 | -0.0659 | -0.0659..-0.0659 | n/a |
| P5-P4 | p95 | -0.0802 | -0.0802..-0.0802 | n/a |
| P5-P4 | p99 | 0.2174 | 0.2174..0.2174 | n/a |
| P5-P4 | worst | -0.8312 | -0.8312..-0.8312 | n/a |
| P5-P4 | long_frames | 0 | 0..0 | n/a |
| P5-P1 | p50 | -0.0705 | -0.0705..-0.0705 | n/a |
| P5-P1 | p95 | 0.6728 | 0.6728..0.6728 | n/a |
| P5-P1 | p99 | 1.3426 | 1.3426..1.3426 | n/a |
| P5-P1 | worst | -49.2602 | -49.2602..-49.2602 | n/a |
| P5-P1 | long_frames | -5 | -5..-5 | n/a |
| P4-P2 | p50 | -0.0059 | -0.0059..-0.0059 | n/a |
| P4-P2 | p95 | 0.5303 | 0.5303..0.5303 | n/a |
| P4-P2 | p99 | 0.6983 | 0.6983..0.6983 | n/a |
| P4-P2 | worst | 1.7382 | 1.7382..1.7382 | n/a |
| P4-P2 | long_frames | 0 | 0..0 | n/a |

A difference that is not sign-consistent across rounds, or is smaller than the round-to-round range of the condition, is not a result.

## Event-conditioned frames (pooled, steady state). Independent populations OVERLAP; exclusive categories sum to the frame total

### P0

events: fx_issue=0, fx_collect=0, raw_issue=0, raw_collect=0, fx_ifail=0, fx_cfail=0, raw_ifail=0, raw_cfail=0

| population | frames | p50 | p95 | p99 | worst | long |
|---|---|---|---|---|---|---|
| none | 2034 | 33.35 | 34.29 | 34.65 | 37.98 | 0 |

### P1

events: fx_issue=131, fx_collect=131, raw_issue=0, raw_collect=0, fx_ifail=0, fx_cfail=0, raw_ifail=0, raw_cfail=0

| population | frames | p50 | p95 | p99 | worst | long |
|---|---|---|---|---|---|---|
| with fxIssue | 131 | 33.62 | 34.69 | 34.99 | 35.11 | 0 |
| with fxCollect | 131 | 33.66 | 34.67 | 35.22 | 35.30 | 0 |
| none | 1768 | 33.30 | 34.28 | 34.83 | 86.64 | 5 |
| fxIssueOnly | 131 | 33.62 | 34.69 | 34.99 | 35.11 | 0 |
| fxCollectOnly | 131 | 33.66 | 34.67 | 35.22 | 35.30 | 0 |

### P2

events: fx_issue=132, fx_collect=132, raw_issue=132, raw_collect=132, fx_ifail=0, fx_cfail=0, raw_ifail=0, raw_cfail=0

| population | frames | p50 | p95 | p99 | worst | long |
|---|---|---|---|---|---|---|
| with fxIssue | 132 | 33.49 | 34.66 | 35.23 | 35.43 | 0 |
| with fxCollect | 132 | 34.36 | 35.39 | 35.73 | 36.10 | 0 |
| with rawIssue | 132 | 33.55 | 34.63 | 35.45 | 35.46 | 0 |
| with rawCollect | 132 | 34.21 | 35.39 | 35.71 | 36.10 | 0 |
| none | 1541 | 33.22 | 34.22 | 34.86 | 36.47 | 0 |
| multi | 14 | 34.63 | 36.10 | 36.10 | 36.10 | 0 |
| fxIssueOnly | 130 | 33.49 | 34.64 | 35.23 | 35.43 | 0 |
| fxCollectOnly | 120 | 34.31 | 35.26 | 35.63 | 35.73 | 0 |
| rawIssueOnly | 120 | 33.53 | 34.40 | 34.52 | 34.92 | 0 |
| rawCollectOnly | 130 | 34.19 | 35.24 | 35.59 | 35.71 | 0 |

### P3

events: fx_issue=0, fx_collect=0, raw_issue=132, raw_collect=132, fx_ifail=0, fx_cfail=0, raw_ifail=0, raw_cfail=0

| population | frames | p50 | p95 | p99 | worst | long |
|---|---|---|---|---|---|---|
| with rawIssue | 132 | 33.59 | 34.61 | 35.22 | 35.51 | 0 |
| with rawCollect | 132 | 34.08 | 35.04 | 35.46 | 35.47 | 0 |
| none | 1791 | 33.28 | 34.25 | 34.79 | 35.45 | 0 |
| rawIssueOnly | 132 | 33.59 | 34.61 | 35.22 | 35.51 | 0 |
| rawCollectOnly | 132 | 34.08 | 35.04 | 35.46 | 35.47 | 0 |

### P4

events: fx_issue=0, fx_collect=0, raw_issue=0, raw_collect=0, fx_ifail=0, fx_cfail=0, raw_ifail=0, raw_cfail=0

| population | frames | p50 | p95 | p99 | worst | long |
|---|---|---|---|---|---|---|
| none | 2054 | 33.33 | 35.17 | 35.98 | 38.21 | 0 |

### P5

events: fx_issue=132, fx_collect=132, raw_issue=0, raw_collect=0, fx_ifail=0, fx_cfail=0, raw_ifail=0, raw_cfail=0

| population | frames | p50 | p95 | p99 | worst | long |
|---|---|---|---|---|---|---|
| with fxIssue | 132 | 34.54 | 36.13 | 36.26 | 36.58 | 0 |
| with fxCollect | 132 | 33.37 | 35.70 | 36.31 | 37.27 | 0 |
| none | 1792 | 33.20 | 34.90 | 36.00 | 37.37 | 0 |
| fxIssueOnly | 132 | 34.54 | 36.13 | 36.26 | 36.58 | 0 |
| fxCollectOnly | 132 | 33.37 | 35.70 | 36.31 | 37.27 | 0 |

## Sampler (last stat line per run; cumulative counters since start, ring p50/p95/max over the latest 256 samples)

| run | stream | total p50/p95/max | latency p50/p95/max | issued | completed | gaps | dropGen | invalid | discardTimeout | discardWaitFailed | glErrors |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 02_P1_r1 | FX | 0.241/0.393/0.592 | 33.322/33.567/40.695 | 175 |  | 0 | 0 | 0 | 0 | 0 |  |
| 03_P2_r1 | FX | 0.890/1.204/1.452 | 33.326/33.494/47.526 | 174 |  | 0 | 0 | 0 | 0 | 0 |  |
| 03_P2_r1 | RAW | 0.857/1.167/1.316 | 33.336/34.435/34.625 | 173 | 173 | 0 | 0 | 0 | 0 | 0 |  |
| 04_P3_r1 | RAW | 0.810/1.111/2.422 | 33.318/33.514/33.709 | 173 | 173 | 0 | 0 | 0 | 0 | 0 |  |
| 05_P4_r1 | CPURAW |  |  |  |  | 0 | 0 |  |  |  |  |
| 06_P5_r1 | CPURAW |  |  |  |  | 0 | 0 |  |  |  |  |
| 06_P5_r1 | FX | 0.736/1.240/4.246 | 33.324/33.646/64.670 | 174 |  | 0 | 0 | 0 | 0 | 0 |  |

Association is not GPU causation: the probe observes wall/CPU time per frame segment, not GPU timestamps.
