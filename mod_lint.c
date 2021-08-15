/*
 * ProFTPD: mod_lint -- a module for linting configurations
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
 *
 * This is mod_lint, contrib software for proftpd 1.3.x and above.
 * For more information contact TJ Saunders <tj@castaglia.org>.
 */

#include "conf.h"

#define MOD_LINT_VERSION		"mod_lint/0.0"

/* Make sure the version of proftpd is as necessary. */
#if PROFTPD_VERSION_NUMBER < 0x0001030802
# error "ProFTPD 1.3.8rc2 or later required"
#endif

module lint_module;

static int lint_engine = TRUE;
static pool *lint_pool = NULL;

static const char *trace_channel = "lint";

/* Configuration handlers
 */

/* usage: LintEngine on|off */
MODRET set_lintengine(cmd_rec *cmd) {
  int engine = -1;
  config_rec *c = NULL;

  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  engine = get_boolean(cmd, 1);
  if (engine == -1) {
    CONF_ERROR(cmd, "expected Boolean parameter");
  }

  c = add_config_param(cmd->argv[0], 1, NULL);
  c->argv[0] = pcalloc(c->pool, sizeof(int));
  *((int *) c->argv[0]) = engine;

  return PR_HANDLED(cmd);
}

/* Event listeners
 */

#if defined(PR_SHARED_MODULE)
static void lint_mod_unload_ev(const void *event_data, void *user_data) {
  if (strcmp((const char *) event_data, "mod_lint.c") != 0) {
    return;
  }

  /* Unregister ourselves from all events. */
  pr_event_unregister(&lint_module, NULL, NULL);

  destroy_pool(lint_pool);
  lint_pool = NULL;
}
#endif /* PR_SHARED_MODULE */

static void lint_parsed_line_ev(const void *event_data, void *user_data) {
  const pr_parsed_line_t *parsed_line;

  parsed_line = event_data;
  pr_trace_msg(trace_channel, 7, "%s # %s:%u", parsed_line->text,
    parsed_line->source_file, parsed_line->source_lineno);
}

static void lint_restart_ev(const void *event_data, void *user_data) {
}

/* Initialization functions
 */

static int lint_init(void) {
  lint_pool = make_sub_pool(permanent_pool);
  pr_pool_tag(lint_pool, MOD_LINT_VERSION);

#if defined(PR_SHARED_MODULE)
  pr_event_register(&lint_module, "core.module-unload", lint_mod_unload_ev,
    NULL);
#endif /* PR_SHARED_MODULE */
  pr_event_register(&lint_module, "core.parsed-line", lint_parsed_line_ev,
    NULL);
  pr_event_register(&lint_module, "core.restart", lint_restart_ev, NULL);

  return 0;
}

/* Module API tables
 */

static conftable lint_conftab[] = {
  { "LintEngine",		set_lintengine,	NULL },
  { NULL }
};

module lint_module = {
  NULL, NULL,

  /* Module API version 2.0 */
  0x20,

  /* Module name */
  "lint",

  /* Module configuration handler table */
  lint_conftab,

  /* Module command handler table */
  NULL,

  /* Module authentication handler table */
  NULL,

  /* Module initialization function */
  lint_init,

  /* Session initialization function */
  NULL,

  /* Module version */
  MOD_LINT_VERSION
};
