/*
 * ProFTPD: mod_lint text implementation
 * Copyright (c) 2021 TJ Saunders
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 *
 * As a special exemption, TJ Saunders and other respective copyright holders
 * give permission to link this program with OpenSSL, and distribute the
 * resulting executable, without including the source code for OpenSSL in the
 * source distribution.
 */

#include "mod_lint.h"
#include "lint/text.h"

#define LINT_BUFFER_SIZE		PR_TUNABLE_BUFFER_SIZE * 2

static const char *trace_channel = "lint.text";

int lint_text_write_text(pr_fh_t *fh, const char *text, size_t textsz) {
  int res, xerrno;

  if (fh == NULL ||
      text == NULL) {
    errno = EINVAL;
    return -1;
  }

  if (textsz == 0) {
    return 0;
  }

  pr_trace_msg(trace_channel, 29, "writing text: '%s' (%lu)", text,
    (unsigned long) textsz);
  res = pr_fsio_write(fh, text, textsz);
  xerrno = errno;

  if (res < 0) {
    pr_trace_msg(trace_channel, 1, "error writing %lu bytes to '%s': %s",
      (unsigned long) textsz, fh->fh_path, strerror(xerrno));
  }

  errno = xerrno;
  return res;
}

int lint_text_write_msg(pr_fh_t *fh, const char *fmt, va_list msg) {
  char buf[LINT_BUFFER_SIZE];
  size_t buflen;

  if (fh == NULL ||
      fmt == NULL) {
    errno = EINVAL;
    return -1;
  }

  buflen = pr_vsnprintf(buf, sizeof(buf)-1, fmt, msg);

  /* Always make sure the buffer is NUL-terminated. */
  buf[sizeof(buf)-1] = '\0';

  return lint_text_write_text(fh, buf, buflen);
}

int lint_text_write_fmt(pr_fh_t *fh, const char *fmt, ...) {
  int res, xerrno;
  va_list msg;

  if (fh == NULL ||
      fmt == NULL) {
    errno = EINVAL;
    return -1;
  }

  va_start(msg, fmt);
  res = lint_text_write_msg(fh, fmt, msg);
  xerrno = errno;
  va_end(msg);

  errno = xerrno;
  return res;
}

int lint_text_add_msg(pool *p, array_header *buffered_lines, const char *fmt,
    va_list msg) {
  char buf[LINT_BUFFER_SIZE];
  size_t buflen;
  struct lint_buffered_line *bl;

  if (p == NULL ||
      buffered_lines == NULL ||
      fmt == NULL) {
    errno = EINVAL;
    return -1;
  }

  buflen = pr_vsnprintf(buf, sizeof(buf)-1, fmt, msg);

  /* Always make sure the buffer is NUL-terminated. */
  buf[sizeof(buf)-1] = '\0';

  bl = pcalloc(p, sizeof(struct lint_buffered_line));
  bl->text = pstrdup(p, buf);
  bl->textsz = buflen;

  *((struct lint_buffered_line **) push_array(buffered_lines)) = bl;
  return 0;
}

int lint_text_add_fmt(pool *p, array_header *buffered_lines, const char *fmt,
    ...) {
  int res, xerrno;
  va_list msg;

  if (p == NULL ||
      buffered_lines == NULL ||
      fmt == NULL) {
    errno = EINVAL;
    return -1;
  }

  va_start(msg, fmt);
  res = lint_text_add_msg(p, buffered_lines, fmt, msg);
  xerrno = errno;
  va_end(msg);

  errno = xerrno;
  return res;
}

static int buffered_linecmp(const void *a, const void *b) {
  const struct lint_buffered_line *bla, *blb;

  bla = *((struct lint_buffered_line **) a);
  blb = *((struct lint_buffered_line **) b);
  return strcmp(bla->text, blb->text);
}

int lint_text_write_buffered_lines(pr_fh_t *fh, array_header *buffered_lines) {
  register unsigned int i;

  if (buffered_lines == NULL) {
    return 0;
  }

  if (fh == NULL) {
    errno = EINVAL;
    return -1;
  }

  /* Sort the lines first */
  qsort((void *) buffered_lines->elts, buffered_lines->nelts,
    sizeof(struct lint_buffered_line *), buffered_linecmp);

  for (i = 0; i < buffered_lines->nelts; i++) {
    int res;
    struct lint_buffered_line *bl;

    bl = ((struct lint_buffered_line **) buffered_lines->elts)[i];
    res = lint_text_write_text(fh, bl->text, bl->textsz);
    if (res < 0) {
      return -1;
    }
  }

  return 0;
}
