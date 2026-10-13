#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "jpeg_wrapper.h"
#include "stego_analyze.h"

/* CS4463 06 STEGANOGRAPHY DCT PARITY PROJECT
 * GROUP 16: DOMINIC MILLER, CARLOS MEJIA ROSALES, FIZA RAHMAN
 *
 * This module performs simple steganalysis-oriented comparisons:
 *   - DCT coefficient histogram (-255 through 255, with overflow bins)
 *   - zero/nonzero coefficient counts
 *   - even/odd parity balance among usable AC coefficients
 *   - number and percentage of coefficients changed
 *   - mean absolute difference and root mean square difference
 *   - chi-square score for adjacent even/odd histogram pairs
 */

#define HIST_MIN (-255)
#define HIST_MAX 255
#define HIST_BINS (HIST_MAX - HIST_MIN + 3)
#define HIST_UNDERFLOW 0
#define HIST_OFFSET 1
#define HIST_OVERFLOW (HIST_BINS - 1)

typedef struct {
    unsigned long histogram[HIST_BINS];
    unsigned long total;
    unsigned long zeros;
    unsigned long nonzero;
    unsigned long ac_total;
    unsigned long ac_usable;
    unsigned long ac_even;
    unsigned long ac_odd;
} ImageStats;

static int histogram_index(long value) {
    if (value < HIST_MIN)
        return HIST_UNDERFLOW;
    if (value > HIST_MAX)
        return HIST_OVERFLOW;
    return (int)(value - HIST_MIN + HIST_OFFSET);
}

static void collect_stats(JpegImage *img, ImageStats *stats) {
    int component;
    JDIMENSION row;
    JDIMENSION col;
    int k;
    ZigzagBlock block;

    memset(stats, 0, sizeof(*stats));

    for (component = 0; component < img->num_components; component++) {
        for (row = 0; row < jpegwrap_blocks_high(img, component); row++) {
            for (col = 0; col < jpegwrap_blocks_wide(img, component); col++) {
                if (!jpegwrap_get_block(img, component, row, col, block))
                    continue;

                for (k = 0; k < DCTSIZE2; k++) {
                    long value = (long)block[k];
                    stats->histogram[histogram_index(value)]++;
                    stats->total++;

                    if (value == 0)
                        stats->zeros++;
                    else
                        stats->nonzero++;

                    if (k != 0) {
                        long magnitude;
                        stats->ac_total++;

                        /* The embedding code uses nonzero AC coefficients.
                         * Count parity only for coefficients that could carry data.
                         */
                        if (value != 0) {
                            stats->ac_usable++;
                            magnitude = (value < 0) ? -value : value;
                            if ((magnitude & 1L) == 0)
                                stats->ac_even++;
                            else
                                stats->ac_odd++;
                        }
                    }
                }
            }
        }
    }
}

static double parity_chi_square(const ImageStats *stats) {
    double total = (double)(stats->ac_even + stats->ac_odd);
    double expected;
    double d_even;
    double d_odd;

    if (total <= 0.0)
        return 0.0;

    expected = total / 2.0;
    d_even = (double)stats->ac_even - expected;
    d_odd = (double)stats->ac_odd - expected;

    return (d_even * d_even / expected) + (d_odd * d_odd / expected);
}

/* Pair adjacent coefficient magnitudes, such as 2/3, 4/5, and so on.
 * LSB replacement tends to make neighboring bins more alike.
 * A lower score generally means a more pair-balanced histogram.
 */
static double adjacent_pair_chi_square(const ImageStats *stats) {
    int magnitude;
    double score = 0.0;

    for (magnitude = 2; magnitude < HIST_MAX; magnitude += 2) {
        int pos_even = histogram_index(magnitude);
        int pos_odd = histogram_index(magnitude + 1);
        int neg_even = histogram_index(-magnitude);
        int neg_odd = histogram_index(-(magnitude + 1));
        double a;
        double b;
        double expected;
        double diff;

        a = (double)stats->histogram[pos_even];
        b = (double)stats->histogram[pos_odd];
        if (a + b > 0.0) {
            expected = (a + b) / 2.0;
            diff = a - expected;
            score += (diff * diff / expected) + ((b - expected) * (b - expected) / expected);
        }

        a = (double)stats->histogram[neg_even];
        b = (double)stats->histogram[neg_odd];
        if (a + b > 0.0) {
            expected = (a + b) / 2.0;
            diff = a - expected;
            score += (diff * diff / expected) + ((b - expected) * (b - expected) / expected);
        }
    }

    return score;
}

