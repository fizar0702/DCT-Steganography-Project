#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "stego_embed.h"

/*
 * CS4463 06 STEGANOGRAPHY DCT PARITY PROJECT
 * GROUP 16: DOMINIC MILLER, CARLOS MEJIA ROSALES, FIZA RAHMAN
 */

/*
 * Reads an entire file into a heap-allocated byte buffer.
 * The caller must free the returned buffer.
 */
static unsigned char *read_file_bytes(const char *path, long *size)
{
    FILE *fp;
    unsigned char *buf;
    size_t bytes_read;

    if (path == NULL || size == NULL) {
        fprintf(stderr, "stego_embed: invalid file arguments\n");
        return NULL;
    }

    fp = fopen(path, "rb");

    if (fp == NULL) {
        fprintf(stderr, "stego_embed: cannot open '%s'\n", path);
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "stego_embed: cannot seek in '%s'\n", path);
        fclose(fp);
        return NULL;
    }

    *size = ftell(fp);

    if (*size < 0) {
        fprintf(stderr,
                "stego_embed: cannot determine size of '%s'\n",
                path);
        fclose(fp);
        return NULL;
    }

    rewind(fp);

    /*
     * malloc(0) is implementation-dependent, so allocate at least one byte.
     */
    buf = (unsigned char *)malloc((size_t)((*size > 0) ? *size : 1));

    if (buf == NULL) {
        fprintf(stderr,
                "stego_embed: malloc failed (%ld bytes)\n",
                *size);
        fclose(fp);
        return NULL;
    }

    if (*size > 0) {
        bytes_read = fread(buf, 1, (size_t)*size, fp);

        if (bytes_read != (size_t)*size) {
            fprintf(stderr,
                    "stego_embed: read error on '%s'\n",
                    path);
            free(buf);
            fclose(fp);
            return NULL;
        }
    }

    fclose(fp);
    return buf;
}

/*
 * Creates a heap-allocated buffer containing random bytes.
 * /dev/urandom is preferred. rand() is used only as a fallback.
 */
static unsigned char *generate_random_bytes(long size)
{
    FILE *fp;
    unsigned char *buf;
    long i;

    if (size <= 0) {
        fprintf(stderr,
                "stego_embed: random byte count must be greater than zero\n");
        return NULL;
    }

    buf = (unsigned char *)malloc((size_t)size);

    if (buf == NULL) {
        fprintf(stderr,
                "stego_embed: malloc failed (%ld bytes)\n",
                size);
        return NULL;
    }

    fp = fopen("/dev/urandom", "rb");

    if (fp != NULL) {
        if (fread(buf, 1, (size_t)size, fp) == (size_t)size) {
            fclose(fp);
            return buf;
        }

        fprintf(stderr,
                "stego_embed: warning: could not read all bytes "
                "from /dev/urandom; using fallback\n");

        fclose(fp);
    }

    srand((unsigned int)time(NULL));

    for (i = 0; i < size; i++) {
        buf[i] = (unsigned char)(rand() & 0xFF);
    }

    return buf;
}

/*
 * Gets a bit from a byte buffer.
 * Position zero is the most-significant bit of byte zero.
 */
static int buf_get_bit(const unsigned char *buf, long pos)
{
    return (buf[pos / 8] >> (7 - (int)(pos % 8))) & 1;
}

/*
 * Gets a bit from the 32-bit message-length header.
 * Position zero represents bit 31.
 */
static int u32_get_bit(unsigned long value, int pos)
{
    return (int)((value >> (31 - pos)) & 1UL);
}

/*
 * Returns true when the coefficient parity already represents the bit.
 */
static int parity_matches(
    JCOEF coef,
    int bit,
    int parity_one_is_even)
{
    int coefficient_is_even;
    int wanted_even;

    coefficient_is_even = ((coef % 2) == 0);

    if (parity_one_is_even) {
        wanted_even = (bit == 1);
    } else {
        wanted_even = (bit == 0);
    }

    return coefficient_is_even == wanted_even;
}

/*
 * Changes coefficient parity while preserving coefficient eligibility.
 *
 * Important:
 * If min_magnitude is 2, changing 2 to 1 would make the coefficient
 * unusable during extraction. That would cause the encoder and decoder
 * to visit different coefficients and corrupt the message.
 *
 * Therefore:
 *   - If moving toward zero would cross below min_magnitude,
 *     move away from zero instead.
 *   - Otherwise, move one step toward zero.
 */
static JCOEF flip_parity(JCOEF coef, int min_magnitude)
{
    int abs_value;
    int sign;

    abs_value = (coef < 0) ? -(int)coef : (int)coef;
    sign = (coef < 0) ? -1 : 1;

    /*
     * Zero can only be eligible when min_magnitude is zero.
     */
    if (abs_value == 0) {
        return (JCOEF)1;
    }

    /*
     * Moving toward zero would make this coefficient fail the same
     * magnitude test during extraction. Move away from zero instead.
     */
    if ((abs_value - 1) < min_magnitude) {
        return (JCOEF)(sign * (abs_value + 1));
    }

    /*
     * Safe to move toward zero while remaining eligible.
     */
    return (JCOEF)(sign * (abs_value - 1));
}

/*
 * Sets the default embedding/extraction options.
 */
