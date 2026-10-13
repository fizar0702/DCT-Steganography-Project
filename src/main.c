#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "stego_embed.h"
#include "stego_extract.h"
#include "stego_analyze.h"

/*
 * CS4463 06 STEGANOGRAPHY DCT PARITY PROJECT
 * GROUP 16: DOMINIC MILLER, CARLOS MEJIA ROSALES, FIZA RAHMAN
 */

/*
 * Displays command usage and examples.
 */
static void print_usage(const char *prog)
{
    fprintf(
        stderr,
        "Usage:\n"
        "  %s encode <cover.jpg> <message_file|random> <output.jpg> [options]\n"
        "  %s decode <stego.jpg> <output_file>                        [options]\n"
        "  %s analyze <original.jpg> <stego.jpg> <report.txt>\n"
        "\n"
        "Options:\n"
        "  Every DCT option used during encode must also be used during decode.\n"
        "\n"
        "  -c N   Maximum eligible AC coefficients used per 8x8 block\n"
        "         N must be from 0 through 63. A value of 0 means all.\n"
        "         Default: 0\n"
        "\n"
        "  -m N   Minimum absolute DCT coefficient magnitude\n"
        "         N must be a nonnegative integer.\n"
        "         Default: 1\n"
        "\n"
        "  -s N   Zigzag starting index\n"
        "         N must be from 1 through 63. Index 0 is the DC coefficient.\n"
        "         Default: 1\n"
        "\n"
        "  -p     Invert parity mapping\n"
        "         Default: bit 1 maps to even and bit 0 maps to odd.\n"
        "         With -p: bit 1 maps to odd and bit 0 maps to even.\n"
        "\n"
        "  -r N   Number of random bytes when the message argument is random\n"
        "         N must be a positive integer.\n"
        "         Default: 1000\n"
        "\n"
        "Examples:\n"
        "  %s encode cover.jpg secret.txt stego.jpg\n"
        "  %s decode stego.jpg recovered.txt\n"
        "\n"
        "  %s encode cover.jpg secret.txt stego.jpg -c 3 -m 2 -s 1\n"
        "  %s decode stego.jpg recovered.txt -c 3 -m 2 -s 1\n"
        "\n"
        "  %s encode cover.jpg secret.txt parity.jpg -p\n"
        "  %s decode parity.jpg recovered.txt -p\n"
        "\n"
        "  %s encode cover.jpg random random_stego.jpg -r 10000\n"
        "  %s decode random_stego.jpg recovered_random.bin\n"
        "\n"
        "  %s analyze cover.jpg stego.jpg analysis.txt\n",
        prog,
        prog,
        prog,
        prog,
        prog,
        prog,
        prog,
        prog,
        prog,
        prog,
        prog,
        prog);
}

/*
 * Parses an integer represented as text.
 *
 * Returns 1 on success and stores the value in output_value.
 * Returns 0 if the input is malformed or outside the int range.
 */
static int parse_integer(const char *text, int *output_value)
{
    char *end;
    long value;

    if (text == NULL || output_value == NULL) {
        return 0;
    }

    errno = 0;
    end = NULL;

    value = strtol(text, &end, 10);

    if (errno != 0 ||
        end == text ||
        *end != '\0' ||
        value < INT_MIN ||
        value > INT_MAX) {
        return 0;
    }

    *output_value = (int)value;
    return 1;
}

/*
 * Parses a positive long integer.
 *
 * Used for the number of random bytes requested with -r.
 */
static int parse_positive_long(const char *text, long *output_value)
{
    char *end;
    long value;

    if (text == NULL || output_value == NULL) {
        return 0;
    }

    errno = 0;
    end = NULL;

    value = strtol(text, &end, 10);

    if (errno != 0 ||
        end == text ||
        *end != '\0' ||
        value <= 0) {
        return 0;
    }

    *output_value = value;
    return 1;
}

