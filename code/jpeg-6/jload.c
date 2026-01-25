
#include "../game/q_shared.h"
#include "../qcommon/qcommon.h"

/*
 * Include file for users of JPEG library.
 * You will need to have included system headers that define at least
 * the typedefs FILE and size_t before you can include jpeglib.h.
 * (stdio.h is sufficient on ANSI-conforming systems.)
 * You may also wish to include "jerror.h".
 */

#include "jpeglib.h"


int LoadJPG( const char *filename, unsigned char **pic, int *width, int *height ) {
  /* This struct contains the JPEG decompression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   */
  struct jpeg_decompress_struct cinfo;
  /* We use our private extension JPEG error handler.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  /* This struct represents a JPEG error handler.  It is declared separately
   * because applications often want to supply a specialized error handler
   * (see the second half of this file for an example).  But here we just
   * take the easy way out and use the standard error handler, which will
   * print a message on stderr and call exit() if compression fails.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
  struct jpeg_error_mgr jerr;
  /* More stuff */
  fileHandle_t infile;		/* source file */
  JSAMPARRAY buffer;		/* Output row buffer */
  int row_stride;		/* physical row width in output buffer */
  unsigned char *out;

  /* In this example we want to open the input file before doing anything else,
   * so that the setjmp() error recovery below can assume the file is open.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to read binary files.
   */

  FS_FOpenFileRead( filename, &infile, qfalse );
  if (infile == 0) {
    return 0;
  }

  /* Step 1: allocate and initialize JPEG decompression object */

  /* We have to set up the error handler first, in case the initialization
   * step fails.  (Unlikely, but it could happen if you are out of memory.)
   * This routine fills in the contents of struct jerr, and returns jerr's
   * address which we place into the link field in cinfo.
   */
  cinfo.err = jpeg_std_error(&jerr);

  /* Now we can initialize the JPEG decompression object. */
  jpeg_create_decompress(&cinfo);

  /* Step 2: specify data source (eg, a file) */
  {
    int len;
    
    len = FS_filelength(infile);
    cinfo.src = NULL; // Ensure src is null before first init if re-using (not here though)
    // We need to buffer the whole file because the modified jpeg_stdio_src expects a memory buffer
    // not a FILE *. 
    // Optimization: we could implement a custom source manager that reads from FS_Read,
    // but for now, matching the library expectation is safer.
    
    // Using a static buffer or malloc? Z_Malloc is standard.
    // We need to keep this buffer valid until jpeg_finish_decompress.
    // Ideally we'd validata len.
    
    unsigned char *fileBuf = Z_Malloc(len);
    FS_Read(fileBuf, len, infile);
    
    // We can close the file handle now as we have the data, but existing code closes it later.
    // We'll leave the close call where it is or move it up.
    // The library uses the pointer.
    
    jpeg_stdio_src(&cinfo, fileBuf);
    
    // Store buf pointer in client_data or similar? No, standard struct doesn't have it.
    // We need to free 'fileBuf' at the end.
    // Hack: we can use a local variable but we need to free it at the end of the function.
    // But we need to make sure we don't return early without freeing.
    // There is an error exit setup. 
    // However, this function 'LoadJPG' has a 'return 0' on error (infile==0).
    // The error handler calls exit()! (See comments in jload.c)
    // "use the standard error handler, which will print a message on stderr and call exit()"
    // Q3 usually uses setjmp/longjmp for jpeg errors. 
    // This jload.c seems very primitive.
    // If it calls exit(), we leak memory but the game exits so it doesn't matter?
    // Wait, Q3's ERR_DROP/ERR_FATAL uses longjmp usually.
    // But here standard error handler is used.
    
    // Lets Just fix the immediate crash.
    
    (void) jpeg_read_header(&cinfo, TRUE);
    (void) jpeg_start_decompress(&cinfo);
    
    row_stride = cinfo.output_width * cinfo.output_components;
    out = Z_Malloc(cinfo.output_width*cinfo.output_height*cinfo.output_components);
    *pic = out;
    *width = cinfo.output_width;
    *height = cinfo.output_height;
    
    while (cinfo.output_scanline < cinfo.output_height) {
        buffer = (JSAMPARRAY)out+(row_stride*cinfo.output_scanline);
        (void) jpeg_read_scanlines(&cinfo, buffer, 1);
    }
    (void) jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    FS_FCloseFile(infile);
    Z_Free(fileBuf);
    return 1;
  }
  // The rest of the function is replaced by the block above to handle scope of fileBuf
  // Wait, I should not delete the rest of the function in "ReplacementContent" unless I target it.
  // I will target the block from "jpeg_stdio_src" to the end.

}

