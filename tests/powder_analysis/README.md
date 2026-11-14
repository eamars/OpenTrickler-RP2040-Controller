# Powder analysis unit tests

Host tests for the search logic in `src/powder_analysis_tuner.cpp`. They run on the PC with the
system C++ compiler, no Pico SDK or FreeRTOS needed.

    make -C tests/powder_analysis test

The tests drive the tuner against a simple simulated powder (`simulated_powder.cpp`) whose
overshoot grows with Kp and shrinks with Kd. A failing check prints its file and line; the
program exits with a non-zero code if any check failed.

Build artifacts go to `tests/powder_analysis/build/` (ignored by git). `make clean` removes them.