int main(int argc, char *argv[])
{
    EmbedOptions opts;
    const char *operation;

    int is_encode;
    int is_decode;
    int minimum_arguments;
    int i;
    int result;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    operation = argv[1];

    /*
     * Statistical comparison mode.
     */
    if (strcmp(operation, "analyze") == 0) {
        if (argc != 5) {
            fprintf(
                stderr,
                "error: analyze requires an original JPEG, "
                "a stego JPEG, and a report filename\n\n");

            print_usage(argv[0]);
            return 1;
        }

        result = stego_analyze(
            argv[2],
            argv[3],
            argv[4]);

        return (result < 0) ? 1 : 0;
    }

    /*
     * The remaining valid operations are encode and decode.
     */
    if (strcmp(operation, "encode") != 0 &&
        strcmp(operation, "decode") != 0) {
        fprintf(
            stderr,
            "error: first argument must be "
            "'encode', 'decode', or 'analyze'\n\n");

        print_usage(argv[0]);
        return 1;
    }

    is_encode = (strcmp(operation, "encode") == 0);
    is_decode = !is_encode;

    /*
     * Encode requires:
     *   program, encode, cover, message, output
     *
     * Decode requires:
     *   program, decode, stego, output
     */
    minimum_arguments = is_encode ? 5 : 4;

    if (argc < minimum_arguments) {
        fprintf(
            stderr,
            "error: missing required arguments for '%s'\n\n",
            operation);

        print_usage(argv[0]);
        return 1;
    }

    embed_options_default(&opts);

    /*
     * Parse optional arguments.
     */
    for (i = minimum_arguments; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
            int value;

            if (++i >= argc) {
                fprintf(
                    stderr,
                    "error: -c requires an integer value\n");

                return 1;
            }

            if (!parse_integer(argv[i], &value) ||
                value < 0 ||
                value > 63) {
                fprintf(
                    stderr,
                    "error: -c must be an integer from 0 through 63\n");

                return 1;
            }

            opts.coefs_per_block = value;
        }

        else if (strcmp(argv[i], "-m") == 0) {
            int value;

            if (++i >= argc) {
                fprintf(
                    stderr,
                    "error: -m requires an integer value\n");

                return 1;
            }

            if (!parse_integer(argv[i], &value) ||
                value < 0) {
                fprintf(
                    stderr,
                    "error: -m must be a nonnegative integer\n");

                return 1;
            }

            opts.min_magnitude = value;
        }

        else if (strcmp(argv[i], "-s") == 0) {
            int value;

            if (++i >= argc) {
                fprintf(
                    stderr,
                    "error: -s requires an integer value\n");

                return 1;
            }

            if (!parse_integer(argv[i], &value) ||
                value < 1 ||
                value >= DCTSIZE2) {
                fprintf(
                    stderr,
                    "error: -s must be an integer from 1 through 63\n");

                return 1;
            }

            opts.start_index = value;
        }

        else if (strcmp(argv[i], "-p") == 0) {
            opts.parity_one_is_even = 0;
        }

        else if (strcmp(argv[i], "-r") == 0) {
            if (++i >= argc) {
                fprintf(
                    stderr,
                    "error: -r requires a positive integer value\n");

                return 1;
            }

            if (!parse_positive_long(
                    argv[i],
                    &opts.random_bytes)) {
                fprintf(
                    stderr,
                    "error: -r must be a positive integer\n");

                return 1;
            }
        }

        else {
            fprintf(
                stderr,
                "error: unknown option '%s'\n\n",
                argv[i]);

            print_usage(argv[0]);
            return 1;
        }
    }

    /*
     * -r only affects random-data generation during encoding.
     */
    if (is_decode && opts.random_bytes != 1000) {
        fprintf(
            stderr,
            "warning: -r is ignored during decode\n");
    }

    /*
     * A random byte count is meaningful only when the message argument
     * is exactly "random".
     */
    if (is_encode &&
        opts.random_bytes != 1000 &&
        strcmp(argv[3], "random") != 0) {
        fprintf(
            stderr,
            "warning: -r is ignored because the message argument "
            "is not 'random'\n");
    }

    if (is_encode) {
        result = stego_embed(
            argv[2],
            argv[3],
            argv[4],
            &opts);
    } else {
        result = stego_extract(
            argv[2],
            argv[3],
            &opts);
    }

    return (result < 0) ? 1 : 0;
}
