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

#define LINT_BUFFER_SIZE		PR_TUNABLE_BUFFER_SIZE * 2

extern module *static_modules[];
extern module *loaded_modules;

/* These are from src/dirtree.c */
extern xaset_t *server_list;
extern unsigned long ServerMaxInstances;
extern char ServerType;
extern int ServerUseReverseDNS;
extern int SocketBindTight;
extern int tcpBackLog;

module lint_module;

static int lint_engine = TRUE;
static pool *lint_pool = NULL;

struct lint_buffered_line {
  const char *text;
  size_t textsz;
};

struct lint_parsed_line {
  struct lint_parsed_line *next, *prev;
  const char *directive;
  const char *text;
  const char *source_file;
  unsigned int source_lineno;
};

static xaset_t *parsed_lines = NULL;

static const char *trace_channel = "lint";

static int lint_add_config_set(pool *p, array_header *bl, xaset_t *set,
  char *indent);

/* XXX TODO:
 *
 *  In order of parsed lines (thus track this), AND given merged config
 *  tree, emit full config file.
 *
 *  Watch for modules (like mod_sql), whose config_recs don't necessarily
 *  match the text name; some register multiple config_recs!
 *
 *  need to find which parsed_line entries don't have matching config_recs!
 *  need to find which config_recs don't have matching parsed_line entries!
 *
 * What about Defines?
 */

static struct lint_parsed_line *lint_find_parsed_line(const char *directive) {
  struct lint_parsed_line *parsed_line;

  if (parsed_lines == NULL) {
    return NULL;
  }

  for (parsed_line = (struct lint_parsed_line *) parsed_lines->xas_list;
       parsed_line != NULL;
       parsed_line = parsed_line->next) {
    if (strcmp(parsed_line->directive, directive) == 0) {
      return parsed_line;
    }
  }

  return NULL;
}