static int write_histogram_csv(const char *path, const ImageStats *stats) {
    FILE *out;
    int value;

    out = fopen(path, "w");
    if (!out) {
        fprintf(stderr, "stego_analyze: cannot write '%s'\n", path);
        return 0;
    }

    fprintf(out, "coefficient,count\n");
    fprintf(out, "below_%d,%lu\n", HIST_MIN, stats->histogram[HIST_UNDERFLOW]);
    for (value = HIST_MIN; value <= HIST_MAX; value++)
        fprintf(out, "%d,%lu\n", value, stats->histogram[histogram_index(value)]);
    fprintf(out, "above_%d,%lu\n", HIST_MAX, stats->histogram[HIST_OVERFLOW]);

    fclose(out);
    return 1;
}

static void make_related_path(const char *report_path,
                              const char *suffix,
                              char *output,
                              size_t output_size) {
    const char *dot = strrchr(report_path, '.');
    size_t base_length;

    if (dot == NULL)
        base_length = strlen(report_path);
    else
        base_length = (size_t)(dot - report_path);

    if (base_length + strlen(suffix) + 1 > output_size)
        base_length = output_size - strlen(suffix) - 1;

    memcpy(output, report_path, base_length);
    output[base_length] = '\0';
    strncat(output, suffix, output_size - strlen(output) - 1);
}

static int images_are_comparable(JpegImage *a, JpegImage *b) {
    int component;

    if (a->num_components != b->num_components)
        return 0;

    for (component = 0; component < a->num_components; component++) {
        if (jpegwrap_blocks_wide(a, component) != jpegwrap_blocks_wide(b, component) ||
            jpegwrap_blocks_high(a, component) != jpegwrap_blocks_high(b, component))
            return 0;
    }

    return 1;
}

