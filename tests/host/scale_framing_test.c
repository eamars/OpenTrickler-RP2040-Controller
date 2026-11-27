/*
 * The scale's framing (ADR-017, gate 2), for the faults that happened or would read silently wrong: a frame stitched
 * from two, a frame with no number, the Generic's long line. Each case feeds a make's framer bytes and checks what
 * it hands on: the readings, and the frames with no number (NaN), which the scale counts as bad.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "scale_make.h"

#define MOST_FRAMES 16

typedef struct {
    float weights[MOST_FRAMES];
    size_t count;
} handed_t;


static void record(void *context, const char *frame, size_t len, float weight) {
    (void) frame;
    (void) len;
    handed_t *handed = context;
    if (handed->count < MOST_FRAMES) {
        handed->weights[handed->count] = weight;
    }
    handed->count += 1;
}


static void feed(scale_framer_t *framer, const char *bytes, handed_t *handed) {
    for (const char *at = bytes; *at != '\0'; at += 1) {
        scale_framer_take(framer, (uint8_t) *at, record, handed);
    }
}


// Each frame handed on, a reading or NaN, as the case expects them
static int check(const char *name, const handed_t *handed, const float *expected, size_t count) {
    bool same = handed->count == count;
    for (size_t idx = 0; same && idx < count; idx += 1) {
        float got = handed->weights[idx];
        same = isnan(expected[idx]) ? isnan(got) : (!isnan(got) && fabsf(got - expected[idx]) < 1e-4f);
    }
    printf("%s %s: %zu frames handed on", same ? "PASS" : "FAIL", name, handed->count);
    for (size_t idx = 0; idx < handed->count && idx < MOST_FRAMES; idx += 1) {
        printf(" %g", handed->weights[idx]);
    }
    printf("\n");
    return same ? 0 : 1;
}


// A stream from the framer's start: its first line ending ends the line under way, no frame
static int run(const char *name, const scale_make_t *make, const char *stream, const float *expected, size_t count) {
    scale_framer_t framer;
    handed_t handed = {0};
    scale_framer_start(&framer, make);
    feed(&framer, stream, &handed);
    return check(name, &handed, expected, count);
}


int main(void) {
    int failed = 0;

    // The count makes took a frame at a count of bytes since a line ending: a byte too many was read shifted
    const float creedmoor_added[] = {142.02f, NAN, 9.198f};
    failed += run("creedmoor, a byte added: its frame refused, not read shifted", &creedmoor_scale_make,
                  "\n+0142.02 GN \r\n+00012.34 GN \r\n+009.198 g  \r\n", creedmoor_added, 3);
    const float steinberg_together[] = {NAN, -1.14f};
    failed += run("steinberg, two frames run together: refused", &steinberg_scale_make,
                  "\nSD   -143.02 GN\rSD   -467.16 GN\r\nSD     -1.14 GN\r\n", steinberg_together, 2);

    // A frame stitched from two at a hand-over: the reader held the start of one, the port was drained, and the
    // next bytes came from the middle of another, where the first had stopped. The two make a frame of the right
    // length, "+  308.019GN", a weight of neither. Started afresh, the reader takes none of it
    {
        scale_framer_t framer;
        handed_t handed = {0};
        scale_framer_start(&framer, &ussolid_scale_make);
        feed(&framer, "\n+  320.344GN \r\n+  3", &handed);          // the start of a frame, held
        scale_framer_start(&framer, &ussolid_scale_make);           // the port drained (scale_reader_restart())
        feed(&framer, "08.019GN \r\n+  889.629GN \r\n", &handed);   // "+ 1508.019GN" from its fifth byte
        const float expected[] = {320.344f, 889.629f};
        failed += check("us solid, a hand-over in a frame: no stitched frame", &handed, expected, 2);
    }

    // The Radwag's frame is 21 bytes and starts "SUI": one that does not is a frame with no number
    const float radwag[] = {1.56f, NAN, NAN, -2.18f};
    failed += run("radwag, not SUI, and short: counted bad", &radwag_ps_r2_scale_make,
                  "\nSUI        1.56 gr \r\nXUI        1.56 gr \r\nT_A\r\nSUI?      -2.18 gr \r\n", radwag, 4);

    // A frame with no number: counted bad for every make, the Generic's included (it was dropped unseen)
    const float generic_no_number[] = {12.34f, NAN, -0.5f};
    failed += run("generic, a line with no number: counted bad", &generic_scale_make,
                  "\nST,GS,  12.34 g\r\nOL\r\n\r\n-.5\r\n", generic_no_number, 3);
    const float fa_en_no_number[] = {0.0238f, NAN, -5.4205f};
    failed += run("fa-e/en, a line not of its frames: counted bad", &fa_en_scale_make,
                  "\rE *+  0.0238  g\r\n*+  0.02\r\nE *-  5.4205  g\r\n", fa_en_no_number, 3);

    // The Generic took a line longer than its buffer in two, and read its tail: "...  12" then "3.45 g" read 3.45
    const float generic_long[] = {NAN, 7.5f};
    failed += run("generic, a line too long: dropped whole, counted bad", &generic_scale_make,
                  "\nST,GS,US,NT,XX,YY,ZZ,0000000  12" "3.45 g\r\n+ 7.50 g\r\n", generic_long, 2);
    const float sartorius_long[] = {NAN, 27.35f};
    failed += run("sartorius, a line too long: dropped whole, counted bad", &sartorius_scale_make,
                  "\r+                                  12345.67 g\r\n+   27.350\r\n", sartorius_long, 2);

    // The JM Science's frame runs from its header for its length: one cut short by the next header is counted bad
    const float jm[] = {12.345f, NAN, -0.002f};
    failed += run("jm science, a frame cut short by the next: counted bad", &jm_science_scale_make,
                  "\r\nE  +   12.345 g  \r\nE  +   1E  -    0.002 g  \r\n", jm, 3);

    // The A&D, as on the bench: whole frames read, a line of another length counted bad, a blank line no frame
    const float and_fxi[] = {192.18f, NAN, -0.02f};
    failed += run("a&d, its frames", &and_fxi_scale_make,
                  "GN\r\nST,+00192.18 GN\r\n000.00 GN\r\n\r\nUS,-00000.02 GN\r\n", and_fxi, 3);

    printf("VERDICT %s\n", failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}
