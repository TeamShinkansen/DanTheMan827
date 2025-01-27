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

#if defined(STRESS_CLONE)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define CLONE_STACK_SIZE	(32*1024)

static uint64_t opt_clone_max = DEFAULT_ZOMBIES;
static bool set_clone_max = false;

typedef struct clone {
	pid_t	pid;
	struct clone *next;
	char *stack;
} clone_t;

typedef struct {
	clone_t *head;		/* Head of clone procs list */
	clone_t *tail;		/* Tail of clone procs list */
	clone_t *free;		/* List of free'd clones */
	uint64_t length;	/* Length of list */
} clone_list_t;

static clone_list_t clones;

/*
 *  A random selection of clone flags that are worth exercising
 */
static int flags[] = {
	0,
#if defined(CLONE_FILES)
	CLONE_FILES,
#endif
#if defined(CLONE_FS)
	CLONE_FS,
#endif
#if defined(CLONE_IO)
	CLONE_IO,
#endif
#if defined(CLONE_NEWIPC)
	CLONE_NEWIPC,
#endif
#if defined(CLONE_NEWNET)
	CLONE_NEWNET,
#endif
#if defined(CLONE_NEWNS)
	CLONE_NEWNS,
#endif
#if defined(CLONE_NEWUSER)
	CLONE_NEWUSER,
#endif
#if defined(CLONE_NEWUTS)
	CLONE_NEWUTS,
#endif
#if defined(CLONE_SIGHAND)
	CLONE_SIGHAND,
#endif
#if defined(CLONE_SYSVSEM)
	CLONE_SYSVSEM,
#endif
#if defined(CLONE_UNTRACED)
	CLONE_UNTRACED,
#endif
#if defined(CLONE_VM)
	CLONE_VM,
#endif
};

static int unshare_flags[] = {
#if defined(CLONE_FILES)
	CLONE_FILES,
#endif
#if defined(CLONE_FS)
	CLONE_FS,
#endif
#if defined(CLONE_NEWIPC)
	CLONE_NEWIPC,
#endif
#if defined(CLONE_NEWNET)
	CLONE_NEWNET,
#endif
#if defined(CLONE_NEWNS)
	CLONE_NEWNS,
#endif
#if defined(CLONE_NEWUTS)
	CLONE_NEWUTS,
#endif
#if defined(CLONE_SYSVSEM)
	CLONE_SYSVSEM,
#endif
};


/*
 *  stress_clone_new()
 *	allocate a new clone, add to end of list
 */
static clone_t *stress_clone_new(void)
{
	clone_t *new;

	if (clones.free) {
		/* Pop an old one off the free list */
		new = clones.free;
		clones.free = new->next;
		new->next = NULL;
	} else {
		new = calloc(1, sizeof(*new));
		if (!new)
			return NULL;
		new->stack = malloc(CLONE_STACK_SIZE);
		if (!new->stack) {
			free(new);
			return NULL;
		}
	}

	if (clones.head)
		clones.tail->next = new;
	else
		clones.head = new;

	clones.tail = new;
	clones.length++;

	return new;
}

/*
 *  stress_clone_head_remove
 *	reap a clone and remove a clone from head of list, put it onto
 *	the free clone list
 */
void stress_clone_head_remove(void)
{
	if (clones.head) {
		int status;
		clone_t *head = clones.head;

		(void)waitpid(clones.head->pid, &status, __WCLONE);

		if (clones.tail == clones.head) {
			clones.tail = NULL;
			clones.head = NULL;
		} else {
			clones.head = head->next;
		}

		/* Shove it on the free list */
		head->next = clones.free;
		clones.free = head;

		clones.length--;
	}
}

/*
 *  stress_clone_free()
 *	free the clones off the clone free lists
 */
void stress_clone_free(void)
{
	while (clones.head) {
		clone_t *next = clones.head->next;

		free(clones.head->stack);
		free(clones.head);
		clones.head = next;
	}
	while (clones.free) {
		clone_t *next = clones.free->next;

		free(clones.free->stack);
		free(clones.free);
		clones.free = next;
	}
}

/*
 *  stress_set_clone_max()
 *	set maximum number of clones allowed
 */
void stress_set_clone_max(const char *optarg)
{
	set_clone_max = true;
	opt_clone_max = get_uint64_byte(optarg);
	check_range("clone-max", opt_clone_max,
		MIN_ZOMBIES, MAX_ZOMBIES);
}

/*
 *  clone_func()
 *	clone thread just returns immediately
 */
static int clone_func(void *arg)
{
	size_t i;

	(void)arg;

	for (i = 0; i < SIZEOF_ARRAY(unshare_flags); i++) {
		(void)unshare(unshare_flags[i]);
	}

	return 0;
}

/*
 *  clone_stack_dir()
 *	determine which way the stack goes, up / down
 */
static ssize_t clone_stack_dir(uint8_t *val1)
{
	uint8_t val2;

	return (val1 - &val2) > 0 ? 1 : -1;
}


/*
 *  stress_clone()
 *	stress by cloning and exiting
 */
int stress_clone(
	uint64_t *const counter,
	const uint32_t instance,
	const uint64_t max_ops,
	const char *name)
{
	uint64_t max_clones = 0;
	const ssize_t stack_offset = clone_stack_dir((uint8_t *)&max_clones) * CLONE_STACK_SIZE;

	(void)instance;

	if (!set_clone_max) {
		if (opt_flags & OPT_FLAGS_MAXIMIZE)
			opt_clone_max = MAX_ZOMBIES;
		if (opt_flags & OPT_FLAGS_MINIMIZE)
			opt_clone_max = MIN_ZOMBIES;
	}

	do {
		if (clones.length < opt_clone_max) {
			clone_t *clone_info;
			char *stack_top;
			int flag = flags[mwc32() % SIZEOF_ARRAY(flags)];

			clone_info = stress_clone_new();
			if (!clone_info)
				break;
			stack_top = clone_info->stack + stack_offset;
			clone_info->pid = clone(clone_func, stack_top, flag, NULL);
			if (clone_info->pid == -1) {
				/* Reached max forks or error (e.g. EPERM)? .. then reap */
				stress_clone_head_remove();
				continue;
			}

			if (max_clones < clones.length)
				max_clones = clones.length;
			(*counter)++;
		} else {
			stress_clone_head_remove();
		}
	} while (opt_do_run && (!max_ops || *counter < max_ops));

	pr_inf(stderr, "%s: created a maximum of %" PRIu64 " clones\n",
		name, max_clones);

	/* And reap */
	while (clones.head) {
		stress_clone_head_remove();
	}
	/* And free */
	stress_clone_free();

	return EXIT_SUCCESS;
}

#endif