int stego_analyze(const char *original_path,
                  const char *stego_path,
                  const char *report_path) {
    JpegImage original;
    JpegImage stego;
    ImageStats original_stats;
    ImageStats stego_stats;
    FILE *report;
    char original_csv[1024];
    char stego_csv[1024];
    unsigned long changed = 0;
    unsigned long compared = 0;
    double absolute_sum = 0.0;
    double squared_sum = 0.0;
    int component;
    JDIMENSION row;
    JDIMENSION col;
    int k;
    ZigzagBlock original_block;
    ZigzagBlock stego_block;

    memset(&original, 0, sizeof(original));
    memset(&stego, 0, sizeof(stego));

    if (!jpegwrap_read(&original, original_path))
        return -1;

    if (!jpegwrap_read(&stego, stego_path)) {
        jpegwrap_close(&original);
        return -1;
    }

    if (!images_are_comparable(&original, &stego)) {
        fprintf(stderr, "stego_analyze: images have different component/block dimensions\n");
        jpegwrap_close(&original);
        jpegwrap_close(&stego);
        return -1;
    }

    collect_stats(&original, &original_stats);
    collect_stats(&stego, &stego_stats);

    for (component = 0; component < original.num_components; component++) {
        for (row = 0; row < jpegwrap_blocks_high(&original, component); row++) {
            for (col = 0; col < jpegwrap_blocks_wide(&original, component); col++) {
                if (!jpegwrap_get_block(&original, component, row, col, original_block) ||
                    !jpegwrap_get_block(&stego, component, row, col, stego_block))
                    continue;

                for (k = 0; k < DCTSIZE2; k++) {
                    double difference = (double)stego_block[k] - (double)original_block[k];
                    if (difference != 0.0)
                        changed++;
                    if (difference < 0.0)
                        absolute_sum -= difference;
                    else
                        absolute_sum += difference;
                    squared_sum += difference * difference;
                    compared++;
                }
            }
        }
    }

    report = fopen(report_path, "w");
    if (!report) {
        fprintf(stderr, "stego_analyze: cannot write '%s'\n", report_path);
        jpegwrap_close(&original);
        jpegwrap_close(&stego);
        return -1;
    }

    fprintf(report, "JPEG DCT Statistical Comparison\n");
    fprintf(report, "===============================\n");
    fprintf(report, "Original: %s\n", original_path);
    fprintf(report, "Stego:    %s\n\n", stego_path);

    fprintf(report, "Coefficient comparison\n");
    fprintf(report, "----------------------\n");
    fprintf(report, "Total coefficients compared: %lu\n", compared);
    fprintf(report, "Changed coefficients:        %lu\n", changed);
    fprintf(report, "Changed percentage:          %.6f%%\n",
            compared ? (100.0 * (double)changed / (double)compared) : 0.0);
    fprintf(report, "Mean absolute difference:    %.8f\n",
            compared ? absolute_sum / (double)compared : 0.0);
    fprintf(report, "Root mean square difference: %.8f\n\n",
            compared ? sqrt(squared_sum / (double)compared) : 0.0);

    fprintf(report, "Original image statistics\n");
    fprintf(report, "-------------------------\n");
    fprintf(report, "Zero coefficients:             %lu (%.6f%%)\n",
            original_stats.zeros,
            original_stats.total ? 100.0 * (double)original_stats.zeros / (double)original_stats.total : 0.0);
    fprintf(report, "Usable nonzero AC coefficients:%lu\n", original_stats.ac_usable);
    fprintf(report, "Even magnitude AC values:      %lu (%.6f%%)\n",
            original_stats.ac_even,
            original_stats.ac_usable ? 100.0 * (double)original_stats.ac_even / (double)original_stats.ac_usable : 0.0);
    fprintf(report, "Odd magnitude AC values:       %lu (%.6f%%)\n",
            original_stats.ac_odd,
            original_stats.ac_usable ? 100.0 * (double)original_stats.ac_odd / (double)original_stats.ac_usable : 0.0);
    fprintf(report, "Parity chi-square:              %.8f\n", parity_chi_square(&original_stats));
    fprintf(report, "Adjacent-pair chi-square:       %.8f\n\n", adjacent_pair_chi_square(&original_stats));

    fprintf(report, "Stego image statistics\n");
    fprintf(report, "----------------------\n");
    fprintf(report, "Zero coefficients:             %lu (%.6f%%)\n",
            stego_stats.zeros,
            stego_stats.total ? 100.0 * (double)stego_stats.zeros / (double)stego_stats.total : 0.0);
    fprintf(report, "Usable nonzero AC coefficients:%lu\n", stego_stats.ac_usable);
    fprintf(report, "Even magnitude AC values:      %lu (%.6f%%)\n",
            stego_stats.ac_even,
            stego_stats.ac_usable ? 100.0 * (double)stego_stats.ac_even / (double)stego_stats.ac_usable : 0.0);
    fprintf(report, "Odd magnitude AC values:       %lu (%.6f%%)\n",
            stego_stats.ac_odd,
            stego_stats.ac_usable ? 100.0 * (double)stego_stats.ac_odd / (double)stego_stats.ac_usable : 0.0);
    fprintf(report, "Parity chi-square:              %.8f\n", parity_chi_square(&stego_stats));
    fprintf(report, "Adjacent-pair chi-square:       %.8f\n\n", adjacent_pair_chi_square(&stego_stats));

    fprintf(report, "Interpretation notes\n");
    fprintf(report, "--------------------\n");
    fprintf(report, "1. A higher changed percentage means more DCT coefficients were modified.\n");
    fprintf(report, "2. LSB-style embedding often pushes even/odd magnitudes toward a 50/50 split.\n");
    fprintf(report, "3. A lower adjacent-pair chi-square can indicate that neighboring histogram bins\n");
    fprintf(report, "   have become more evenly paired. These values are indicators, not proof.\n");

    fclose(report);

    make_related_path(report_path, "_original_histogram.csv", original_csv, sizeof(original_csv));
    make_related_path(report_path, "_stego_histogram.csv", stego_csv, sizeof(stego_csv));

    if (!write_histogram_csv(original_csv, &original_stats) ||
        !write_histogram_csv(stego_csv, &stego_stats)) {
        jpegwrap_close(&original);
        jpegwrap_close(&stego);
        return -1;
    }

    jpegwrap_close(&original);
    jpegwrap_close(&stego);

    printf("stego_analyze: wrote '%s'\n", report_path);
    printf("stego_analyze: wrote '%s'\n", original_csv);
    printf("stego_analyze: wrote '%s'\n", stego_csv);

    return 0;
}