static int lint_write_text(pr_fh_t *fh, const char *text, size_t textsz) {
  int res, xerrno;

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

static int lint_write_msg(pr_fh_t *fh, const char *fmt, va_list msg) {
  char buf[LINT_BUFFER_SIZE];
  size_t buflen;

  buflen = pr_vsnprintf(buf, sizeof(buf)-1, fmt, msg);

  /* Always make sure the buffer is NUL-terminated. */
  buf[sizeof(buf)-1] = '\0';

  return lint_write_text(fh, buf, buflen);
}

static int lint_write_fmt(pr_fh_t *fh, const char *fmt, ...) {
  int res, xerrno;
  va_list msg;

  va_start(msg, fmt);
  res = lint_write_msg(fh, fmt, msg);
  xerrno = errno;
  va_end(msg);

  errno = xerrno;
  return res;
}

static int lint_add_msg(pool *p, array_header *buffered_lines,
    const char *fmt, va_list msg) {
  char buf[LINT_BUFFER_SIZE];
  size_t buflen;
  struct lint_buffered_line *bl;

  buflen = pr_vsnprintf(buf, sizeof(buf)-1, fmt, msg);

  /* Always make sure the buffer is NUL-terminated. */
  buf[sizeof(buf)-1] = '\0';

  bl = pcalloc(p, sizeof(struct lint_buffered_line));
  bl->text = pstrdup(p, buf);
  bl->textsz = buflen;

  *((struct lint_buffered_line **) push_array(buffered_lines)) = bl;
  return 0;
}

static int lint_add_fmt(pool *p, array_header *buffered_lines,
    const char *fmt, ...) {
  int res, xerrno;
  va_list msg;

  va_start(msg, fmt);
  res = lint_add_msg(p, buffered_lines, fmt, msg);
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

static int lint_write_buffered_lines(pr_fh_t *fh,
    array_header *buffered_lines) {
  register unsigned int i;

  if (buffered_lines == NULL) {
    return 0;
  }

  /* Sort the lines first */
  qsort((void *) buffered_lines->elts, buffered_lines->nelts,
    sizeof(struct lint_buffered_line *), buffered_linecmp);

  for (i = 0; i < buffered_lines->nelts; i++) {
    int res;
    struct lint_buffered_line *bl;

    bl = ((struct lint_buffered_line **) buffered_lines->elts)[i];
    res = lint_write_text(fh, bl->text, bl->textsz);
    if (res < 0) {
      return -1;
    }
  }

  return 0;
}

static int lint_write_header(pool *p, pr_fh_t *fh) {
  int res;
  pool *tmp_pool;
  const char *text;
  size_t textsz;
  struct tm *tm;
  time_t now;

  tmp_pool = make_sub_pool(p);

  now = time(NULL);
  tm = pr_gmtime(tmp_pool, &now);
  if (tm != NULL) {
    char ts[128];
    const char *time_fmt = "%Y-%m-%d %H:%M:%S %z";

    strftime(ts, sizeof(ts)-1, time_fmt, tm);
    text = pstrcat(tmp_pool, "# AUTO-GENERATED BY ", MOD_LINT_VERSION, " on ",
      ts, "\n", NULL);

  } else {
    text = pstrcat(tmp_pool, "# AUTO-GENERATED BY ", MOD_LINT_VERSION, "\n",
      NULL);
  }

  textsz = strlen(text);

  res = lint_write_text(fh, text, textsz);
  destroy_pool(tmp_pool);
  return res;
}

#if defined(PR_SHARED_MODULE)
static int is_static_module(module *m) {
  register unsigned int i;

  for (i = 0; static_modules[i] != NULL; i++) {
    module *sm;

    sm = static_modules[i];
    if (strcmp(m->name, sm->name) == 0) {
      return TRUE;
    }
  }

  return FALSE;
}
#endif /* PR_SHARED_MODULE */

static int lint_add_config_rec(pool *p, array_header *buffered_lines,
    config_rec *c, const char *indent) {
  int res = 0;
  struct lint_parsed_line *parsed_line;

  /* Skip directives that start with an underscore. */
  if (c->name != NULL &&
      *(c->name) == '_') {
    return 0;
  }

  /* XXX TODO: watch for c->config_type: CONF_DIR, CONF_ANON, CONF_VIRTUAL,
   *  etc.
   *
   * Start with CONF_PARAM.
   */

  switch (c->config_type) {
    /* TODO: Break these out into their own separate, specialized write
     * functions.
     */
    case CONF_PARAM: {
      const char *directive;

      directive = c->name;

      /* There are some directives whose config_rec names don't quite match;
       * handle them, too.
       */
      if (strcmp(c->name, "GroupID") == 0 ||
          strcmp(c->name, "UserID") == 0) {
        return 0;
      }

      if (strcmp(c->name, "GroupName") == 0) {
        directive = "Group";

      } else if (strcmp(c->name, "UserName") == 0) {
        directive = "User";
      }

      parsed_line = lint_find_parsed_line(directive);
      if (parsed_line != NULL) {
        res = lint_add_fmt(p, buffered_lines, "%s%s\n", indent,
          parsed_line->text);

      } else {
        pr_trace_msg(trace_channel, 1, "found no matching parsed line for %s",
          directive);
      }
      break;
    }

    case CONF_ROOT:
    case CONF_DIR:
    case CONF_ANON:
    case CONF_LIMIT:
    case CONF_VIRTUAL:
    case CONF_GLOBAL: /* We don't expect to see this */
    case CONF_CLASS:
    default:
      pr_trace_msg(trace_channel, 7, "config type %d not implemented yet",
        c->config_type);
      return 0;
  }

  return res;
}

static int lint_add_config_set(pool *p, array_header *buffered_lines,
    xaset_t *set, char *indent) {
  int res;
  config_rec *c;

  if (set->xas_list == NULL) {
    return 0;
  }

  if (indent == NULL) {
    indent = "";
  }

  for (c = (config_rec *) set->xas_list; c; c = c->next) {
    pr_signals_handle();

    res = lint_add_config_rec(p, buffered_lines, c, indent);
    if (res < 0) {
      return -1;
    }

    if (c->subset != NULL) {
      pool *iter_pool;

      iter_pool = make_sub_pool(p);
      res = lint_add_config_set(p, buffered_lines, c->subset,
        pstrcat(iter_pool, indent, "  ", NULL));
      destroy_pool(iter_pool);

      if (res < 0) {
        return -1;
      }
    }
  }

  return 0;
}

static int lint_add_server_rec(pool *p, array_header *buffered_lines,
    server_rec *s) {
  int res;

  if (s->conf == NULL ||
      s->conf->xas_list == NULL) {
    return 0;
  }

  res = lint_add_config_set(p, buffered_lines, s->conf, NULL);
  if (res < 0) {
    return -1;
  }

  return 0;
}

static int lint_write_modules(pool *p, pr_fh_t *fh) {
#if defined(PR_SHARED_MODULE)
  int res;
  module *m;

  /* XXX TODO Scan recorded parsed_lines for ModulePath et al */

  res = lint_write_fmt(fh, "%s", "\n# Modules\n\n<IfModule mod_dso.c>\n");
  if (res < 0) {
    return -1;
  }

  for (m = loaded_modules; m; m = m->next) {
    pr_signals_handle();

    /* Skip if this module is a static module. */
    if (is_static_module(m) == TRUE) {
      continue;
    }

    res = lint_write_fmt(fh, "  LoadModule mod_%s.c\n", m->name);
    if (res < 0) {
      return -1;
    }
  }

  res = lint_write_fmt(fh, "%s", "<IfModule>\n");
  if (res < 0) {
    return -1;
  }
#endif /* PR_SHARED_MODULE */

  return 0;
}

static int lint_write_server_config(pool *p, pr_fh_t *fh) {
  int res;
  pool *ctx_pool;
  struct lint_parsed_line *parsed_line;
  array_header *buffered_lines;

  res = lint_write_fmt(fh, "%s", "\n# Server Config\n\n");
  if (res < 0) {
    return -1;
  }

  /* Store our lines in a buffer for sorting, for emitting normalized
   * configs.
   */
  ctx_pool = make_sub_pool(p);
  pr_pool_tag(ctx_pool, "Lint server context pool");
  buffered_lines = make_array(ctx_pool, 10,
    sizeof(struct lint_buffered_line *));

  res = lint_add_fmt(ctx_pool, buffered_lines, "DefaultAddress %s\n",
    main_server->ServerAddress);
  if (res < 0) {
    destroy_pool(ctx_pool);
    return -1;
  }

  /* MaxConnectionRate changes variables that are scoped to mod_core only. */
  parsed_line = lint_find_parsed_line("MaxConnectionRate");
  if (parsed_line != NULL) {
    res = lint_add_fmt(ctx_pool, buffered_lines, "%s\n", parsed_line->text);
    if (res < 0) {
      destroy_pool(ctx_pool);
      return -1;
    }
  }

  if (ServerMaxInstances > 0) {
    res = lint_add_fmt(ctx_pool, buffered_lines, "MaxInstances %lu\n",
      ServerMaxInstances);
    if (res < 0) {
      destroy_pool(ctx_pool);
      return -1;
    }
  }

  res = lint_add_fmt(ctx_pool, buffered_lines, "PidFile %s\n",
    pr_pidfile_get());
  if (res < 0) {
    destroy_pool(ctx_pool);
    return -1;
  }

  res = lint_add_fmt(ctx_pool, buffered_lines, "Port %u\n",
    main_server->ServerPort);
  if (res < 0) {
    destroy_pool(ctx_pool);
    return -1;
  }

  res = lint_add_fmt(ctx_pool, buffered_lines, "ScoreboardFile %s\n",
    pr_get_scoreboard());
  if (res < 0) {
    destroy_pool(ctx_pool);
    return -1;
  }

  res = lint_add_fmt(ctx_pool, buffered_lines, "ScoreboardMutex %s\n",
    pr_get_scoreboard_mutex());
  if (res < 0) {
    destroy_pool(ctx_pool);
    return -1;
  }

  if (main_server->ServerAdmin != NULL) {
    res = lint_add_fmt(ctx_pool, buffered_lines, "ServerAdmin \"%s\"\n",
      main_server->ServerAdmin);
    if (res < 0) {
      destroy_pool(ctx_pool);
      return -1;
    }
  }

  if (main_server->ServerName != NULL) {
    res = lint_add_fmt(ctx_pool, buffered_lines, "ServerName \"%s\"\n",
      main_server->ServerName);
    if (res < 0) {
      destroy_pool(ctx_pool);
      return -1;
    }
  }

  res = lint_add_fmt(ctx_pool, buffered_lines, "ServerType %s\n",
    ServerType == SERVER_STANDALONE ? "standalone" : "inetd");
  if (res < 0) {
    destroy_pool(ctx_pool);
    return -1;
  }

  res = lint_add_fmt(ctx_pool, buffered_lines, "SocketBindTight %s\n",
    SocketBindTight ? "on" : "off");
  if (res < 0) {
    destroy_pool(ctx_pool);
    return -1;
  }

  parsed_line = lint_find_parsed_line("SocketOptions");
  if (parsed_line != NULL) {
    res = lint_add_fmt(ctx_pool, buffered_lines, "%s\n", parsed_line->text);
    if (res < 0) {
      destroy_pool(ctx_pool);
      return -1;
    }
  }

  res = lint_add_fmt(ctx_pool, buffered_lines, "TCPBacklog %d\n", tcpBackLog);
  if (res < 0) {
    destroy_pool(ctx_pool);
    return -1;
  }

  parsed_line = lint_find_parsed_line("TraceLog");
  if (parsed_line != NULL) {
    res = lint_add_fmt(ctx_pool, buffered_lines, "%s\n", parsed_line->text);
    if (res < 0) {
      destroy_pool(ctx_pool);
      return -1;
    }
  }

  parsed_line = lint_find_parsed_line("Trace");
  if (parsed_line != NULL) {
    res = lint_add_fmt(ctx_pool, buffered_lines, "%s\n", parsed_line->text);
    if (res < 0) {
      destroy_pool(ctx_pool);
      return -1;
    }
  }

  parsed_line = lint_find_parsed_line("TraceOptions");
  if (parsed_line != NULL) {
    res = lint_add_fmt(ctx_pool, buffered_lines, "%s\n", parsed_line->text);
    if (res < 0) {
      destroy_pool(ctx_pool);
      return -1;
    }
  }

  res = lint_add_fmt(ctx_pool, buffered_lines, "UseIPv6 %s\n",
    pr_netaddr_use_ipv6() ? "on" : "off");
  if (res < 0) {
    destroy_pool(ctx_pool);
    return -1;
  }

  res = lint_add_fmt(ctx_pool, buffered_lines, "UseReverseDNS %s\n",
    ServerUseReverseDNS ? "on" : "off");
  if (res < 0) {
    destroy_pool(ctx_pool);
    return -1;
  }

  res = lint_add_server_rec(ctx_pool, buffered_lines, main_server);
  if (res < 0) {
    destroy_pool(ctx_pool);
    return -1;
  }

  res = lint_write_buffered_lines(fh, buffered_lines);
  if (res < 0) {
    destroy_pool(ctx_pool);
    return -1;
  }

  destroy_pool(ctx_pool);
  return 0;
}

static int lint_write_classes(pool *p, pr_fh_t *fh) {
  int res;
  const pr_class_t *cls;

  res = lint_write_fmt(fh, "%s", "\n# Classes\n");
  if (res < 0) {
    return -1;
  }

  cls = pr_class_get(NULL);
  if (cls == NULL) {
    return 0;
  }

  while (cls != NULL) {
    register unsigned int i;
    const pr_netacl_t **acls;

    pr_signals_handle();

    res = lint_write_fmt(fh, "\n<Class %s>\n", cls->cls_name);
    if (res < 0) {
      return -1;
    }

    /* Note: it is tempting to try to use pr_netacl_get_str(), but that
     * only returns a description, not the actual raw text needed to
     * re-create the given ACL.  And, worse, the definition for
     * struct pr_netacl_t is scoped to netacl.c, making it private.  Thus
     * this will require some core changes, a new NetACL API,
     * e.g. pr_netacl_to_text().
     *
     * Since there will be multiple From directives, using
     * lint_find_parsed_line() might not work well...could make it work!
     */

    acls = cls->cls_acls->elts;
    for (i = 0; i < cls->cls_acls->nelts; i++) {
      /* XXX TODO: Fix to use to_text() function once available. */
      res = lint_write_fmt(fh, "  # From %s\n", pr_netacl_get_str(p, acls[i]));
      if (res < 0) {
        return -1;
      }
    }

    res = lint_write_fmt(fh, "  Satisfy %s\n",
      cls->cls_satisfy == PR_CLASS_SATISFY_ANY ? "any" : "all");
    if (res < 0) {
      return -1;
    }

    res = lint_write_fmt(fh, "%s", "</Class>\n");
    if (res < 0) {
      return -1;
    }

    cls = pr_class_get(cls);
  }

  return 0;
}

static int lint_write_ctrls(pool *p, pr_fh_t *fh) {
#if defined(PR_USE_CTRLS)
  int res;

  /* ControlsLog, Socket, etc. */

  res = lint_write_fmt(fh, "%s", "\n# Controls\n\n");
  if (res < 0) {
    return -1;
  }
#endif /* PR_USE_CTRLS */

  return 0;
}

static int lint_write_vhosts(pool *p, pr_fh_t *fh) {
  int res;
  pool *ctx_pool;
  server_rec *s;
  array_header *buffered_lines;

  res = lint_write_fmt(fh, "%s", "\n# VirtualHosts\n");
  if (res < 0) {
    return -1;
  }

  ctx_pool = make_sub_pool(p);
  pr_pool_tag(ctx_pool, "Lint <VirtualHost> context pool");
  buffered_lines = make_array(ctx_pool, 10,
    sizeof(struct lint_buffered_line *));

  for (s = (server_rec *) server_list->xas_list; s; s = s->next) {
    pr_signals_handle();

    if (s == main_server) {
      /* We wrote out the main_server config earlier. */
      continue;
    }

    res = lint_add_server_rec(ctx_pool, buffered_lines, s);
    if (res < 0) {
      destroy_pool(ctx_pool);
      return -1;
    }
  }

  res = lint_write_buffered_lines(fh, buffered_lines);
  if (res < 0) {
    destroy_pool(ctx_pool);
    return -1;
  }

  destroy_pool(ctx_pool);
  return 0;
}

static int lint_write_config(pool *p, const char *path) {
  pr_fh_t *fh;
  int xerrno;

  /* TODO: Will we want/need root privs here? */

  fh = pr_fsio_open(path, O_CREAT|O_WRONLY|O_TRUNC);
  xerrno = errno;
  if (fh == NULL) {
    pr_trace_msg(trace_channel, 1, "error opening '%s': %s", path,
      strerror(xerrno));
    errno = xerrno;
    return -1;
  }

  if (lint_write_header(p, fh) < 0) {
    xerrno = errno;

    (void) pr_fsio_close(fh);
    errno = xerrno;
    return -1;
  }

  if (lint_write_modules(p, fh) < 0) {
    xerrno = errno;

    (void) pr_fsio_close(fh);
    errno = xerrno;
    return -1;
  }

  if (lint_write_server_config(p, fh) < 0) {
    xerrno = errno;

    (void) pr_fsio_close(fh);
    errno = xerrno;
    return -1;
  }

  if (lint_write_classes(p, fh) < 0) {
    xerrno = errno;

    (void) pr_fsio_close(fh);
    errno = xerrno;
    return -1;
  }

  if (lint_write_ctrls(p, fh) < 0) {
    xerrno = errno;

    (void) pr_fsio_close(fh);
    errno = xerrno;
    return -1;
  }

  if (lint_write_vhosts(p, fh) < 0) {
    xerrno = errno;

    (void) pr_fsio_close(fh);
    errno = xerrno;
    return -1;
  }

  if (pr_fsio_close(fh) < 0) {
    xerrno = errno;

    pr_trace_msg(trace_channel, 1, "error writing '%s': %s", path,
      strerror(xerrno));
    errno = xerrno;
    return -1;
  }

  return 0;
}

/* Configuration handlers
 */

/* usage: LintConfigFile path */
MODRET set_lintconfigfile(cmd_rec *cmd) {
  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT);

  if (pr_fs_valid_path(cmd->argv[1]) < 0) {
    CONF_ERROR(cmd, "must be an absolute path");
  }

  add_config_param_str(cmd->argv[0], 1, cmd->argv[1]);
  return PR_HANDLED(cmd);
}

/* usage: LintEngine on|off */
MODRET set_lintengine(cmd_rec *cmd) {
  int engine = -1;
  config_rec *c = NULL;

  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT);

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
  const pr_parsed_line_t *parsed_data;
  struct lint_parsed_line *parsed_line;
  const char *text;

  parsed_data = event_data;
  pr_trace_msg(trace_channel, 7, "%s # %s:%u", parsed_data->text,
    parsed_data->source_file, parsed_data->source_lineno);

  if (lint_pool == NULL) {
    lint_pool = make_sub_pool(permanent_pool);
    pr_pool_tag(lint_pool, MOD_LINT_VERSION);
  }

  /* Keep a list of these text lines, in order of appearance.  First phase
   * is simply to write them back out, in order, generating the single
   * config file.
   */
  if (parsed_lines == NULL) {
    parsed_lines = xaset_create(lint_pool, NULL);
  }

  parsed_line = pcalloc(lint_pool, sizeof(struct lint_parsed_line));
  parsed_line->directive = pstrdup(lint_pool, parsed_data->cmd->argv[0]);

  text = parsed_data->text;

  /* Skip past any leading whitespace. */
  for (; *text && PR_ISSPACE(*text); text++) {
  }

  parsed_line->text = pstrdup(lint_pool, text);

  parsed_line->source_file = pstrdup(lint_pool, parsed_data->source_file);
  parsed_line->source_lineno = parsed_data->source_lineno;

  xaset_insert_end(parsed_lines, (xasetmember_t *) parsed_line);
}

