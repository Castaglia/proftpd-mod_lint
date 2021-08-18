/*
 * ProFTPD - mod_lint cop API
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

#ifndef MOD_LINT_COP_H
#define MOD_LINT_COP_H

#include "mod_lint.h"

struct lint_cop {
  const char *name;
  module *m;

  const char *(*get_directive)(pool *p, config_rec *c);
};

const struct lint_cop *lint_cop_get_config_cop(config_rec *c);
const struct lint_cop *lint_cop_get_module_cop(module *m);

/* Provides the directive name for a config_rec. */
const char *lint_cop_get_directive(const struct lint_cop *cop, pool *p,
  config_rec *c);

#endif /* MOD_LINT_COP_H */
