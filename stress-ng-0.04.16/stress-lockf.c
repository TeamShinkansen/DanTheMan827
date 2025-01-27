/*
 * Copyright (C) 2013-2015 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * This code is a complete clean re-write of the stress tool by
 * Colin Ian King <colin.king@canonical.com> and attempts to be
 * backwardly compatible with the stress tool by Amos Waterland
 * <apw@rossby.metr.ou.edu> but has more stress tests and more
 * functionality.
 *
 */
#define _GNU_SOURCE

#include "stress-ng.h"

#if defined(STRESS_LOCKF)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

#define LOCK_FILE_SIZE	(64 * 1024)
#define LOCK_SIZE	(8)
#define LOCK_MAX	(1024)

typedef struct lockf_info {
	off_t	offset;
	struct lockf_info *next;
} lockf_info_t;

typedef struct {
	lockf_info_t *head;		/* Head of lockf_info procs list */
	lockf_info_t *tail;		/* Tail of lockf_info procs list */
	lockf_info_t *free;		/* List of free'd lockf_infos */
	uint64_t length;		/* Length of list */
} lockf_info_list_t;

static lockf_info_list_t lockf_infos;

/*
 *  stress_lockf_info_new()
 *	allocate a new lockf_info, add to end of list
 */
static lockf_info_t *stress_lockf_info_new(void)
{
	lockf_info_t *new;

	if (lockf_infos.free) {
		/* Pop an old one off the free list */
		new = lockf_infos.free;
		lockf_infos.free = new->next;
		new->next = NULL;
	} else {
		new = calloc(1, sizeof(*new));
		if (!new)
			return NULL;
	}

	if (lockf_infos.head)
		lockf_infos.tail->next = new;
	else
		lockf_infos.head = new;

	lockf_infos.tail = new;
	lockf_infos.length++;

	return new;
}

/*
 *  stress_lockf_info_head_remove
 *	reap a lockf_info and remove a lockf_info from head of list, put it onto
 *	the free lockf_info list
 */
static void stress_lockf_info_head_remove(void)
{
	if (lockf_infos.head) {
		lockf_info_t *head = lockf_infos.head;

		if (lockf_infos.tail == lockf_infos.head) {
			lockf_infos.tail = NULL;
			lockf_infos.head = NULL;
		} else {
			lockf_infos.head = head->next;
		}

		/* Shove it on the free list */
		head->next = lockf_infos.free;
		lockf_infos.free = head;

		lockf_infos.length--;
	}
}

/*
 *  stress_lockf_info_free()
 *	free the lockf_infos off the lockf_info head and free lists
 */
static void stress_lockf_info_free(void)
{
	while (lockf_infos.head) {
		lockf_info_t *next = lockf_infos.head->next;

		free(lockf_infos.head);
		lockf_infos.head = next;
	}

	while (lockf_infos.free) {
		lockf_info_t *next = lockf_infos.free->next;

		free(lockf_infos.free);
		lockf_infos.free = next;
	}
}

/*
 *  stress_lockf_unlock()
 *	pop oldest lock record off list and unlock it
 */
static int stress_lockf_unlock(const char *name, const int fd)
{
	/* Pop one off list */
	if (!lockf_infos.head)
		return 0;

	if (lseek(fd, lockf_infos.head->offset, SEEK_SET) < 0) {
		pr_failed_err(name, "lseek");
		return -1;
	}
	stress_lockf_info_head_remove();

	if (lockf(fd, F_ULOCK, LOCK_SIZE) < 0) {
		pr_failed_err(name, "lockf unlock");
		return -1;
	}
	return 0;
}

/*
 *  stress_lockf_contention()
 *	hammer lock/unlock to create some file lock contention
 */
