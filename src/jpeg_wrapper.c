#include <stdlib.h>
#include <string.h>
#include "jpeg_wrapper.h"

//CS4463 06 STEGANOGRAPHY DCT PARITY PROJECT
//GROUP 16: DOMINIC MILLER, CARLOS MEJIA ROSALES, FIZA RAHMAN

//mapping for array size DCTSIZE2 (64) with indices in zigzag order to scan natrually ordered arrays of coefficients in
//zigzag order is useful here because it sorts coefficients from lowest to highest frequency (aka most to least visually significant)
static const int zigzag_to_natural[DCTSIZE2] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

//attempts to read in ALL the DCT tables from an image "filename" at once, into virtual arrays (typically memory efficient)
// returns 1 on success, 0 on failure 
int jpegwrap_read(JpegImage *img, const char *filename) {
    memset(img, 0, sizeof(*img));

    //open filename for reading 
    img->infile = fopen(filename, "rb");
    //if fopen fails, return 0
    if (img->infile == NULL) {
        fprintf(stderr, "jpegwrap_read: cannot open %s\n", filename);
        return 0;
    }

    //use libjpeg error messages
    img->cinfo.err = jpeg_std_error(&img->jerr);
    //initialize decompress struct
    jpeg_create_decompress(&img->cinfo);
    //jpeglib takes bytes from the image using stdio
    jpeg_stdio_src(&img->cinfo, img->infile);

    //saves jpeg comments (JPEG_COM), and markers up to 0xFFFF bytes
    jpeg_save_markers(&img->cinfo, JPEG_COM, 0xFFFF);

    //parses jpeg header data, require_image = TRUE
    (void) jpeg_read_header(&img->cinfo, TRUE);

    //uses libjpeg jpeg_read_coefficients(), reads all the coefficients from the image into img->coef_arrays
    img->coef_arrays = jpeg_read_coefficients(&img->cinfo);
    //if reading somehow fails (img->coef_arrays empty), return 0, throw error, fclose file
    if (img->coef_arrays == NULL) {
        fprintf(stderr, "jpegwrap_read: jpeg_read_coefficients failed\n");
        //dealloc decompress struct
        jpeg_destroy_decompress(&img->cinfo);
        fclose(img->infile);
        img->infile = NULL;
        return 0;
    }

    //get num components for convenience 
    img->num_components = img->cinfo.num_components;
    img->is_open = 1;
    return 1;
}

//gets width and height of the image in 8x8 blocks
//component arg nessecary in case chroma subsampling stores components at lower resolutions
JDIMENSION jpegwrap_blocks_wide(JpegImage *img, int component) {
    return img->cinfo.comp_info[component].width_in_blocks;
}
JDIMENSION jpegwrap_blocks_high(JpegImage *img, int component) {
    return img->cinfo.comp_info[component].height_in_blocks;
}

//get access to a single row of blocks starting at block_row
//writable arg is nessecary so changes are flushed out
static JBLOCKROW get_block_row(JpegImage *img, int component, JDIMENSION block_row, boolean writable) {
    JBLOCKARRAY buffer;

    buffer = (img->cinfo.mem->access_virt_barray)((j_common_ptr) &img->cinfo, img->coef_arrays[component], block_row, (JDIMENSION) 1, writable);

    return buffer[0];
}

//checks if a coordinate is correct
static int check_coords(JpegImage *img, int component, JDIMENSION block_row, JDIMENSION block_col) {
    //returns 0 if: image couldnt be opened, component out of range, row/col past the block grid, else returns 1
    if (!img->is_open) return 0;
    if (component < 0 || component >= img->num_components) return 0;
    if (block_row >= jpegwrap_blocks_high(img, component)) return 0;
    if (block_col >= jpegwrap_blocks_wide(img, component)) return 0;
    return 1;
}

//gets a specific block, reorders the coefficients to zigzag, returns 1 on success
int jpegwrap_get_block(JpegImage *img, int component, JDIMENSION block_row, JDIMENSION block_col, ZigzagBlock out_block) {
    JBLOCKROW row;
    JCOEFPTR block;
    int k;

    //if coord check fails, return 0
    if (!check_coords(img, component, block_row, block_col)) return 0;

    row = get_block_row(img, component, block_row, FALSE);
    block = row[block_col];

    //converts natural block order (libjpeg) to zigzag
    for (k = 0; k < DCTSIZE2; k++)
        out_block[k] = block[zigzag_to_natural[k]];

    return 1;
}

