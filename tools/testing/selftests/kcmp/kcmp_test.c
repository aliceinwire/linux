#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include <linux/unistd.h>
#include <linux/kcmp.h>

#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "../kselftest.h"

static long sys_kcmp(int pid1, int pid2, int type, int fd1, int fd2)
{
	return syscall(__NR_kcmp, pid1, pid2, type, fd1, fd2);
}

int main(int argc, char **argv)
{
	const char kpath[] = "kcmp-test-file";
	int pid1, pid2;
	int fd1, fd2;
	int status;

	fd1 = open(kpath, O_RDWR | O_CREAT | O_TRUNC, 0644);
	pid1 = getpid();

	if (fd1 < 0) {
		ksft_exit_fail_msg("Can't create file");
	}

	pid2 = fork();
	if (pid2 < 0) {
		ksft_exit_fail_msg("fork failed");
	}

	if (!pid2) {
		int pid2 = getpid();
		int ret;
		char buf[512];

		fd2 = open(kpath, O_RDWR, 0644);
		if (fd2 < 0) {
			ksft_exit_fail_msg("Can't open file");
		}

		/* An example of output and arguments */
		snprintf(&buf[0], sizeof(buf) ,"pid1: %6d pid2: %6d FD: %2ld FILES: %2ld VM: %2ld "
		       "FS: %2ld SIGHAND: %2ld IO: %2ld SYSVSEM: %2ld "
		       "INV: %2ld",
		       pid1, pid2,
		       sys_kcmp(pid1, pid2, KCMP_FILE,		fd1, fd2),
		       sys_kcmp(pid1, pid2, KCMP_FILES,		0, 0),
		       sys_kcmp(pid1, pid2, KCMP_VM,		0, 0),
		       sys_kcmp(pid1, pid2, KCMP_FS,		0, 0),
		       sys_kcmp(pid1, pid2, KCMP_SIGHAND,	0, 0),
		       sys_kcmp(pid1, pid2, KCMP_IO,		0, 0),
		       sys_kcmp(pid1, pid2, KCMP_SYSVSEM,	0, 0),

			/* This one should fail */
		       sys_kcmp(pid1, pid2, KCMP_TYPES + 1,	0, 0));

		/* This one should return same fd */
		ret = sys_kcmp(pid1, pid2, KCMP_FILE, fd1, fd1);
		ksft_test_result_pass(buf);
		if (ret) {
			snprintf(&buf[0], sizeof(buf), "0 expected but %d returned (%s)",
				ret, strerror(errno));
			ksft_test_result_fail(buf);
			ret = -1;
		} else {
			ksft_test_result_pass("0 returned as expected");
		}

		/* Compare with self */
		ret = sys_kcmp(pid1, pid1, KCMP_VM, 0, 0);
		if (ret) {
			snprintf(&buf[0], sizeof(buf), "0 expected but %d returned (%s)",
				ret, strerror(errno));
			ksft_test_result_fail(buf);
			ret = -1;
		} else {
			ksft_test_result_pass("0 returned as expected");
		}

		if (ret)
			ksft_exit_fail();
		else
			ksft_exit_pass();
	}

	waitpid(pid2, &status, P_ALL);

	return 0;
}
