/*
 * ProFTPD - mod_lint text API
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

#ifndef MOD_LINT_TEXT_H
#define MOD_LINT_TEXT_H

#include "mod_lint.h"

struct lint_buffered_line {
  const char *text;
  size_t textsz;
};

int lint_text_add_fmt(pool *p, array_header *buffered_lines, const char *fmt,
  ...);
int lint_text_add_msg(pool *p, array_header *buffered_lines, const char *fmt,
  va_list msg);

int lint_text_write_fmt(pr_fh_t *fh, const char *fmt, ...);
int lint_text_write_msg(pr_fh_t *fh, const char *fmt, va_list msg);
int lint_text_write_text(pr_fh_t *fh, const char *text, size_t textsz);

int lint_text_write_buffered_lines(pr_fh_t *fh, array_header *buffered_lines);

#endif /* MOD_LINT_TEXT_H */