static void lint_postparse_ev(const void *event_data, void *user_data) {
  int res;
  config_rec *c;

  /* At this point in time, we no longer care about parsed lines, as for
   * .ftpaccess files.
   */
  pr_event_unregister(&lint_module, "core.parsed-line", NULL);

  c = find_config(main_server->conf, CONF_PARAM, "LintEngine", FALSE);
  if (c != NULL) {
    lint_engine = *((int *) c->argv[0]);
    if (lint_engine == FALSE) {
      destroy_pool(lint_pool);
      lint_pool = NULL;
      parsed_lines = NULL;

      return;
    }
  }

  /* At this point in time, the config tree should be usable; servers have
   * been fixed up, etc.
   */

  c = find_config(main_server->conf, CONF_PARAM, "LintConfigFile", FALSE);
  if (c == NULL) {
    pr_trace_msg(trace_channel, 1, "%s",
      "no LintConfigFile configured, skipping");
    destroy_pool(lint_pool);
    lint_pool = NULL;
    parsed_lines = NULL;

    return;
  }

  res = lint_write_config(lint_pool, c->argv[0]);
  if (res < 0) {
    pr_trace_msg(trace_channel, 1, "failed to emit config file to '%s': %s",
      c->argv[0], strerror(errno));
  }

  /* Once we're done, we can destroy our pool; no need to keep it lingering
   * around.
   */
  destroy_pool(lint_pool);
  lint_pool = NULL;
  parsed_lines = NULL;
}

static void lint_restart_ev(const void *event_data, void *user_data) {
  /* Re-register our interest in parsed line events, now that the
   * (possibly changed) configuration will be re-read.
   */
  pr_event_register(&lint_module, "core.parsed-line", lint_parsed_line_ev,
    NULL);
  lint_engine = TRUE;
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
  pr_event_register(&lint_module, "core.postparse", lint_postparse_ev, NULL);
  pr_event_register(&lint_module, "core.restart", lint_restart_ev, NULL);

  return 0;
}

/* Module API tables
 */

static conftable lint_conftab[] = {
  { "LintConfigFile",		set_lintconfigfile, NULL },
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
