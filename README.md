To compile, use "make" in the same directory as the makefile. After, the "stego" executable should be created.
The program was made for, and tested on Linux

The following is the usage command, printed to the terminal after executing "./stego" with no arguments:

Usage:
  ./stego encode <cover.jpg> <message_file> <output.jpg> [options]
  ./stego decode <stego.jpg> <output_file>               [options]

Options (every flag used at encode time must also be passed at decode time):
  -c N   Max AC coefficients used per 8x8 block   (default: all)
  -m N   Minimum coefficient |magnitude|           (default: 1)
  -s N   Zigzag start index 0-63                   (default: 1, skips DC)
  -p     Invert parity: bit 1 maps to odd          (default: bit 1 maps to even)

Examples:
  ./stego encode cover.jpg secret.txt stego.jpg
  ./stego decode stego.jpg recovered.txt
  ./stego encode cover.jpg secret.txt stego.jpg -c 3 -m 2 -s 1
  ./stego decode stego.jpg recovered.txt -c 3 -m 2 -s 1


src/ contains C files written for this project
libjpeg/ contains libjpeg, the library used to interface with JPEGs for this project (https://github.com/kornelski/libjpeg#)
samples/ contains sample images that were used to test the effectiveness of the tool
(I like the toucan one the most)

STATISTICAL ANALYSIS
--------------------
Compare an original JPEG with a stego JPEG:

  ./stego analyze original.jpg stego.jpg analysis.txt

This creates:
  analysis.txt                         Human-readable statistics
  analysis_original_histogram.csv      Original DCT histogram
  analysis_stego_histogram.csv         Stego DCT histogram

The report includes the changed coefficient percentage, mean absolute
difference, root mean square difference, zero coefficient percentage,

## Team Project

This project was developed collaboratively as part of a university Computer Science course.

**My Contributions:**
- Developed the 
**Team Contributions:**
- Project planning and development
- Implementation and testing
- Troubleshooting and documentation

**Acknowledgments:**
Developed with my project teammates as part of our coursework at the University of Texas at San Antonio.



even/odd AC coefficient ratios, and two chi-square indicators. The CSV
files can be opened in Excel or Google Sheets to create histogram charts.
