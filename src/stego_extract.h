#ifndef STEGO_EXTRACT_H
#define STEGO_EXTRACT_H

//CS4463 06 STEGANOGRAPHY DCT PARITY PROJECT
//GROUP 16: DOMINIC MILLER, CARLOS MEJIA ROSALES, FIZA RAHMAN

#include "stego_embed.h"

int stego_extract(const char *stego_path, const char *output_path, const EmbedOptions *opts);

#endif 