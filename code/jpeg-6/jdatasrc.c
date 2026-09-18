/*
 * jdatasrc.c
 *
 * Copyright (C) 1994, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * Checked memory and stdio sources. The memory API always receives its
 * complete input length; both sources synthesize EOI only at actual EOF.
 */
#include "jinclude.h"
#include "jpeglib.h"
#include "jerror.h"

#define INPUT_BUF_SIZE 4096

typedef struct {
  struct jpeg_source_mgr pub;
  FILE *file;
  const JOCTET *data;
  size_t remaining;
  boolean memory, start_of_file, synthetic_eoi;
  JOCTET buffer[INPUT_BUF_SIZE];
} checked_source_mgr;

/** Initialize per-image EOF state without changing the caller's source span. */
METHODDEF void init_source(j_decompress_ptr cinfo)
{
  checked_source_mgr *src = (checked_source_mgr *)cinfo->src;
  src->start_of_file = TRUE;
  src->synthetic_eoi = FALSE;
}

/** Refill from complete remaining bytes and insert a single EOI only at true EOF. */
METHODDEF boolean fill_input_buffer(j_decompress_ptr cinfo)
{
  checked_source_mgr *src = (checked_source_mgr *)cinfo->src;
  size_t count;
  if (src->memory) {
    count = src->remaining < INPUT_BUF_SIZE ? src->remaining : INPUT_BUF_SIZE;
    if (count) { MEMCOPY(src->buffer, src->data, count); src->data += count; src->remaining -= count; }
  } else {
    count = JFREAD(src->file, src->buffer, INPUT_BUF_SIZE);
    if (ferror(src->file)) ERREXIT(cinfo, JERR_FILE_READ);
  }
  if (!count) {
    if (src->start_of_file) ERREXIT(cinfo, JERR_INPUT_EMPTY);
    if (src->synthetic_eoi) ERREXIT(cinfo, JERR_INPUT_EOF);
    src->synthetic_eoi = TRUE;
    WARNMS(cinfo, JWRN_JPEG_EOF);
    src->buffer[0] = 0xff; src->buffer[1] = JPEG_EOI; count = 2;
  }
  src->pub.next_input_byte = src->buffer;
  src->pub.bytes_in_buffer = count;
  src->start_of_file = FALSE;
  return TRUE;
}

/** Skip only real input; a truncated marker payload cannot consume synthesized EOI. */
METHODDEF void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
  checked_source_mgr *src = (checked_source_mgr *)cinfo->src;
  if (num_bytes <= 0) return;
  if (src->synthetic_eoi) ERREXIT(cinfo, JERR_INPUT_EOF);
  while ((size_t)num_bytes > src->pub.bytes_in_buffer) {
    num_bytes -= (long)src->pub.bytes_in_buffer;
    if (src->synthetic_eoi || (src->memory && !src->remaining)) ERREXIT(cinfo, JERR_INPUT_EOF);
    (void)fill_input_buffer(cinfo);
    if (src->synthetic_eoi) ERREXIT(cinfo, JERR_INPUT_EOF);
  }
  src->pub.next_input_byte += (size_t)num_bytes;
  src->pub.bytes_in_buffer -= (size_t)num_bytes;
}

/** Keep stream/file ownership with the caller, including after libjpeg aborts. */
METHODDEF void term_source(j_decompress_ptr cinfo) { (void)cinfo; }

/** Allocate a fresh manager so switching source APIs never casts an incompatible object. */
LOCAL checked_source_mgr *prepare_source(j_decompress_ptr cinfo)
{
  checked_source_mgr *src = (checked_source_mgr *)(*cinfo->mem->alloc_small)
    ((j_common_ptr)cinfo, JPOOL_PERMANENT, SIZEOF(checked_source_mgr));
  src->pub.init_source = init_source;
  src->pub.fill_input_buffer = fill_input_buffer;
  src->pub.skip_input_data = skip_input_data;
  src->pub.resync_to_restart = jpeg_resync_to_restart;
  src->pub.term_source = term_source;
  src->pub.next_input_byte = NULL; src->pub.bytes_in_buffer = 0;
  src->start_of_file = TRUE; src->synthetic_eoi = FALSE;
  cinfo->src = &src->pub;
  return src;
}

/** Supply an explicit memory span; no refill ever assumes unprovided padding. */
GLOBAL void jpeg_mem_src(j_decompress_ptr cinfo, const unsigned char *data, size_t length)
{
  checked_source_mgr *src;
  if (!data && length) ERREXIT(cinfo, JERR_INPUT_EMPTY);
  src = prepare_source(cinfo);
  src->memory = TRUE; src->file = NULL; src->data = data; src->remaining = length;
}

/** Preserve the standard FILE API for bundled JPEG command-line callers. */
GLOBAL void jpeg_stdio_src(j_decompress_ptr cinfo, FILE *file)
{
  checked_source_mgr *src;
  if (!file) ERREXIT(cinfo, JERR_INPUT_EMPTY);
  src = prepare_source(cinfo);
  src->memory = FALSE; src->file = file; src->data = NULL; src->remaining = 0;
}