void embed_options_default(EmbedOptions *opts)
{
    if (opts == NULL) {
        return;
    }

    opts->coefs_per_block = 0;
    opts->min_magnitude = 1;
    opts->start_index = 1;
    opts->parity_one_is_even = 1;
    opts->random_bytes = 1000;
}

/*
 * Embeds a normal file or generated random data into JPEG DCT
 * coefficient parity.
 *
 * The first 32 embedded bits contain the message length in bytes.
 *
 * Returns the total number of embedded bits, including the header,
 * or -1 on failure.
 */
int stego_embed(
    const char *cover_path,
    const char *message_path,
    const char *output_path,
    const EmbedOptions *opts)
{
    JpegImage img;
    unsigned char *msg_buf;
    long msg_bytes;
    long msg_bits;
    long total_bits;
    long bit_cursor;
    int c;

    if (cover_path == NULL ||
        message_path == NULL ||
        output_path == NULL ||
        opts == NULL) {
        fprintf(stderr, "stego_embed: invalid argument\n");
        return -1;
    }

    /*
     * Index 0 is the DC coefficient. This project normally starts at 1.
     * Change the lower bound to 0 only if DC embedding is intentionally
     * supported.
     */
    if (opts->start_index < 1 || opts->start_index >= DCTSIZE2) {
        fprintf(stderr,
                "stego_embed: start_index %d is out of range (1-63)\n",
                opts->start_index);
        return -1;
    }

    if (opts->min_magnitude < 0) {
        fprintf(stderr,
                "stego_embed: minimum magnitude cannot be negative\n");
        return -1;
    }

    if (opts->coefs_per_block < 0) {
        fprintf(stderr,
                "stego_embed: coefficients per block cannot be negative\n");
        return -1;
    }

    msg_buf = NULL;
    msg_bytes = 0;

    if (strcmp(message_path, "random") == 0) {
        msg_bytes = opts->random_bytes;
        msg_buf = generate_random_bytes(msg_bytes);

        if (msg_buf == NULL) {
            return -1;
        }

        printf("stego_embed: generated %ld random bytes\n",
               msg_bytes);
    } else {
        msg_buf = read_file_bytes(message_path, &msg_bytes);

        if (msg_buf == NULL) {
            return -1;
        }
    }

    /*
     * The current header stores a 32-bit unsigned byte count.
     */
    if ((unsigned long)msg_bytes > 0xFFFFFFFFUL) {
        fprintf(stderr,
                "stego_embed: message is too large for the 32-bit header\n");
        free(msg_buf);
        return -1;
    }

    msg_bits = msg_bytes * 8L;
    total_bits = 32L + msg_bits;

    if (!jpegwrap_read(&img, cover_path)) {
        free(msg_buf);
        return -1;
    }

    bit_cursor = 0;

    /*
     * This traversal must exactly match stego_extract().
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
                    JCOEF new_coef;
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

                    abs_value = (coef < 0)
                                    ? -(int)coef
                                    : (int)coef;

                    if (abs_value < opts->min_magnitude) {
                        continue;
                    }

                    if (bit_cursor < 32) {
                        bit = u32_get_bit(
                            (unsigned long)msg_bytes,
                            (int)bit_cursor);
                    } else {
                        bit = buf_get_bit(
                            msg_buf,
                            bit_cursor - 32);
                    }

                    if (!parity_matches(
                            coef,
                            bit,
                            opts->parity_one_is_even)) {
                        new_coef = flip_parity(
                            coef,
                            opts->min_magnitude);

                        /*
                         * Safety check: the changed coefficient must remain
                         * eligible under the same extraction settings.
                         */
                        if (((new_coef < 0)
                                ? -(int)new_coef
                                : (int)new_coef)
                            < opts->min_magnitude) {
                            fprintf(stderr,
                                    "stego_embed: internal error: "
                                    "coefficient became ineligible\n");

                            jpegwrap_close(&img);
                            free(msg_buf);
                            return -1;
                        }

                        if (!jpegwrap_set_coef(
                                &img,
                                c,
                                r,
                                col,
                                zi,
                                new_coef)) {
                            fprintf(stderr,
                                    "stego_embed: failed to update "
                                    "DCT coefficient\n");

                            jpegwrap_close(&img);
                            free(msg_buf);
                            return -1;
                        }
                    }

                    bit_cursor++;
                    coefs_used++;
                }
            }
        }
    }

    if (bit_cursor < 32) {
        fprintf(stderr,
                "stego_embed: cover image too small; only "
                "%ld of 32 header bits embedded\n",
                bit_cursor);

        jpegwrap_close(&img);
        free(msg_buf);
        return -1;
    }

    /*
     * Do not write a misleading partial stego image.
     */
    if (bit_cursor < total_bits) {
        fprintf(stderr,
                "stego_embed: cover image too small; only "
                "%ld of %ld payload bits could be embedded\n",
                bit_cursor - 32,
                msg_bits);

        jpegwrap_close(&img);
        free(msg_buf);
        return -1;
    }

    if (!jpegwrap_write(&img, output_path)) {
        jpegwrap_close(&img);
        free(msg_buf);
        return -1;
    }

    printf(
        "stego_embed: wrote '%s' "
        "[message %ld bytes, %ld bits embedded "
        "(32 header + %ld payload)]\n",
        output_path,
        msg_bytes,
        bit_cursor,
        bit_cursor - 32);

    jpegwrap_close(&img);
    free(msg_buf);

    return (int)bit_cursor;
}
