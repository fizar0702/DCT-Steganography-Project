#ifndef JPEG_WRAPPER_H
#define JPEG_WRAPPER_H

#include <stdio.h>
#include <jpeglib.h>

//CS4463 06 STEGANOGRAPHY DCT PARITY PROJECT
//GROUP 16: DOMINIC MILLER, CARLOS MEJIA ROSALES, FIZA RAHMAN

typedef JCOEF ZigzagBlock[DCTSIZE2];

typedef struct {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *infile;

    jvirt_barray_ptr *coef_arrays;
    int num_components;

    int is_open;
} 
JpegImage;

int jpegwrap_read(JpegImage *img, const char *filename);


JDIMENSION jpegwrap_blocks_wide(JpegImage *img, int component);
JDIMENSION jpegwrap_blocks_high(JpegImage *img, int component);


int jpegwrap_get_block(JpegImage *img, int component, JDIMENSION block_row, JDIMENSION block_col, ZigzagBlock out_block);
int jpegwrap_set_block(JpegImage *img, int component, JDIMENSION block_row, JDIMENSION block_col, const ZigzagBlock in_block);


int jpegwrap_get_coef(JpegImage *img, int component, JDIMENSION block_row, JDIMENSION block_col, int zigzag_index, JCOEF *out_value);
int jpegwrap_set_coef(JpegImage *img, int component, JDIMENSION block_row, JDIMENSION block_col, int zigzag_index, JCOEF value);


int jpegwrap_write(JpegImage *img, const char *filename);

void jpegwrap_close(JpegImage *img);

#endif 