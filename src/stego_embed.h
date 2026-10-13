#ifndef STEGO_EMBED_H
#define STEGO_EMBED_H

#include "jpeg_wrapper.h"

//CS4463 06 STEGANOGRAPHY DCT PARITY PROJECT
//GROUP 16: DOMINIC MILLER, CARLOS MEJIA ROSALES, FIZA RAHMAN

typedef struct {
    int coefs_per_block;
    int min_magnitude;
    int start_index;
    int parity_one_is_even;
    long random_bytes;
} EmbedOptions;

void embed_options_default(EmbedOptions *opts);

int stego_embed(const char *cover_path, const char *message_path, const char *output_path, const EmbedOptions *opts);

#endif
