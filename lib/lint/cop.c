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

#include "mod_lint.h"
#include "lint/cop.h"

/* Known cops. */
struct lint_cop *lint_cop_get_core_cop(void);
struct lint_cop *lint_cop_get_default_cop(void);

struct lint_cop_provider {
  const char *name;
  struct lint_cop *(*get_cop)(void);
};

static struct lint_cop_provider module_providers[] = {
  { "core",	lint_cop_get_core_cop },
  { NULL, 	NULL }
};

/* Here's where things get messy.  We need to maintain our own heuristic,
 * a list of known config_rec names and their corresponding modules.
 */
static struct lint_cop_provider config_providers[] = {
  { "GroupID",		lint_cop_get_core_cop },
  { "GroupName",	lint_cop_get_core_cop },
  { "UserID",		lint_cop_get_core_cop },
  { "UserName",		lint_cop_get_core_cop },
  { NULL, 		NULL }
};

const struct lint_cop *lint_cop_get_config_cop(config_rec *c) {
  register unsigned int i;
  conftable *conftab = NULL;
  int idx = -1;
  unsigned int hash = 0;

  /* NOTE: Watch out for non-CONF_PARAM configs. */

  if (c == NULL ||
      c->name == NULL ||
      c->config_type != CONF_PARAM) {
    errno = EINVAL;
    return NULL;
  }

  conftab = pr_stash_get_symbol2(PR_SYM_CONF, c->name, NULL, &idx, &hash);
  if (conftab != NULL) {
    return lint_cop_get_module_cop(conftab->m);
  }

  for (i = 0; config_providers[i].name != NULL; i++) {
    if (strcmp(c->name, config_providers[i].name) == 0) {
      return (*config_providers[i].get_cop)();
    }
  }

  errno = ENOENT;
  return NULL;
}

const struct lint_cop *lint_cop_get_module_cop(module *m) {
  register unsigned int i;
  struct lint_cop *cop = NULL;

  if (m == NULL) {
    errno = EINVAL;
    return NULL;
  }

  for (i = 0; module_providers[i].name != NULL; i++) {
    if (strcmp(m->name, module_providers[i].name) == 0) {
      cop = (*module_providers[i].get_cop)();
      break;
    }
  }

  if (cop == NULL) {
    cop = lint_cop_get_default_cop();
  }

  cop->m = m;
  return cop;
}

const char *lint_cop_get_directive(const struct lint_cop *cop, pool *p,
    config_rec *c) {
  if (cop == NULL ||
      p == NULL ||
      c == NULL) {
    errno = EINVAL;
    return NULL;
  }

  return (cop->get_directive)(p, c);
}
