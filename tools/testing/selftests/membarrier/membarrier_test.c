#define _GNU_SOURCE
#include <linux/membarrier.h>
#include <syscall.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "../kselftest.h"

enum test_membarrier_status {
	TEST_MEMBARRIER_PASS = 0,
	TEST_MEMBARRIER_FAIL,
	TEST_MEMBARRIER_SKIP,
};

static int test_num = 0;

static int sys_membarrier(int cmd, int flags)
{
	return syscall(__NR_membarrier, cmd, flags);
}

static int test_result(const char *msg, int value)
{
	test_num++;
	printf("%s %d %s\n", value ? "not ok" : "ok", test_num, msg);
	return value;
}

static int skip_tests(void)
{
	printf("Bail out\n");
	printf("1..%d\n", test_num);
	return ksft_exit_skip();
}

static int bail_out(void)
{
	printf("Bail out!\n");
	printf("1..%d\n", test_num);
	return ksft_exit_fail();
}


static enum test_membarrier_status test_membarrier_cmd_fail(void)
{
	int cmd = -1, flags = 0;
	const char *test_name = "membarrier command fail";

	if (sys_membarrier(cmd, flags) != -1)
		return test_result(test_name, TEST_MEMBARRIER_FAIL);

	return test_result(test_name, TEST_MEMBARRIER_PASS);
}

static enum test_membarrier_status test_membarrier_flags_fail(void)
{
	int cmd = MEMBARRIER_CMD_QUERY, flags = 1;
	const char *test_name = "Wrong flags should fail";

	if (sys_membarrier(cmd, flags) != -1)
		return test_result(test_name, TEST_MEMBARRIER_FAIL);

	return test_result(test_name, TEST_MEMBARRIER_PASS);
}

static enum test_membarrier_status test_membarrier_success(void)
{
	int cmd = MEMBARRIER_CMD_SHARED, flags = 0;
	const char *test_name = "execute MEMBARRIER_CMD_SHARED";

	if (sys_membarrier(cmd, flags) != 0) {
		return test_result(test_name, TEST_MEMBARRIER_FAIL);
	}

	return test_result(test_name, TEST_MEMBARRIER_PASS);
}

static enum test_membarrier_status test_membarrier(void)
{
	enum test_membarrier_status status;

	status = test_membarrier_cmd_fail();
	if (status)
		return status;
	status = test_membarrier_flags_fail();
	if (status)
		return status;
	status = test_membarrier_success();
	if (status)
		return status;
	return TEST_MEMBARRIER_PASS;
}

static enum test_membarrier_status test_membarrier_query(void)
{
	int flags = 0, ret;

	printf("membarrier MEMBARRIER_CMD_QUERY ");
	ret = sys_membarrier(MEMBARRIER_CMD_QUERY, flags);
	if (ret < 0) {
		if (errno == ENOSYS) {
			/*
			 * It is valid to build a kernel with
			 * CONFIG_MEMBARRIER=n. However, this skips the tests.
			 */
			test_result("CONFIG_MEMBARRIER is not enabled\n", 0);
			return skip_tests();
		}
		return test_result("sys_membarrier() failed\n",
				   TEST_MEMBARRIER_FAIL);
	}
	if (!(ret & MEMBARRIER_CMD_SHARED)) {
		return test_result("command MEMBARRIER_CMD_SHARED is not supported.\n",
				   TEST_MEMBARRIER_FAIL);
	}
	return test_result("sys_membarrier available", TEST_MEMBARRIER_PASS);
}

int main(int argc, char **argv)
{
	printf("TAP version 13\n");
	switch (test_membarrier_query()) {
	case TEST_MEMBARRIER_FAIL:
		return bail_out();
	case TEST_MEMBARRIER_SKIP:
		return skip_tests();
	}
	switch (test_membarrier()) {
	case TEST_MEMBARRIER_FAIL:
		return bail_out();
	case TEST_MEMBARRIER_SKIP:
		return skip_tests();
	}
	printf("1..%d\n", test_num);
	return ksft_exit_pass();
}
