#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "stego_extract.h"

/*
 * CS4463 06 STEGANOGRAPHY DCT PARITY PROJECT
 * GROUP 16: DOMINIC MILLER, CARLOS MEJIA ROSALES, FIZA RAHMAN
 */

/*
 * Returns the absolute magnitude of a DCT coefficient.
 */
static int coefficient_magnitude(JCOEF coef)
{
    return (coef < 0) ? -(int)coef : (int)coef;
}

/*
 * Decodes one message bit from a coefficient's parity.
 *
 * Default behavior:
 *     even coefficient = bit 1
 *     odd coefficient  = bit 0
 *
 * With the -p option:
 *     even coefficient = bit 0
 *     odd coefficient  = bit 1
 */
static int parity_to_bit(JCOEF coef, int parity_one_is_even)
{
    int coefficient_is_even;

    coefficient_is_even = ((coef % 2) == 0);

    if (parity_one_is_even) {
        return coefficient_is_even ? 1 : 0;
    }

    return coefficient_is_even ? 0 : 1;
}

/*
 * Counts the total number of eligible DCT coefficients using the exact
 * same traversal rules as embedding and extraction.
 *
 * This lets the extractor check whether the message length stored in
 * the header could actually fit inside the image.
 */
static long count_eligible_coefficients(
    JpegImage *img,
    const EmbedOptions *opts)
{
    long eligible_count;
    int c;

    eligible_count = 0;

    for (c = 0; c < img->num_components; c++) {
        JDIMENSION rows;
        JDIMENSION cols;
        JDIMENSION r;
        JDIMENSION col;

        rows = jpegwrap_blocks_high(img, c);
        cols = jpegwrap_blocks_wide(img, c);

        for (r = 0; r < rows; r++) {
            for (col = 0; col < cols; col++) {
                int coefs_used;
                int zi;

                coefs_used = 0;

                for (zi = opts->start_index;
                     zi < DCTSIZE2;
                     zi++) {
                    JCOEF coef;
                    int abs_value;

                    if (opts->coefs_per_block > 0 &&
                        coefs_used >= opts->coefs_per_block) {
                        break;
                    }

                    if (!jpegwrap_get_coef(
                            img,
                            c,
                            r,
                            col,
                            zi,
                            &coef)) {
                        continue;
                    }

                    abs_value = coefficient_magnitude(coef);

                    if (abs_value < opts->min_magnitude) {
                        continue;
                    }

                    eligible_count++;
                    coefs_used++;
                }
            }
        }
    }

    return eligible_count;
}

/*
 * Reads a JPEG stego image, extracts the 32-bit message-length header,
 * recovers the message data, and writes it to output_path.
 *
 * The same DCT options used for encoding must also be used for decoding.
 *
 * Returns the number of recovered message bytes on success.
 * Returns -1 on failure.
 */
