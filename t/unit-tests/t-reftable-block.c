/*
Copyright 2020 Google LLC

Use of this source code is governed by a BSD-style
license that can be found in the LICENSE file or at
https://developers.google.com/open-source/licenses/bsd
*/

#include "test-lib.h"
#include "reftable/block.h"
#include "reftable/blocksource.h"
#include "reftable/constants.h"
#include "reftable/reftable-error.h"

static void t_block_read_write(void)
{
	const int header_off = 21; /* random */
	char *names[30];
	const size_t N = ARRAY_SIZE(names);
	const size_t block_size = 1024;
	struct reftable_block block = { 0 };
	struct block_writer bw = {
		.last_key = STRBUF_INIT,
	};
	struct reftable_record rec = {
		.type = BLOCK_TYPE_REF,
	};
	size_t i = 0;
	int ret;
	struct block_reader br = { 0 };
	struct block_iter it = BLOCK_ITER_INIT;
	size_t j = 0;
	struct strbuf want = STRBUF_INIT;

	REFTABLE_CALLOC_ARRAY(block.data, block_size);
	block.len = block_size;
	block.source = malloc_block_source();
	block_writer_init(&bw, BLOCK_TYPE_REF, block.data, block_size,
			  header_off, hash_size(GIT_SHA1_FORMAT_ID));

	rec.u.ref.refname = (char *) "";
	rec.u.ref.value_type = REFTABLE_REF_DELETION;
	ret = block_writer_add(&bw, &rec);
	check_int(ret, ==, REFTABLE_API_ERROR);

	for (i = 0; i < N; i++) {
		char name[100];
		snprintf(name, sizeof(name), "branch%02"PRIuMAX, (uintmax_t)i);

		rec.u.ref.refname = name;
		rec.u.ref.value_type = REFTABLE_REF_VAL1;
		memset(rec.u.ref.value.val1, i, GIT_SHA1_RAWSZ);

		names[i] = xstrdup(name);
		ret = block_writer_add(&bw, &rec);
		rec.u.ref.refname = NULL;
		rec.u.ref.value_type = REFTABLE_REF_DELETION;
		check_int(ret, ==, 0);
	}

	ret = block_writer_finish(&bw);
	check_int(ret, >, 0);

	block_writer_release(&bw);

	block_reader_init(&br, &block, header_off, block_size, GIT_SHA1_RAWSZ);

	block_iter_seek_start(&it, &br);

	while (1) {
		ret = block_iter_next(&it, &rec);
		check_int(ret, >=, 0);
		if (ret > 0) {
			check_int(i, ==, N);
			break;
		}
		check_str(names[j], rec.u.ref.refname);
		j++;
	}

	reftable_record_release(&rec);
	block_iter_close(&it);

	for (i = 0; i < N; i++) {
		struct block_iter it = BLOCK_ITER_INIT;
		strbuf_reset(&want);
		strbuf_addstr(&want, names[i]);

		ret = block_iter_seek_key(&it, &br, &want);
		check_int(ret, ==, 0);

		ret = block_iter_next(&it, &rec);
		check_int(ret, ==, 0);

		check_str(names[i], rec.u.ref.refname);

		want.len--;
		ret = block_iter_seek_key(&it, &br, &want);
		check_int(ret, ==, 0);

		ret = block_iter_next(&it, &rec);
		check_int(ret, ==, 0);
		check_str(names[10 * (i / 10)], rec.u.ref.refname);

		block_iter_close(&it);
	}

	reftable_record_release(&rec);
	reftable_block_done(&br.block);
	strbuf_release(&want);
	for (i = 0; i < N; i++)
		reftable_free(names[i]);
}

int cmd_main(int argc, const char *argv[])
{
	TEST(t_block_read_write(), "read-write operations on blocks work");

	return test_done();
}
