/*
 * ProFTPD - mod_lint mod_core cop
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
#include "lint/cop.h"

static const char *get_directive(pool *p, config_rec *c) {
  if (strcmp(c->name, "GroupID") == 0 ||
      strcmp(c->name, "UserID") == 0) {
    errno = ENOENT;
    return NULL;
  }

  if (strcmp(c->name, "GroupName") == 0) {
    return "Group";
  }

  if (strcmp(c->name, "UserName") == 0) {
    return "User";
  }

  return c->name;
}

struct lint_cop core_cop = {
  "core",	NULL,	get_directive
};

struct lint_cop *lint_cop_get_core_cop(void) {
  return &core_cop;
}