int stego_extract(
    const char *stego_path,
    const char *output_path,
    const EmbedOptions *opts)
{
    JpegImage img;
    FILE *outfile;

    unsigned long header_value;
    unsigned long message_bytes;

    long message_bits;
    long total_bits;
    long bit_cursor;
    long extracted_bits;
    long extracted_bytes;
    long eligible_coefficients;
    long maximum_payload_bits;
    long maximum_payload_bytes;

    unsigned char byte_buffer;

    int header_finished;
    int extraction_failed;
    int c;

    if (stego_path == NULL ||
        output_path == NULL ||
        opts == NULL) {
        fprintf(stderr, "stego_extract: invalid argument\n");
        return -1;
    }

    /*
     * Index zero is the DC coefficient.
     * The project normally begins with AC coefficient index one.
     */
    if (opts->start_index < 1 ||
        opts->start_index >= DCTSIZE2) {
        fprintf(
            stderr,
            "stego_extract: start_index %d is out of range (1-63)\n",
            opts->start_index);

        return -1;
    }

    if (opts->min_magnitude < 0) {
        fprintf(
            stderr,
            "stego_extract: minimum magnitude cannot be negative\n");

        return -1;
    }

    if (opts->coefs_per_block < 0) {
        fprintf(
            stderr,
            "stego_extract: coefficients per block cannot be negative\n");

        return -1;
    }

    if (!jpegwrap_read(&img, stego_path)) {
        return -1;
    }

    /*
     * Determine how many bits could possibly be stored using the
     * selected traversal options.
     */
    eligible_coefficients =
        count_eligible_coefficients(&img, opts);

    if (eligible_coefficients < 32) {
        fprintf(
            stderr,
            "stego_extract: image contains only %ld eligible "
            "coefficient(s); at least 32 are required for the header\n",
            eligible_coefficients);

        jpegwrap_close(&img);
        return -1;
    }

    maximum_payload_bits = eligible_coefficients - 32;
    maximum_payload_bytes = maximum_payload_bits / 8;

    outfile = fopen(output_path, "wb");

    if (outfile == NULL) {
        fprintf(
            stderr,
            "stego_extract: cannot open output '%s'\n",
            output_path);

        jpegwrap_close(&img);
        return -1;
    }

    header_value = 0;
    message_bytes = 0;
    message_bits = 0;

    /*
     * Initially stop after 32 bits. Once the header is decoded,
     * total_bits is updated to include the payload.
     */
    total_bits = 32;
    bit_cursor = 0;
    byte_buffer = 0;

    header_finished = 0;
    extraction_failed = 0;

    /*
     * This traversal must exactly match stego_embed().
     */
    for (c = 0;
         c < img.num_components && bit_cursor < total_bits;
         c++) {
        JDIMENSION rows;
        JDIMENSION cols;
        JDIMENSION r;
        JDIMENSION col;

        rows = jpegwrap_blocks_high(&img, c);
        cols = jpegwrap_blocks_wide(&img, c);

        for (r = 0;
             r < rows && bit_cursor < total_bits;
             r++) {
            for (col = 0;
                 col < cols && bit_cursor < total_bits;
                 col++) {
                int coefs_used;
                int zi;

                coefs_used = 0;

                for (zi = opts->start_index;
                     zi < DCTSIZE2 && bit_cursor < total_bits;
                     zi++) {
                    JCOEF coef;
                    int abs_value;
                    int bit;

                    if (opts->coefs_per_block > 0 &&
                        coefs_used >= opts->coefs_per_block) {
                        break;
                    }

                    if (!jpegwrap_get_coef(
                            &img,
                            c,
                            r,
                            col,
                            zi,
                            &coef)) {
                        continue;
                    }

                    abs_value = coefficient_magnitude(coef);

                    if (abs_value < opts->min_magnitude) {
                        continue;
                    }

                    bit = parity_to_bit(
                        coef,
                        opts->parity_one_is_even);

                    /*
                     * The first 32 eligible coefficients contain the
                     * message length in bytes, MSB first.
                     */
                    if (bit_cursor < 32) {
                        header_value =
                            (header_value << 1) |
                            (unsigned long)bit;

                        if (bit_cursor == 31) {
                            message_bytes = header_value;
                            header_finished = 1;

                            /*
                             * Reject a corrupted or impossible header
                             * before attempting a huge extraction.
                             */
                            if (message_bytes >
                                (unsigned long)maximum_payload_bytes) {
                                fprintf(
                                    stderr,
                                    "stego_extract: invalid header: "
                                    "claims %lu byte(s), but the selected "
                                    "settings can hold at most %ld byte(s)\n",
                                    message_bytes,
                                    maximum_payload_bytes);

                                extraction_failed = 1;
                                break;
                            }

                            /*
                             * Guard the conversion before multiplying.
                             */
                            if (message_bytes >
                                (unsigned long)(LONG_MAX / 8L)) {
                                fprintf(
                                    stderr,
                                    "stego_extract: message length "
                                    "overflows local size limits\n");

                                extraction_failed = 1;
                                break;
                            }

                            message_bits =
                                (long)message_bytes * 8L;

                            total_bits =
                                32L + message_bits;

                            printf(
                                "stego_extract: header decoded: "
                                "expecting %lu byte(s)\n",
                                message_bytes);
                        }
                    } else {
                        long payload_position;
                        int bit_in_byte;

                        payload_position =
                            bit_cursor - 32;

                        bit_in_byte =
                            (int)(payload_position % 8);

                        /*
                         * Reconstruct message bytes MSB first.
                         */
                        byte_buffer =
                            (unsigned char)(
                                (byte_buffer << 1) | bit);

                        if (bit_in_byte == 7) {
                            if (fwrite(
                                    &byte_buffer,
                                    1,
                                    1,
                                    outfile) != 1) {
                                fprintf(
                                    stderr,
                                    "stego_extract: write error on '%s'\n",
                                    output_path);

                                extraction_failed = 1;
                                break;
                            }

                            byte_buffer = 0;
                        }
                    }

                    bit_cursor++;
                    coefs_used++;
                }

                if (extraction_failed) {
                    break;
                }
            }

            if (extraction_failed) {
                break;
            }
        }

        if (extraction_failed) {
            break;
        }
    }

    if (fclose(outfile) != 0) {
        fprintf(
            stderr,
            "stego_extract: error closing output '%s'\n",
            output_path);

        extraction_failed = 1;
    }

    jpegwrap_close(&img);

    if (extraction_failed) {
        /*
         * Do not leave a misleading partial output file behind.
         */
        remove(output_path);
        return -1;
    }

    if (!header_finished || bit_cursor < 32) {
        fprintf(
            stderr,
            "stego_extract: could not read the complete "
            "32-bit header\n");

        remove(output_path);
        return -1;
    }

    extracted_bits = bit_cursor - 32;
    extracted_bytes = extracted_bits / 8;

    if (extracted_bits < message_bits) {
        fprintf(
            stderr,
            "stego_extract: warning: recovered only "
            "%ld of %lu byte(s)\n",
            extracted_bytes,
            message_bytes);

        /*
         * A partial output should be treated as failure because it is
         * not identical to the embedded message.
         */
        remove(output_path);
        return -1;
    }

    printf(
        "stego_extract: wrote '%s' "
        "[%ld of %lu byte(s) recovered]\n",
        output_path,
        extracted_bytes,
        message_bytes);

    return (int)extracted_bytes;
}
