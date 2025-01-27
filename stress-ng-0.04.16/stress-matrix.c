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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>
#include <sys/time.h>
#include "stress-ng.h"

typedef float	matrix_type_t;

/*
 *  the CPU stress test has different classes of cpu stressor
 */
typedef void (*stress_matrix_func)(
	const size_t n,
	matrix_type_t a[n][n],
	matrix_type_t b[n][n],
	matrix_type_t r[n][n]);

typedef struct {
	const char		*name;	/* human readable form of stressor */
	const stress_matrix_func	func;	/* the stressor function */
} stress_matrix_stressor_info_t;

static stress_matrix_stressor_info_t *opt_matrix_stressor;
static stress_matrix_stressor_info_t matrix_methods[];
static size_t opt_matrix_size = 128;
static bool set_matrix_size = false;

void stress_set_matrix_size(const char *optarg)
{
	uint64_t size;

	set_matrix_size = true;
        size = get_uint64_byte(optarg);
        check_range("matrix-size", size,
                MIN_MATRIX_SIZE, MAX_MATRIX_SIZE);
	opt_matrix_size = (size_t)size;
}

/*
 *  stress_matrix_prod(void)
 *	matrix product
 */
static void OPTIMIZE3 stress_matrix_prod(
	const size_t n,
	matrix_type_t a[n][n],
	matrix_type_t b[n][n],
	matrix_type_t r[n][n])
{
	size_t i;

	for (i = 0; i < n; i++) {
		register size_t j;

		for (j = 0; j < n; j++) {
			register size_t k;

			for (k = 0; k < n; k++) {
				r[i][j] += a[i][k] * b[k][j];
			}
			if (!opt_do_run)
				return;
		}
	}
}

/*
 *  stress_matrix_add(void)
 *	matrix addition
 */
static void OPTIMIZE3 stress_matrix_add(
	const size_t n,
	matrix_type_t a[n][n],
	matrix_type_t b[n][n],
	matrix_type_t r[n][n])
{
	register size_t i;

	for (i = 0; i < n; i++) {
		register size_t j;

		for (j = 0; j < n; j++) {
			r[i][j] = a[i][j] + b[i][j];
		}
		if (!opt_do_run)
			return;
	}
}

/*
 *  stress_matrix_sub(void)
 *	matrix subtraction
 */
static void OPTIMIZE3 stress_matrix_sub(
	const size_t n,
	matrix_type_t a[n][n],
	matrix_type_t b[n][n],
	matrix_type_t r[n][n])
{
	register size_t i;

	for (i = 0; i < n; i++) {
		register size_t j;

		for (j = 0; j < n; j++) {
			r[i][j] = a[i][j] - b[i][j];
		}
		if (!opt_do_run)
			return;
	}
}

/*
 *  stress_matrix_trans(void)
 *	matrix transpose
 */
static void OPTIMIZE3 stress_matrix_trans(
	const size_t n,
	matrix_type_t a[n][n],
	matrix_type_t b[n][n],	/* Ignored */
	matrix_type_t r[n][n])
{
	register size_t i;

	(void)b;

	for (i = 0; i < n; i++) {
		register size_t j;

		for (j = 0; j < n; j++) {
			r[i][j] = a[j][i];
		}
		if (!opt_do_run)
			return;
	}
}

/*
 *  stress_matrix_mult(void)
 *	matrix scalar multiply
 */
static void OPTIMIZE3 stress_matrix_mult(
	const size_t n,
	matrix_type_t a[n][n],
	matrix_type_t b[n][n],
	matrix_type_t r[n][n])
{
	register size_t i;

	(void)b;
	matrix_type_t v = b[0][0];

	for (i = 0; i < n; i++) {
		register size_t j;

		for (j = 0; j < n; j++) {
			r[i][j] = v * a[i][j];
		}
		if (!opt_do_run)
			return;
	}
}

/*
 *  stress_matrix_div(void)
 *	matrix scalar divide
 */
static void OPTIMIZE3 stress_matrix_div(
	const size_t n,
	matrix_type_t a[n][n],
	matrix_type_t b[n][n],
	matrix_type_t r[n][n])
{
	register size_t i;

	(void)b;
	matrix_type_t v = b[0][0];

	for (i = 0; i < n; i++) {
		register size_t j;

		for (j = 0; j < n; j++) {
			r[i][j] = a[i][j] / v;
		}
		if (!opt_do_run)
			return;
	}
}

