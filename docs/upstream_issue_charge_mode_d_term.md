# Charge mode: both tricklers briefly run at max speed at the start of a throw (D term divides by zero)

## Summary

In `charge_mode_wait_for_complete()` (`src/charge_mode.cpp`) the D term of both PID controllers divides by the time since the previous scale sample. On the first loop pass this time is often **0 ms**. The derivative then becomes `inf` (or `NaN` when Kd is 0, because `0 × inf = NaN`), and the clamp `fmaxf(min, fminf(p + i + d, max))` turns that into the **maximum speed**. Both tricklers run at max speed (5 rps by default) until the next scale sample arrives.

This happens randomly, depending on the timing of the scale. When it does, the throw gets an unwanted burst of powder at the start. With small charges and fine powder the throw can end far above the target.

## Where

`src/charge_mode.cpp`, `charge_mode_wait_for_complete()` (line numbers as of commit `e23cc45`):

```c
TickType_t last_sample_tick = xTaskGetTickCount();                                            // line 297
...
float elapse_time_ms = (current_sample_tick - last_sample_tick) / portTICK_RATE_MS;           // line 357
...
float fine_trickler_derivative = (fine_trickler_error - fine_trickler_last_error) / elapse_time_ms;       // line 359
...
float coarse_trickler_derivative = (coarse_trickler_error - coarse_trickler_last_error) / elapse_time_ms; // line 371
```

## Why the interval is 0 ms

`scale_block_wait_for_next_measurement()` takes a binary semaphore that the scale driver gives whenever a new measurement arrives. If a measurement arrived while charge mode was still in `charge_mode_wait_for_zero()`, the semaphore is already given. The first call in the control loop then returns immediately, in the same tick as `last_sample_tick`.

A second, smaller effect: `*_last_error` starts at `0`. Even with a non-zero interval, the first derivative is `error / elapsed`, which gives a kick at the start of every throw.

## How it was found

The bug was found while developing a new **Powder Analysis** feature, an automatic tuning of powder profiles. That feature runs test throws with a copy of the charge mode control loop and records every control-loop sample: time, weight, P and D terms, and the commanded speed of both tricklers. Test setup: Pico 2 W, A&D FX-series scale (about 20 samples/s), coarse stop threshold 3 gr, fine stop threshold 0.02 gr.

- Vihtavuori N320 (fine stick powder), target 8.2 gr: **13 of 26 throws** ended after exactly 1.6 s at 12 to 13 gr. With the same settings, the other throws took 8 to 30 s and ended close to the target. The trickler could be seen spinning much faster on the affected throws.
- With coarser powders and larger charges the effect is smaller, but it still shows up as coarse-phase times that randomly jump between two clusters (for example 2 s and 5 s with identical settings).

After the fix, a new N320 series of 18 throws showed no such jumps. All throws took 8.4 to 9.5 s and the coarse-phase times were consistent.

## Fix

The fix is implemented in the Powder Analysis branch, in both places that contain the control loop:

- `src/charge_mode.cpp`, `charge_mode_wait_for_complete()`: the charge mode itself
- `src/powder_analysis.cpp`, `run_throw()`: the copy used by the Powder Analysis

The D term is skipped on the first sample of a throw and whenever no time has passed since the last sample. The P and I terms are unchanged. From the second sample on, the D term is computed exactly as before.

Change in `src/charge_mode.cpp`:

```diff
+// Change of the error per millisecond for the D term. 0 on the first sample of a throw and when no
+// time passed since the last sample: on the first loop pass a measurement is often already waiting,
+// so the elapsed time is 0 ms (and the last error still 0). The plain division gave inf/NaN, which
+// sent both tricklers to maximum speed until the next sample.
+static float error_derivative(float error, float last_error, float elapse_time_ms, bool first_sample) {
+    if (first_sample || elapse_time_ms <= 0.0f) {
+        return 0.0f;
+    }
+    return (error - last_error) / elapse_time_ms;
+}
 ...
     bool should_coarse_trickler_move = true;
+    bool first_sample = true;   // no D term before the second measurement
 ...
-        float fine_trickler_derivative = (fine_trickler_error - fine_trickler_last_error) / elapse_time_ms;
+        float fine_trickler_derivative = error_derivative(fine_trickler_error, fine_trickler_last_error, elapse_time_ms, first_sample);
 ...
-            float coarse_trickler_derivative = (coarse_trickler_error - coarse_trickler_last_error) / elapse_time_ms;
+            float coarse_trickler_derivative = error_derivative(coarse_trickler_error, coarse_trickler_last_error, elapse_time_ms, first_sample);
 ...
         coarse_trickler_last_error = coarse_trickler_error;
+        first_sample = false;
```

Branch / commit: LINK_TO_BRANCH_OR_COMMIT

## Side note on the D term

Because the derivative is computed per millisecond, the D term is very small at the gains used in the default profiles. In our recordings (fine Kp 4.8) the fine D term near the end of a throw was below 1 % of the commanded speed at Kd 4, 2 to 4 % at Kd 16, and about 10 to 17 % at Kd 64. It also alternates between 0 and a value because the scale only changes its reading every other sample, in 0.02 gr steps. This might be worth a note in the tuning guide.
