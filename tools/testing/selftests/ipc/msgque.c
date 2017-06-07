#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <linux/msg.h>
#include <fcntl.h>

#include "../kselftest.h"

enum test_ipc_status {
	TEST_IPC_PASS = 0,
	TEST_IPC_FAIL = -1,
	TEST_IPC_SKIP,
};

#define MAX_MSG_SIZE		32

struct msg1 {
	int msize;
	long mtype;
	char mtext[MAX_MSG_SIZE];
};

#define TEST_STRING "Test sysv5 msg"
#define MSG_TYPE 1

#define ANOTHER_TEST_STRING "Yet another test sysv5 msg"
#define ANOTHER_MSG_TYPE 26538

struct msgque_data {
	key_t key;
	int msq_id;
	int qbytes;
	int qnum;
	int mode;
	struct msg1 *messages;
};

static int test_num = 0;

static int test_result(const char *msg, int value)
{
	test_num++;
	printf("%s %d %s\n", value ? "not ok" : "ok", test_num, msg);
	return value;
}

static int skip_tests(void)
{
	printf("1..%d\n", test_num);
	return ksft_exit_skip();
}

static int bail_out(void)
{
	printf("Bail out!\n");
	printf("1..%d\n", test_num);
	return ksft_exit_fail();
}


int restore_queue(struct msgque_data *msgque)
{
	int fd, ret, id, i;
	char buf[32];

	fd = open("/proc/sys/kernel/msg_next_id", O_WRONLY);
	if (fd == -1) {
		return test_result(
			"Failed to open /proc/sys/kernel/msg_next_id",
			    TEST_IPC_FAIL);
	}
	test_result("/proc/sys/kernel/msg_next_id open",
		    TEST_IPC_PASS);
	sprintf(buf, "%d", msgque->msq_id);

	ret = write(fd, buf, strlen(buf));
	if (ret != strlen(buf)) {
		return test_result(
			"Failed to write to /proc/sys/kernel/msg_next_id",
			    TEST_IPC_FAIL);
	}
	test_result("Correctly wrote to /proc/sys/kernel/msg_next_id",
		    TEST_IPC_PASS);

	id = msgget(msgque->key, msgque->mode | IPC_CREAT | IPC_EXCL);
	if (id == -1) {
		return test_result("Failed to create queue",
			    TEST_IPC_FAIL);
	}
	test_result("Created correct queue", TEST_IPC_PASS);

	if (id != msgque->msq_id) {
		ret = test_result("Restored queue has wrong id",
			    TEST_IPC_FAIL);
		goto destroy;
	}
	test_result("Restored queue correct id",
		    TEST_IPC_PASS);

	for (i = 0; i < msgque->qnum; i++) {
		if (msgsnd(msgque->msq_id, &msgque->messages[i].mtype,
			   msgque->messages[i].msize, IPC_NOWAIT) != 0) {
			ret = test_result("msgsnd failed (%m)",
				    TEST_IPC_FAIL);
			goto destroy;
		};
	}
	return test_result("msgsnd succed",
			   TEST_IPC_PASS);

destroy:
	if (msgctl(id, IPC_RMID, 0))
		test_result("Failed to destroy queue",
			    TEST_IPC_FAIL);
	return ret;
}

int check_and_destroy_queue(struct msgque_data *msgque)
{
	struct msg1 message;
	int cnt = 0, ret;

	while (1) {
		ret = msgrcv(msgque->msq_id, &message.mtype, MAX_MSG_SIZE,
				0, IPC_NOWAIT);
		if (ret < 0) {
			if (errno == ENOMSG)
				break;
			ret = test_result("Failed to read IPC message.",
					   TEST_IPC_FAIL);
			goto err;
		}
		test_result("Read IPC message", TEST_IPC_PASS);

		if (ret != msgque->messages[cnt].msize) {
			ret = test_result("Wrong message size",
					   TEST_IPC_FAIL);
			goto err;
		}
		test_result("Correct messsage size", TEST_IPC_PASS);
		if (message.mtype != msgque->messages[cnt].mtype) {
			ret = test_result("Wrong message type",
				    TEST_IPC_FAIL);
			goto err;
		}
		test_result("Correct messsage type", TEST_IPC_PASS);
		if (memcmp(message.mtext, msgque->messages[cnt].mtext, ret)) {
			ret = test_result("Wrong message content",
					   TEST_IPC_FAIL);
			goto err;
		}
		cnt++;
		test_result("Correct messsage content", TEST_IPC_PASS);
	}

	if (cnt != msgque->qnum) {
		ret = test_result("Wrong message number",
			    TEST_IPC_FAIL);
		goto err;
	}
	test_result("Message number correct",
		    TEST_IPC_PASS);

	ret = 0;
err:
	if (msgctl(msgque->msq_id, IPC_RMID, 0)) {
		return test_result("Failed to destroy queue",
			    TEST_IPC_FAIL);
	}
	return test_result("Destroyed queue",
			   TEST_IPC_PASS);
}

