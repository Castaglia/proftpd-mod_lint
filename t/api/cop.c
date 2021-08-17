/*
 * ProFTPD - mod_lint API testsuite
 * Copyright (c) 2021 TJ Saunders <tj@castaglia.org>
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

/* Cop API tests. */

#include "tests.h"
#include "lint/cop.h"

static pool *p = NULL;

static void set_up(void) {
  if (p == NULL) {
    p = permanent_pool = make_sub_pool(NULL);
  }

  if (getenv("TEST_VERBOSE") != NULL) {
    pr_trace_set_levels("lint.cop", 1, 20);
  }

  mark_point();
}

static void tear_down(void) {
  if (getenv("TEST_VERBOSE") != NULL) {
    pr_trace_set_levels("lint.cop", 0, 0);
  }

  if (p != NULL) {
    destroy_pool(p);
    p = permanent_pool = NULL;
  }
}

START_TEST (cop_get_config_cop_test) {
  const struct lint_cop *cop;
  config_rec *c;

  mark_point();
  cop = lint_cop_get_config_cop(NULL);
  fail_unless(cop == NULL, "Failed to handle null config");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  c = pcalloc(p, sizeof(config_rec));
  cop = lint_cop_get_config_cop(c);
  fail_unless(cop == NULL, "Failed to handle null config");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  c->name = pstrdup(p, "LintEngine");
  cop = lint_cop_get_config_cop(c);
  fail_unless(cop == NULL, "Failed to handle null config");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  c->config_type = CONF_PARAM;
  cop = lint_cop_get_config_cop(c);
  fail_unless(cop == NULL, "Failed to handle unknown config");
  fail_unless(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);
}
END_TEST

START_TEST (cop_get_module_cop_test) {
  const struct lint_cop *cop;

  mark_point();
  cop = lint_cop_get_module_cop(NULL);
  fail_unless(cop == NULL, "Failed to handle null module");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  cop = lint_cop_get_module_cop(&lint_module);
  fail_unless(cop != NULL, "Failed to handle lint module: %s", strerror(errno));
}
END_TEST

START_TEST (cop_get_directive_test) {
  const struct lint_cop *cop;
  const char *res;
  config_rec *c;

  mark_point();
  res = lint_cop_get_directive(NULL, NULL, NULL);
  fail_unless(res == NULL, "Failed to handle null cop");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  cop = lint_cop_get_module_cop(&lint_module);
  fail_unless(cop != NULL, "Failed to handle lint module: %s", strerror(errno));

  mark_point();
  res = lint_cop_get_directive(cop, NULL, NULL);
  fail_unless(res == NULL, "Failed to handle null pool");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  res = lint_cop_get_directive(cop, p, NULL);
  fail_unless(res == NULL, "Failed to handle null config");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  c = pcalloc(p, sizeof(config_rec));
  c->config_type = CONF_PARAM;
  c->name = "FooBar";
  res = lint_cop_get_directive(cop, p, c);
  fail_unless(res != NULL, "Failed to get directive: %s", strerror(errno));
  fail_unless(strcmp(res, c->name) == 0,
    "Expected '%s', got '%s'", c->name, res);
}
END_TEST

Suite *tests_get_cop_suite(void) {
  Suite *suite;
  TCase *testcase;

  suite = suite_create("cop");
  testcase = tcase_create("base");

  tcase_add_checked_fixture(testcase, set_up, tear_down);

  tcase_add_test(testcase, cop_get_config_cop_test);
  tcase_add_test(testcase, cop_get_module_cop_test);

  tcase_add_test(testcase, cop_get_directive_test);

  suite_add_tcase(suite, testcase);
  return suite;
}