/*
 *  stress_matrix_hadamard(void)
 *	matrix hadamard product
 *	(A o B)ij = AijBij
 */
static void OPTIMIZE3 stress_matrix_hadamard(
	const size_t n,
	matrix_type_t a[n][n],
	matrix_type_t b[n][n],
	matrix_type_t r[n][n])
{
	register size_t i;

	for (i = 0; i < n; i++) {
		register size_t j;

		for (j = 0; j < n; j++) {
			r[i][j] = a[i][j] * b[i][j];
		}
		if (!opt_do_run)
			return;
	}
}

/*
 *  stress_matrix_frobenius(void)
 *	matrix frobenius product
 *	A : B = Sum(AijBij)
 */
static void OPTIMIZE3 stress_matrix_frobenius(
	const size_t n,
	matrix_type_t a[n][n],
	matrix_type_t b[n][n],
	matrix_type_t r[n][n])
{
	register size_t i;
	matrix_type_t sum = 0.0;

	(void)r;

	for (i = 0; i < n; i++) {
		register size_t j;

		for (j = 0; j < n; j++) {
			sum += a[i][j] * b[i][j];
		}
		if (!opt_do_run)
			return;
	}
	double_put(sum);
}

/*
 *  stress_matrix_all()
 *	iterate over all cpu stressors
 */
static void OPTIMIZE3 stress_matrix_all(
	const size_t n,
	matrix_type_t a[n][n],
	matrix_type_t b[n][n],
	matrix_type_t r[n][n])
{
	static int i = 1;	/* Skip over stress_matrix_all */

	matrix_methods[i++].func(n, a, b, r);
	if (!matrix_methods[i].func)
		i = 1;
}

/*
 * Table of cpu stress methods
 */
static stress_matrix_stressor_info_t matrix_methods[] = {
	{ "all",		stress_matrix_all },	/* Special "alli" test */

	{ "add",		stress_matrix_add },
	{ "div",		stress_matrix_div },
	{ "frobenius",		stress_matrix_frobenius },
	{ "hadamard",		stress_matrix_hadamard },
	{ "mult",		stress_matrix_mult },
	{ "prod",		stress_matrix_prod },
	{ "sub",		stress_matrix_sub },
	{ "trans",		stress_matrix_trans },
	{ NULL,			NULL }
};

/*
 *  stress_set_matrix_method()
 *	set the default matrix stress method
 */
int stress_set_matrix_method(const char *name)
{
	stress_matrix_stressor_info_t *info = matrix_methods;

	for (info = matrix_methods; info->func; info++) {
		if (!strcmp(info->name, name)) {
			opt_matrix_stressor = info;
			return 0;
		}
	}

	fprintf(stderr, "matrix-method must be one of:");
	for (info = matrix_methods; info->func; info++) {
		fprintf(stderr, " %s", info->name);
	}
	fprintf(stderr, "\n");

	return -1;
}

/*
 *  stress_matrix()
 *	stress CPU by doing floating point math ops
 */
int stress_matrix(
	uint64_t *const counter,
	const uint32_t instance,
	const uint64_t max_ops,
	const char *name)
{
	stress_matrix_func func = opt_matrix_stressor->func;
	size_t n;
	const matrix_type_t v = 1 / (matrix_type_t)((uint32_t)~0);

	(void)instance;
	(void)name;

	if (!set_matrix_size) {
		if (opt_flags & OPT_FLAGS_MAXIMIZE)
			opt_matrix_size = MAX_MATRIX_SIZE;
		if (opt_flags & OPT_FLAGS_MINIMIZE)
			opt_matrix_size = MIN_MATRIX_SIZE;
	}
	n = opt_matrix_size;

	{
		register size_t i;
		matrix_type_t a[n][n], b[n][n], r[n][n];
		/*
		 *  Initialise matrices
		 */
		for (i = 0; i < n; i++) {
			register size_t j;

			for (j = 0; j < n; j++) {
				a[i][j] = (matrix_type_t)mwc64() * v;
				b[i][j] = (matrix_type_t)mwc64() * v;
				r[i][j] = 0.0;
			}
		}

		/*
		 * Normal use case, 100% load, simple spinning on CPU
		 */
		do {
			(void)func(n, a, b, r);
			(*counter)++;
		} while (opt_do_run && (!max_ops || *counter < max_ops));
	}

	return EXIT_SUCCESS;
}