int dump_queue(struct msgque_data *msgque)
{
	struct msqid64_ds ds;
	int kern_id;
	int i, ret;

	for (kern_id = 0; kern_id < 256; kern_id++) {
		ret = msgctl(kern_id, MSG_STAT, &ds);
		if (ret < 0) {
			if (errno == -EINVAL)
				continue;
			return test_result(
			"Failed to get stats for IPC queue",
			TEST_IPC_FAIL);
		}

		if (ret == msgque->msq_id)
			break;
	}
	test_result("get stats for IPC queue", TEST_IPC_PASS);

	msgque->messages = malloc(sizeof(struct msg1) * ds.msg_qnum);
	if (msgque->messages == NULL) {
		return test_result("Failed to get stats for IPC queue",
			TEST_IPC_FAIL);
	}
	test_result("get stats for IPC queue", TEST_IPC_PASS);

	msgque->qnum = ds.msg_qnum;
	msgque->mode = ds.msg_perm.mode;
	msgque->qbytes = ds.msg_qbytes;

	for (i = 0; i < msgque->qnum; i++) {
		ret = msgrcv(msgque->msq_id, &msgque->messages[i].mtype,
				MAX_MSG_SIZE, i, IPC_NOWAIT | MSG_COPY);
		if (ret < 0) {
			return test_result("Failed to copy IPC message",
			       TEST_IPC_FAIL);
		}
		msgque->messages[i].msize = ret;
	}
	return test_result("Copied IPC message", TEST_IPC_PASS);
}

int fill_msgque(struct msgque_data *msgque)
{
	struct msg1 msgbuf;

	msgbuf.mtype = MSG_TYPE;
	memcpy(msgbuf.mtext, TEST_STRING, sizeof(TEST_STRING));
	if (msgsnd(msgque->msq_id, &msgbuf.mtype, sizeof(TEST_STRING),
				IPC_NOWAIT) != 0) {
		return test_result("First message send failed",
				   TEST_IPC_FAIL);
	};
	test_result("First message sended",
		    TEST_IPC_PASS);

	msgbuf.mtype = ANOTHER_MSG_TYPE;
	memcpy(msgbuf.mtext, ANOTHER_TEST_STRING, sizeof(ANOTHER_TEST_STRING));
	if (msgsnd(msgque->msq_id, &msgbuf.mtype, sizeof(ANOTHER_TEST_STRING),
				IPC_NOWAIT) != 0) {
		return test_result("Second message send failed",
		       TEST_IPC_FAIL);
	};
	return test_result("Message sended", TEST_IPC_PASS);
}

int main(int argc, char **argv)
{
	int msg, pid, err;
	struct msgque_data msgque;

	printf("TAP version 13\n");

	if (getuid() != 0)
		return test_result("not root", TEST_IPC_FAIL);
	test_result("root",
		    TEST_IPC_PASS);

	msgque.key = ftok(argv[0], 822155650);
	if (msgque.key == -1)
		return test_result("Can't make key", TEST_IPC_FAIL);
	test_result("key made", TEST_IPC_PASS);

	msgque.msq_id = msgget(msgque.key, IPC_CREAT | IPC_EXCL | 0666);
	if (msgque.msq_id == -1) {
		err = -errno;
		test_result("Can't create queue",
			    TEST_IPC_FAIL);
		goto err_out;
	}
	test_result("Created Queue", TEST_IPC_PASS);

	err = fill_msgque(&msgque);
	if (err) {
		test_result("Failed to fill queue",
			    TEST_IPC_FAIL);
		goto err_destroy;
	}
	test_result("Filled queue", TEST_IPC_PASS);

	err = dump_queue(&msgque);
	if (err) {
		test_result("Failed to dump queue",
			    TEST_IPC_FAIL);
		goto err_destroy;
	}
	test_result("Dumped queue correctly",
		    TEST_IPC_PASS);

	err = check_and_destroy_queue(&msgque);
	if (err) {
		test_result("Failed to check and destroy queue",
			    TEST_IPC_FAIL);
		goto err_out;
	}
	test_result("Checked and destroyed queue", TEST_IPC_PASS);

	err = restore_queue(&msgque);
	if (err) {
		test_result("Failed to restore queue",
		       TEST_IPC_FAIL);
		goto err_destroy;
	}
	test_result("Queue restored",
		    TEST_IPC_PASS);

	err = check_and_destroy_queue(&msgque);
	if (err) {
		test_result("Failed to test queue",
			    TEST_IPC_FAIL);
		goto err_out;
	}
	test_result("Queue tested correctly",
		    TEST_IPC_PASS);

	printf("1..%d\n", test_num);
	return ksft_exit_pass();

err_destroy:
	if (msgctl(msgque.msq_id, IPC_RMID, 0)) {
		test_result("Failed to destroy queue",
			    TEST_IPC_FAIL);
		return bail_out();
	}
err_out:
	return bail_out();
}
