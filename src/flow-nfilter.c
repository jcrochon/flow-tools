/*
 * Copyright (c) 2001 Mark Fullmer and The Ohio State University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $Id: flow-nfilter.c,v 1.7 2004/01/05 17:57:18 maf Exp $
 */

#include "ftconfig.h"
#include <ftlib.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#if HAVE_STRINGS_H
 #include <strings.h>
#endif

#if HAVE_STRING_H
  #include <string.h>
#endif


int debug;
void usage(void);

int main(int argc, char **argv)
{
  struct ftio ftio_in, ftio_out;
  struct ftprof ftp;
  struct ftfil ftfil;
  struct ftfil_def *ftfd;
  struct ftver ftv_in, ftv_out;
  struct ftset ftset;
  struct fts3rec_offsets fo;
  struct ftvar ftvar;
  char *rec;
  const char *fname, *dname;
  uint32_t total_flows, cap_start, cap_end;
  uint32_t time_start, time_end;
  int i, keep_input_time;

  /* init fterr */
  fterr_setid(argv[0]);

  bzero(&ftv_in, sizeof ftv_in);
  bzero(&ftv_out, sizeof ftv_out);
  bzero(&ftvar, sizeof ftvar);
  total_flows = 0;

  /* init var binding */
  if (ftvar_new(&ftvar) < 0)
    fterr_errx(1, "ftvar_new(): failed");

  fname = FT_PATH_CFG_FILTER;
  dname = "default";

  keep_input_time = 0;

  /* defaults + no compression */
  ftset_init(&ftset, 0);

  while ((i = getopt(argc, argv, "b:C:d:f:F:h?kv:z:")) != -1)

    switch (i) {

    case 'b': /* output byte order */
      if (!strcasecmp(optarg, "little"))
        ftset.byte_order = FT_HEADER_LITTLE_ENDIAN;
      else if (!strcasecmp(optarg, "big"))
        ftset.byte_order = FT_HEADER_BIG_ENDIAN;
      else
        fterr_errx(1, "expecting \"big\" or \"little\"");
      break;

    case 'C': /* comment field */
      ftset.comments = optarg;
      break;

    case 'd': /* debug */
      debug = atoi(optarg);
      break;

    case 'f': /* filter file name */
      fname = optarg;
      break;

    case 'F': /* filter definition name */
      dname = optarg;
      break;

    case 'k': /* keep the start/end time from the input */
      keep_input_time = 1;
      break;

    case 'v': /* variable */
      if (ftvar_pset(&ftvar, optarg) < 0)
        fterr_errx(1, "ftvar_pset(%s): failed", optarg);
      break;

    case 'z': /* compress level */
      ftset.z_level = atoi(optarg);
      if ((ftset.z_level < 0) || (ftset.z_level > 9))
        fterr_errx(1, "Compression level must be between 0 and 9");
      break;

    case 'h': /* help */
    case '?': 
    default:
      usage();
      exit (1);
      break;

    } /* switch */

  if (argc - optind)
    fterr_errx(1, "Extra arguments starting with %s.", argv[optind]);

  /* initialize and load filters */
  if (ftfil_load(&ftfil, &ftvar, fname))
    fterr_errx(1, "ftfil_load(): failed");

  if (!(ftfd = ftfil_def_find(&ftfil, dname))) 
    fterr_errx(1, "ftfil_def_find(): failed");

  /* input is stdin */
  if (ftio_init(&ftio_in, 0, FT_IO_FLAG_READ | FT_IO_FLAG_MMAP) < 0)
    fterr_errx(1, "ftio_init(): failed");

  /* output is stdout */
  if (ftio_init(&ftio_out, 1, FT_IO_FLAG_WRITE |
    ((ftset.z_level) ? FT_IO_FLAG_ZINIT : 0) ) < 0)
    fterr_errx(1, "ftio_init(): failed");

  /* preserve start/end time from input stream? */
  if (keep_input_time) {      
  
    time_start = ftio_get_cap_start(&ftio_in);
    time_end = ftio_get_cap_end(&ftio_in);
      
    if (time_start && time_end) {
  
      ftio_set_preloaded(&ftio_out, 1);
      ftio_set_cap_time(&ftio_out, time_start, time_end);
  
    }

  } /* keep_input_time */

  ftio_get_ver(&ftio_in, &ftv_in);

  if (ftfil_def_test_xfields(ftfd, ftrec_xfield(&ftv_in)))
    fterr_errx(1, "Filter references a field not in flow.");

  ftv_in.s_version = FT_IO_SVERSION;

  if (!ftv_out.set)
    bcopy(&ftv_in, &ftv_out, sizeof ftv_in);

  /* set the version information in the io stream */
  if (ftio_set_ver(&ftio_out, &ftv_out) < 0)
    fterr_errx(1, "ftio_set_ver(): failed");

  ftio_set_comment(&ftio_out, ftset.comments);
  ftio_set_byte_order(&ftio_out, ftset.byte_order);
  ftio_set_z_level(&ftio_out, ftset.z_level);
  ftio_set_streaming(&ftio_out, 1);
  ftio_set_debug(&ftio_out, debug);
  ftio_set_cap_time(&ftio_out, cap_start, cap_end);
  ftio_set_flows_count(&ftio_out, total_flows);

  if (ftio_write_header(&ftio_out) < 0)
    fterr_errx(1, "ftio_write_header(): failed");

  fts3rec_compute_offsets(&fo, &ftv_in);

  /* profile */
  ftprof_start (&ftp);

  while ((rec = ftio_read(&ftio_in))) {

    ++total_flows;

    if (ftfil_def_eval(ftfd, rec, &fo) == FT_FIL_MODE_DENY)
      continue;

    if (ftio_write(&ftio_out, rec) < 0)
      fterr_errx(1, "ftio_write(): failed");

  } /* while */

  if (ftio_close(&ftio_out) < 0)
    fterr_errx(1, "ftio_close(): failed");

  if (ftio_close(&ftio_in) < 0)
    fterr_errx(1, "ftio_close(): failed");

  ftfil_free(&ftfil);

  ftvar_free(&ftvar);

  if (debug > 0) {
    ftprof_end (&ftp, total_flows);
    ftprof_print(&ftp, argv[0], stderr);
  }

  return 0;

} /* main */

void usage(void) {
  fprintf(stderr, "Usage: flow-nfilter [hk] [-b big|little] [-C comment]\n");
  fprintf(stderr, "       [-d debug_level] [-f filter_fname] [-F filter_definition]\n");
  fprintf(stderr, "       [-z z_level]\n");
}

