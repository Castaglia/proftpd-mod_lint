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

/* Text API tests. */

#include "tests.h"
#include "lint/text.h"

static pool *p = NULL;

static void set_up(void) {
  if (p == NULL) {
    p = permanent_pool = make_sub_pool(NULL);
  }

  if (getenv("TEST_VERBOSE") != NULL) {
    pr_trace_set_levels("lint.text", 1, 20);
  }

  mark_point();
}

static void tear_down(void) {
  if (getenv("TEST_VERBOSE") != NULL) {
    pr_trace_set_levels("lint.text", 0, 0);
  }

  if (p != NULL) {
    destroy_pool(p);
    p = permanent_pool = NULL;
  }
}

START_TEST (text_add_fmt_test) {
  int res;
  array_header *list;

  mark_point();
  res = lint_text_add_fmt(NULL, NULL, NULL);
  fail_unless(res < 0, "Failed to handle null pool");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  res = lint_text_add_fmt(p, NULL, NULL);
  fail_unless(res < 0, "Failed to handle null list");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  list = make_array(p, 0, sizeof(char *));
  res = lint_text_add_fmt(p, list, NULL);
  fail_unless(res < 0, "Failed to handle null fmt");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);
}
END_TEST

START_TEST (text_add_msg_test) {
  int res;
  array_header *list;
  va_list msg;

  mark_point();
  res = lint_text_add_msg(NULL, NULL, NULL, msg);
  fail_unless(res < 0, "Failed to handle null pool");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  res = lint_text_add_msg(p, NULL, NULL, msg);
  fail_unless(res < 0, "Failed to handle null list");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  list = make_array(p, 0, sizeof(char *));
  res = lint_text_add_msg(p, list, NULL, msg);
  fail_unless(res < 0, "Failed to handle null fmt");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);
}
END_TEST

START_TEST (text_write_fmt_test) {
  int res;
  pr_fh_t *fh;

  mark_point();
  res = lint_text_write_fmt(NULL, NULL);
  fail_unless(res < 0, "Failed to handle null fh");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  /* We don't need a real pr_fh_t for this test. */
  fh = (pr_fh_t *) 6;
  res = lint_text_write_fmt(6, NULL);
  fail_unless(res < 0, "Failed to handle null fmt");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);
}
END_TEST

START_TEST (text_write_msg_test) {
  int res;
  pr_fh_t *fh;
  va_list msg;

  mark_point();
  res = lint_text_write_msg(NULL, NULL, msg);
  fail_unless(res < 0, "Failed to handle null fh");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  /* We don't need a real pr_fh_t for this test. */
  fh = (pr_fh_t *) 7;
  res = lint_text_write_msg(fh, NULL, msg);
  fail_unless(res < 0, "Failed to handle null text");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);
}
END_TEST

START_TEST (text_write_text_test) {
  int res;
  pr_fh_t *fh;

  mark_point();
  res = lint_text_write_text(NULL, NULL, 0);
  fail_unless(res < 0, "Failed to handle null fh");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  /* We don't need a real pr_fh_t for this test. */
  fh = (pr_fh_t *) 8;
  res = lint_text_write_text(fh, NULL, 0);
  fail_unless(res < 0, "Failed to handle null text");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  res = lint_text_write_text(fh, "foobar", 0);
  fail_unless(res == 0, "Failed to handle zero text len: %s", strerror(errno));
}
END_TEST

START_TEST (text_write_buffered_lines_test) {
  int res;
  pr_fh_t *fh;

  mark_point();
  res = lint_text_write_buffered_lines(NULL, NULL);
  fail_unless(res < 0, "Failed to handle null fh");
  fail_unless(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  /* We don't need a real pr_fh_t for this test. */
  fh = (pr_fh_t *) 9;
  res = lint_text_write_buffered_lines(fh, NULL);
  fail_unless(res == 0, "Failed to handle null list: %s", strerror(errno));
}
END_TEST

Suite *tests_get_text_suite(void) {
  Suite *suite;
  TCase *testcase;

  suite = suite_create("text");
  testcase = tcase_create("base");

  tcase_add_checked_fixture(testcase, set_up, tear_down);

  tcase_add_test(testcase, text_add_fmt_test);
  tcase_add_test(testcase, text_add_msg_test);

  tcase_add_test(testcase, text_write_fmt_test);
  tcase_add_test(testcase, text_write_msg_test);
  tcase_add_test(testcase, text_write_text_test);

  tcase_add_test(testcase, text_write_buffered_lines_test);

  suite_add_tcase(suite, testcase);
  return suite;
}