//reverse of jpegwrap_get_block
int jpegwrap_set_block(JpegImage *img, int component, JDIMENSION block_row, JDIMENSION block_col, const ZigzagBlock in_block) {
    JBLOCKROW row;
    JCOEFPTR block;
    int k;

    //if coord check fails, return 0
    if (!check_coords(img, component, block_row, block_col)) return 0;

    row = get_block_row(img, component, block_row, TRUE);
    block = row[block_col];

    //converts zigzag to natural order (libjpeg)
    for (k = 0; k < DCTSIZE2; k++)
        block[zigzag_to_natural[k]] = in_block[k];

    return 1;
}

//get specific coefficient in an 8x8 block
int jpegwrap_get_coef(JpegImage *img, int component, JDIMENSION block_row, JDIMENSION block_col, int zigzag_index, JCOEF *out_value) {
    JBLOCKROW row;

    // if coord check fails, or if index out of bounds, return 0
    if (!check_coords(img, component, block_row, block_col)) return 0;
    if (zigzag_index < 0 || zigzag_index >= DCTSIZE2) return 0;

    row = get_block_row(img, component, block_row, FALSE);
    *out_value = row[block_col][zigzag_to_natural[zigzag_index]];
    return 1;
}

//reverse of jpegwrap_get_coeff, writable is set to TRUE for get_block_row
int jpegwrap_set_coef(JpegImage *img, int component, JDIMENSION block_row, JDIMENSION block_col, int zigzag_index, JCOEF value) {
    JBLOCKROW row;

    // if coord check fails, or if index out of bounds, return 0
    if (!check_coords(img, component, block_row, block_col)) return 0;
    if (zigzag_index < 0 || zigzag_index >= DCTSIZE2) return 0;

    row = get_block_row(img, component, block_row, TRUE);
    row[block_col][zigzag_to_natural[zigzag_index]] = value;
    return 1;
}

//write all coefficients out to a new jpeg file ONCE, make all edits and then call the write command one time, it isnt a per-coefficient thing!
int jpegwrap_write(JpegImage *img, const char *filename) {
    struct jpeg_compress_struct dstinfo;
    struct jpeg_error_mgr jdsterr;
    FILE *outfile;

    //if file isnt registered as open, return 0
    if (!img->is_open) {
        fprintf(stderr, "jpegwrap_write: no image loaded\n");
        return 0;
    }

    //if file cant be opened for writing, return 0
    outfile = fopen(filename, "wb");
    if (outfile == NULL) {
        fprintf(stderr, "jpegwrap_write: cannot open %s\n", filename);
        return 0;
    }

    //use libjpeg error messages
    dstinfo.err = jpeg_std_error(&jdsterr);
    //initialize compress struct
    jpeg_create_compress(&dstinfo);
    //jpeglib writes bytes to the outfile using stdio
    jpeg_stdio_dest(&dstinfo, outfile);

    //copies important data not in DCT tables: image dimensions, color space, sampling factors, and quant tables
    jpeg_copy_critical_parameters(&img->cinfo, &dstinfo);

    //use optimal Huffman tables for the images coefficient stats, should get smaller files
    dstinfo.optimize_coding = TRUE;

    //do the actual writing part
    jpeg_write_coefficients(&dstinfo, img->coef_arrays);

    //flush output, free mem, close file
    jpeg_finish_compress(&dstinfo);
    jpeg_destroy_compress(&dstinfo);
    fclose(outfile);

    return 1;
}

//clean file close for libjpeg / stdio, only call this ONCE as well
void jpegwrap_close(JpegImage *img) {
    //safe "close" if the image failed to open; jpeg_destroy_decompress not called
    if (!img->is_open) {
        if (img->infile) {
            fclose(img->infile);
            img->infile = NULL;
        }
        return;
    }

    (void) jpeg_finish_decompress(&img->cinfo);
    jpeg_destroy_decompress(&img->cinfo);

    if (img->infile) {
        fclose(img->infile);
        img->infile = NULL;
    }

    //important
    img->is_open = 0;
}