static int stress_lockf_contention(
	const char *name,
	const int fd,
	uint64_t *const counter,
	const uint64_t max_ops)
{
	const int lockf_cmd = (opt_flags & OPT_FLAGS_LOCKF_NONBLK) ?
		F_TLOCK : F_LOCK;

	mwc_reseed();

	do {
		off_t offset;
		int rc;
		lockf_info_t *lockf_info;

		if (lockf_infos.length >= LOCK_MAX)
			if (stress_lockf_unlock(name, fd) < 0)
				return -1;

		offset = mwc64() % (LOCK_FILE_SIZE - LOCK_SIZE);
		if (lseek(fd, offset, SEEK_SET) < 0) {
			pr_failed_err(name, "lseek");
			return -1;
		}
		rc = lockf(fd, lockf_cmd, LOCK_SIZE);
		if (rc < 0) {
			if (stress_lockf_unlock(name, fd) < 0)
				return -1;
			continue;
		}
		/* Locked OK, add to lock list */

		lockf_info = stress_lockf_info_new();
		if (!lockf_info) {
			pr_failed_err(name, "calloc");
			return -1;
		}
		lockf_info->offset = offset;
		(*counter)++;
	} while (opt_do_run && (!max_ops || *counter < max_ops));

	return 0;
}

/*
 *  stress_lockf
 *	stress file locking via lockf()
 */
int stress_lockf(
	uint64_t *const counter,
	const uint32_t instance,
	const uint64_t max_ops,
	const char *name)
{
	int fd, ret = EXIT_FAILURE;
	pid_t pid = getpid(), cpid = -1;
	char filename[PATH_MAX];
	char dirname[PATH_MAX];
	char buffer[4096];
	off_t offset;
	ssize_t rc;

	memset(buffer, 0, sizeof(buffer));

	/*
	 *  There will be a race to create the directory
	 *  so EEXIST is expected on all but one instance
	 */
	(void)stress_temp_dir(dirname, sizeof(dirname), name, pid, instance);
	if (mkdir(dirname, S_IRWXU) < 0) {
		if (errno != EEXIST) {
			pr_failed_err(name, "mkdir");
			return EXIT_FAILURE;
		}
	}

	/*
	 *  Lock file is based on parent pid and instance 0
	 *  as we need to share this among all the other
	 *  stress flock processes
	 */
	(void)stress_temp_filename(filename, sizeof(filename),
		name, pid, instance, mwc32());

	if ((fd = open(filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)) < 0) {
		pr_failed_err(name, "open");
		(void)rmdir(dirname);
		return EXIT_FAILURE;
	}

	if (lseek(fd, 0, SEEK_SET) < 0) {
		pr_failed_err(name, "lseek");
		goto tidy;
	}
	for (offset = 0; offset < LOCK_FILE_SIZE; offset += sizeof(buffer)) {
redo:
		if (!opt_do_run)
			goto tidy;
		rc = write(fd, buffer, sizeof(buffer));
		if ((rc < 0) || (rc != sizeof(buffer))) {
			if ((errno == EAGAIN) || (errno == EINTR))
				goto redo;
			pr_failed_err(name, "write");
			goto tidy;
		}
	}

again:
	cpid = fork();
	if (cpid < 0) {
		if (opt_do_run && (errno == EAGAIN))
			goto again;
		pr_failed_err(name, "fork");
		goto tidy;
	}
	if (cpid == 0) {
		if (stress_lockf_contention(name, fd, counter, max_ops) < 0)
			exit(EXIT_FAILURE);
		stress_lockf_info_free();
		exit(EXIT_SUCCESS);
	}

	if (stress_lockf_contention(name, fd, counter, max_ops) == 0)
		ret = EXIT_SUCCESS;
tidy:
	if (cpid > 0) {
		int status;

		(void)kill(cpid, SIGKILL);
		(void)waitpid(cpid, &status, 0);
	}
	stress_lockf_info_free();

	(void)close(fd);
	(void)unlink(filename);
	(void)rmdir(dirname);

	return ret;
}

#endif
