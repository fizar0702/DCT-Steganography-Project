#ifndef STEGO_ANALYZE_H
#define STEGO_ANALYZE_H

/*
 * Compare an original JPEG with a stego JPEG using basic DCT statistics.
 * Writes a human-readable report and two CSV histogram files.
 * Returns 0 on success and -1 on failure.
 */
int stego_analyze(const char *original_path,
                  const char *stego_path,
                  const char *report_path);

#endif
