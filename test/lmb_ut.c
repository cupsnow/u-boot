// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2018 Simon Goldschmidt
 */

#include <alist.h>
#include <dm.h>
#include <lmb.h>
#include <log.h>
#include <malloc.h>
#include <dm/test.h>
#include <test/suites.h>
#include <test/test.h>
#include <test/ut.h>


#define LMB_TEST(_name, _flags)	UNIT_TEST(_name, _flags, lmb_test)

static inline bool lmb_is_nomap(struct lmb_region *m)
{
	return m->flags & LMB_NOMAP;
}

static int check_lmb(struct unit_test_state *uts, struct alist *mem_lst,
		     struct alist *used_lst, phys_addr_t ram_base,
		     phys_size_t ram_size, unsigned long num_reserved,
		     phys_addr_t base1, phys_size_t size1,
		     phys_addr_t base2, phys_size_t size2,
		     phys_addr_t base3, phys_size_t size3)
{
	struct lmb_region *mem, *used;

	mem = mem_lst->data;
	used = used_lst->data;

	if (ram_size) {
		ut_asserteq(mem_lst->count, 1);
		ut_asserteq(mem[0].base, ram_base);
		ut_asserteq(mem[0].size, ram_size);
	}

	ut_asserteq(used_lst->count, num_reserved);
	if (num_reserved > 0) {
		ut_asserteq(used[0].base, base1);
		ut_asserteq(used[0].size, size1);
	}
	if (num_reserved > 1) {
		ut_asserteq(used[1].base, base2);
		ut_asserteq(used[1].size, size2);
	}
	if (num_reserved > 2) {
		ut_asserteq(used[2].base, base3);
		ut_asserteq(used[2].size, size3);
	}
	return 0;
}

#define ASSERT_LMB(mem_lst, used_lst, ram_base, ram_size, num_reserved, base1, size1, \
		   base2, size2, base3, size3) \
		   ut_assert(!check_lmb(uts, mem_lst, used_lst, ram_base, ram_size, \
			     num_reserved, base1, size1, base2, size2, base3, \
			     size3))

static int test_multi_alloc(struct unit_test_state *uts, const phys_addr_t ram,
			    const phys_size_t ram_size, const phys_addr_t ram0,
			    const phys_size_t ram0_size,
			    const phys_addr_t alloc_64k_addr)
{
	const phys_addr_t ram_end = ram + ram_size;
	const phys_addr_t alloc_64k_end = alloc_64k_addr + 0x10000;

	long ret;
	struct alist *mem_lst, *used_lst;
	struct lmb_region *mem, *used;
	phys_addr_t a, a2, b, b2, c, d;

	/* check for overflow */
	ut_assert(ram_end == 0 || ram_end > ram);
	ut_assert(alloc_64k_end > alloc_64k_addr);
	/* check input addresses + size */
	ut_assert(alloc_64k_addr >= ram + 8);
	ut_assert(alloc_64k_end <= ram_end - 8);

	ut_asserteq(lmb_init(&mem_lst, &used_lst), 0);
	mem = mem_lst->data;
	used = used_lst->data;

	if (ram0_size) {
		ret = lmb_add(ram0, ram0_size);
		ut_asserteq(ret, 0);
	}

	ret = lmb_add(ram, ram_size);
	ut_asserteq(ret, 0);

	if (ram0_size) {
		ut_asserteq(mem_lst->count, 2);
		ut_asserteq(mem[0].base, ram0);
		ut_asserteq(mem[0].size, ram0_size);
		ut_asserteq(mem[1].base, ram);
		ut_asserteq(mem[1].size, ram_size);
	} else {
		ut_asserteq(mem_lst->count, 1);
		ut_asserteq(mem[0].base, ram);
		ut_asserteq(mem[0].size, ram_size);
	}

	/* reserve 64KiB somewhere */
	ret = lmb_reserve(alloc_64k_addr, 0x10000);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, 0, 0, 1, alloc_64k_addr, 0x10000,
		   0, 0, 0, 0);

	/* allocate somewhere, should be at the end of RAM */
	a = lmb_alloc(4, 1);
	ut_asserteq(a, ram_end - 4);
	ASSERT_LMB(mem_lst, used_lst, 0, 0, 2, alloc_64k_addr, 0x10000,
		   ram_end - 4, 4, 0, 0);
	/* alloc below end of reserved region -> below reserved region */
	b = lmb_alloc_base(4, 1, alloc_64k_end);
	ut_asserteq(b, alloc_64k_addr - 4);
	ASSERT_LMB(mem_lst, used_lst, 0, 0, 2,
		   alloc_64k_addr - 4, 0x10000 + 4, ram_end - 4, 4, 0, 0);

	/* 2nd time */
	c = lmb_alloc(4, 1);
	ut_asserteq(c, ram_end - 8);
	ASSERT_LMB(mem_lst, used_lst, 0, 0, 2,
		   alloc_64k_addr - 4, 0x10000 + 4, ram_end - 8, 8, 0, 0);
	d = lmb_alloc_base(4, 1, alloc_64k_end);
	ut_asserteq(d, alloc_64k_addr - 8);
	ASSERT_LMB(mem_lst, used_lst, 0, 0, 2,
		   alloc_64k_addr - 8, 0x10000 + 8, ram_end - 8, 8, 0, 0);

	ret = lmb_free(a, 4);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, 0, 0, 2,
		   alloc_64k_addr - 8, 0x10000 + 8, ram_end - 8, 4, 0, 0);
	/* allocate again to ensure we get the same address */
	a2 = lmb_alloc(4, 1);
	ut_asserteq(a, a2);
	ASSERT_LMB(mem_lst, used_lst, 0, 0, 2,
		   alloc_64k_addr - 8, 0x10000 + 8, ram_end - 8, 8, 0, 0);
	ret = lmb_free(a2, 4);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, 0, 0, 2,
		   alloc_64k_addr - 8, 0x10000 + 8, ram_end - 8, 4, 0, 0);

	ret = lmb_free(b, 4);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, 0, 0, 3,
		   alloc_64k_addr - 8, 4, alloc_64k_addr, 0x10000,
		   ram_end - 8, 4);
	/* allocate again to ensure we get the same address */
	b2 = lmb_alloc_base(4, 1, alloc_64k_end);
	ut_asserteq(b, b2);
	ASSERT_LMB(mem_lst, used_lst, 0, 0, 2,
		   alloc_64k_addr - 8, 0x10000 + 8, ram_end - 8, 4, 0, 0);
	ret = lmb_free(b2, 4);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, 0, 0, 3,
		   alloc_64k_addr - 8, 4, alloc_64k_addr, 0x10000,
		   ram_end - 8, 4);

	ret = lmb_free(c, 4);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, 0, 0, 2,
		   alloc_64k_addr - 8, 4, alloc_64k_addr, 0x10000, 0, 0);
	ret = lmb_free(d, 4);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, 0, 0, 1, alloc_64k_addr, 0x10000,
		   0, 0, 0, 0);

	if (ram0_size) {
		ut_asserteq(mem_lst->count, 2);
		ut_asserteq(mem[0].base, ram0);
		ut_asserteq(mem[0].size, ram0_size);
		ut_asserteq(mem[1].base, ram);
		ut_asserteq(mem[1].size, ram_size);
	} else {
		ut_asserteq(mem_lst->count, 1);
		ut_asserteq(mem[0].base, ram);
		ut_asserteq(mem[0].size, ram_size);
	}

	lmb_uninit(mem_lst, used_lst);

	return 0;
}

static int test_multi_alloc_512mb(struct unit_test_state *uts,
				  const phys_addr_t ram)
{
	return test_multi_alloc(uts, ram, 0x20000000, 0, 0, ram + 0x10000000);
}

static int test_multi_alloc_512mb_x2(struct unit_test_state *uts,
				     const phys_addr_t ram,
				     const phys_addr_t ram0)
{
	return test_multi_alloc(uts, ram, 0x20000000, ram0, 0x20000000,
				ram + 0x10000000);
}

/* Create a memory region with one reserved region and allocate */
static int lmb_test_lmb_simple_norun(struct unit_test_state *uts)
{
	int ret;

	/* simulate 512 MiB RAM beginning at 1GiB */
	ret = test_multi_alloc_512mb(uts, 0x40000000);
	if (ret)
		return ret;

	/* simulate 512 MiB RAM beginning at 1.5GiB */
	return test_multi_alloc_512mb(uts, 0xE0000000);
}
LMB_TEST(lmb_test_lmb_simple_norun, UT_TESTF_MANUAL);

/* Create two memory regions with one reserved region and allocate */
static int lmb_test_lmb_simple_x2_norun(struct unit_test_state *uts)
{
	int ret;

	/* simulate 512 MiB RAM beginning at 2GiB and 1 GiB */
	ret = test_multi_alloc_512mb_x2(uts, 0x80000000, 0x40000000);
	if (ret)
		return ret;

	/* simulate 512 MiB RAM beginning at 3.5GiB and 1 GiB */
	return test_multi_alloc_512mb_x2(uts, 0xE0000000, 0x40000000);
}
LMB_TEST(lmb_test_lmb_simple_x2_norun, UT_TESTF_MANUAL);

/* Simulate 512 MiB RAM, allocate some blocks that fit/don't fit */
static int test_bigblock(struct unit_test_state *uts, const phys_addr_t ram)
{
	const phys_size_t ram_size = 0x20000000;
	const phys_size_t big_block_size = 0x10000000;
	const phys_addr_t ram_end = ram + ram_size;
	const phys_addr_t alloc_64k_addr = ram + 0x10000000;
	struct alist *mem_lst, *used_lst;
	struct lmb_region *mem, *used;
	long ret;
	phys_addr_t a, b;

	/* check for overflow */
	ut_assert(ram_end == 0 || ram_end > ram);

	ut_asserteq(lmb_init(&mem_lst, &used_lst), 0);
	mem = mem_lst->data;
	used = used_lst->data;

	ret = lmb_add(ram, ram_size);
	ut_asserteq(ret, 0);

	/* reserve 64KiB in the middle of RAM */
	ret = lmb_reserve(alloc_64k_addr, 0x10000);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, alloc_64k_addr, 0x10000,
		   0, 0, 0, 0);

	/* allocate a big block, should be below reserved */
	a = lmb_alloc(big_block_size, 1);
	ut_asserteq(a, ram);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, a,
		   big_block_size + 0x10000, 0, 0, 0, 0);
	/* allocate 2nd big block */
	/* This should fail, printing an error */
	b = lmb_alloc(big_block_size, 1);
	ut_asserteq(b, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, a,
		   big_block_size + 0x10000, 0, 0, 0, 0);

	ret = lmb_free(a, big_block_size);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, alloc_64k_addr, 0x10000,
		   0, 0, 0, 0);

	/* allocate too big block */
	/* This should fail, printing an error */
	a = lmb_alloc(ram_size, 1);
	ut_asserteq(a, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, alloc_64k_addr, 0x10000,
		   0, 0, 0, 0);

	lmb_uninit(mem_lst, used_lst);

	return 0;
}

static int lmb_test_lmb_big_norun(struct unit_test_state *uts)
{
	int ret;

	/* simulate 512 MiB RAM beginning at 1GiB */
	ret = test_bigblock(uts, 0x40000000);
	if (ret)
		return ret;

	/* simulate 512 MiB RAM beginning at 1.5GiB */
	return test_bigblock(uts, 0xE0000000);
}
LMB_TEST(lmb_test_lmb_big_norun, UT_TESTF_MANUAL);

/* Simulate 512 MiB RAM, allocate a block without previous reservation */
static int test_noreserved(struct unit_test_state *uts, const phys_addr_t ram,
			   const phys_addr_t alloc_size, const ulong align)
{
	const phys_size_t ram_size = 0x20000000;
	const phys_addr_t ram_end = ram + ram_size;
	long ret;
	phys_addr_t a, b;
	struct alist *mem_lst, *used_lst;
	struct lmb_region *mem, *used;
	const phys_addr_t alloc_size_aligned = (alloc_size + align - 1) &
		~(align - 1);

	/* check for overflow */
	ut_assert(ram_end == 0 || ram_end > ram);

	ut_asserteq(lmb_init(&mem_lst, &used_lst), 0);
	mem = mem_lst->data;
	used = used_lst->data;

	ret = lmb_add(ram, ram_size);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 0, 0, 0, 0, 0, 0, 0);

	/* allocate a block */
	a = lmb_alloc(alloc_size, align);
	ut_assert(a != 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1,
		   ram + ram_size - alloc_size_aligned, alloc_size, 0, 0, 0, 0);

	/* allocate another block */
	b = lmb_alloc(alloc_size, align);
	ut_assert(b != 0);
	if (alloc_size == alloc_size_aligned) {
		ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, ram + ram_size -
			   (alloc_size_aligned * 2), alloc_size * 2, 0, 0, 0,
			   0);
	} else {
		ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 2, ram + ram_size -
			   (alloc_size_aligned * 2), alloc_size, ram + ram_size
			   - alloc_size_aligned, alloc_size, 0, 0);
	}
	/* and free them */
	ret = lmb_free(b, alloc_size);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1,
		   ram + ram_size - alloc_size_aligned,
		   alloc_size, 0, 0, 0, 0);
	ret = lmb_free(a, alloc_size);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 0, 0, 0, 0, 0, 0, 0);

	/* allocate a block with base*/
	b = lmb_alloc_base(alloc_size, align, ram_end);
	ut_assert(a == b);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1,
		   ram + ram_size - alloc_size_aligned,
		   alloc_size, 0, 0, 0, 0);
	/* and free it */
	ret = lmb_free(b, alloc_size);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 0, 0, 0, 0, 0, 0, 0);

	lmb_uninit(mem_lst, used_lst);

	return 0;
}

static int lmb_test_lmb_noreserved_norun(struct unit_test_state *uts)
{
	int ret;

	/* simulate 512 MiB RAM beginning at 1GiB */
	ret = test_noreserved(uts, 0x40000000, 4, 1);
	if (ret)
		return ret;

	/* simulate 512 MiB RAM beginning at 1.5GiB */
	return test_noreserved(uts, 0xE0000000, 4, 1);
}
LMB_TEST(lmb_test_lmb_noreserved_norun, UT_TESTF_MANUAL);

static int lmb_test_lmb_unaligned_size_norun(struct unit_test_state *uts)
{
	int ret;

	/* simulate 512 MiB RAM beginning at 1GiB */
	ret = test_noreserved(uts, 0x40000000, 5, 8);
	if (ret)
		return ret;

	/* simulate 512 MiB RAM beginning at 1.5GiB */
	return test_noreserved(uts, 0xE0000000, 5, 8);
}
LMB_TEST(lmb_test_lmb_unaligned_size_norun, UT_TESTF_MANUAL);

/*
 * Simulate a RAM that starts at 0 and allocate down to address 0, which must
 * fail as '0' means failure for the lmb_alloc functions.
 */
static int lmb_test_lmb_at_0_norun(struct unit_test_state *uts)
{
	const phys_addr_t ram = 0;
	const phys_size_t ram_size = 0x20000000;
	struct alist *mem_lst, *used_lst;
	struct lmb_region *mem, *used;
	long ret;
	phys_addr_t a, b;

	ut_asserteq(lmb_init(&mem_lst, &used_lst), 0);
	mem = mem_lst->data;
	used = used_lst->data;

	ret = lmb_add(ram, ram_size);
	ut_asserteq(ret, 0);

	/* allocate nearly everything */
	a = lmb_alloc(ram_size - 4, 1);
	ut_asserteq(a, ram + 4);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, a, ram_size - 4,
		   0, 0, 0, 0);
	/* allocate the rest */
	/* This should fail as the allocated address would be 0 */
	b = lmb_alloc(4, 1);
	ut_asserteq(b, 0);
	/* check that this was an error by checking lmb */
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, a, ram_size - 4,
		   0, 0, 0, 0);
	/* check that this was an error by freeing b */
	ret = lmb_free(b, 4);
	ut_asserteq(ret, -1);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, a, ram_size - 4,
		   0, 0, 0, 0);

	ret = lmb_free(a, ram_size - 4);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 0, 0, 0, 0, 0, 0, 0);

	lmb_uninit(mem_lst, used_lst);

	return 0;
}
LMB_TEST(lmb_test_lmb_at_0_norun, UT_TESTF_MANUAL);

/* Check that calling lmb_reserve with overlapping regions fails. */
static int lmb_test_lmb_overlapping_reserve_norun(struct unit_test_state *uts)
{
	const phys_addr_t ram = 0x40000000;
	const phys_size_t ram_size = 0x20000000;
	struct alist *mem_lst, *used_lst;
	struct lmb_region *mem, *used;
	long ret;

	ut_asserteq(lmb_init(&mem_lst, &used_lst), 0);
	mem = mem_lst->data;
	used = used_lst->data;

	ret = lmb_add(ram, ram_size);
	ut_asserteq(ret, 0);

	ret = lmb_reserve(0x40010000, 0x10000);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, 0x40010000, 0x10000,
		   0, 0, 0, 0);
	/* allocate overlapping region should return the coalesced count */
	ret = lmb_reserve(0x40011000, 0x10000);
	ut_asserteq(ret, 1);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, 0x40010000, 0x11000,
		   0, 0, 0, 0);
	/* allocate 3nd region */
	ret = lmb_reserve(0x40030000, 0x10000);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 2, 0x40010000, 0x11000,
		   0x40030000, 0x10000, 0, 0);
	/* allocate 2nd region , This should coalesced all region into one */
	ret = lmb_reserve(0x40020000, 0x10000);
	ut_assert(ret >= 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, 0x40010000, 0x30000,
		   0, 0, 0, 0);

	/* allocate 2nd region, which should be added as first region */
	ret = lmb_reserve(0x40000000, 0x8000);
	ut_assert(ret >= 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 2, 0x40000000, 0x8000,
		   0x40010000, 0x30000, 0, 0);

	/* allocate 3rd region, coalesce with first and overlap with second */
	ret = lmb_reserve(0x40008000, 0x10000);
	ut_assert(ret >= 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, 0x40000000, 0x40000,
		   0, 0, 0, 0);

	lmb_uninit(mem_lst, used_lst);

	return 0;
}
LMB_TEST(lmb_test_lmb_overlapping_reserve_norun, UT_TESTF_MANUAL);

/*
 * Simulate 512 MiB RAM, reserve 3 blocks, allocate addresses in between.
 * Expect addresses outside the memory range to fail.
 */
static int test_alloc_addr(struct unit_test_state *uts, const phys_addr_t ram)
{
	struct lmb_region *mem, *used;
	struct alist *mem_lst, *used_lst;
	const phys_size_t ram_size = 0x20000000;
	const phys_addr_t ram_end = ram + ram_size;
	const phys_size_t alloc_addr_a = ram + 0x8000000;
	const phys_size_t alloc_addr_b = ram + 0x8000000 * 2;
	const phys_size_t alloc_addr_c = ram + 0x8000000 * 3;
	long ret;
	phys_addr_t a, b, c, d, e;

	/* check for overflow */
	ut_assert(ram_end == 0 || ram_end > ram);

	ut_asserteq(lmb_init(&mem_lst, &used_lst), 0);
	mem = mem_lst->data;
	used = used_lst->data;

	ret = lmb_add(ram, ram_size);
	ut_asserteq(ret, 0);

	/*  reserve 3 blocks */
	ret = lmb_reserve(alloc_addr_a, 0x10000);
	ut_asserteq(ret, 0);
	ret = lmb_reserve(alloc_addr_b, 0x10000);
	ut_asserteq(ret, 0);
	ret = lmb_reserve(alloc_addr_c, 0x10000);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 3, alloc_addr_a, 0x10000,
		   alloc_addr_b, 0x10000, alloc_addr_c, 0x10000);

	/* allocate blocks */
	a = lmb_alloc_addr(ram, alloc_addr_a - ram);
	ut_asserteq(a, ram);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 3, ram, 0x8010000,
		   alloc_addr_b, 0x10000, alloc_addr_c, 0x10000);
	b = lmb_alloc_addr(alloc_addr_a + 0x10000,
			   alloc_addr_b - alloc_addr_a - 0x10000);
	ut_asserteq(b, alloc_addr_a + 0x10000);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 2, ram, 0x10010000,
		   alloc_addr_c, 0x10000, 0, 0);
	c = lmb_alloc_addr(alloc_addr_b + 0x10000,
			   alloc_addr_c - alloc_addr_b - 0x10000);
	ut_asserteq(c, alloc_addr_b + 0x10000);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, ram, 0x18010000,
		   0, 0, 0, 0);
	d = lmb_alloc_addr(alloc_addr_c + 0x10000,
			   ram_end - alloc_addr_c - 0x10000);
	ut_asserteq(d, alloc_addr_c + 0x10000);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, ram, ram_size,
		   0, 0, 0, 0);

	/* allocating anything else should fail */
	e = lmb_alloc(1, 1);
	ut_asserteq(e, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, ram, ram_size,
		   0, 0, 0, 0);

	ret = lmb_free(d, ram_end - alloc_addr_c - 0x10000);
	ut_asserteq(ret, 0);

	/* allocate at 3 points in free range */

	d = lmb_alloc_addr(ram_end - 4, 4);
	ut_asserteq(d, ram_end - 4);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 2, ram, 0x18010000,
		   d, 4, 0, 0);
	ret = lmb_free(d, 4);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, ram, 0x18010000,
		   0, 0, 0, 0);

	d = lmb_alloc_addr(ram_end - 128, 4);
	ut_asserteq(d, ram_end - 128);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 2, ram, 0x18010000,
		   d, 4, 0, 0);
	ret = lmb_free(d, 4);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, ram, 0x18010000,
		   0, 0, 0, 0);

	d = lmb_alloc_addr(alloc_addr_c + 0x10000, 4);
	ut_asserteq(d, alloc_addr_c + 0x10000);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, ram, 0x18010004,
		   0, 0, 0, 0);
	ret = lmb_free(d, 4);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, ram, 0x18010000,
		   0, 0, 0, 0);

	/* allocate at the bottom */
	ret = lmb_free(a, alloc_addr_a - ram);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, ram + 0x8000000,
		   0x10010000, 0, 0, 0, 0);
	d = lmb_alloc_addr(ram, 4);
	ut_asserteq(d, ram);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 2, d, 4,
		   ram + 0x8000000, 0x10010000, 0, 0);

	/* check that allocating outside memory fails */
	if (ram_end != 0) {
		ret = lmb_alloc_addr(ram_end, 1);
		ut_asserteq(ret, 0);
	}
	if (ram != 0) {
		ret = lmb_alloc_addr(ram - 1, 1);
		ut_asserteq(ret, 0);
	}

	lmb_uninit(mem_lst, used_lst);

	return 0;
}

static int lmb_test_lmb_alloc_addr_norun(struct unit_test_state *uts)
{
	int ret;

	/* simulate 512 MiB RAM beginning at 1GiB */
	ret = test_alloc_addr(uts, 0x40000000);
	if (ret)
		return ret;

	/* simulate 512 MiB RAM beginning at 1.5GiB */
	return test_alloc_addr(uts, 0xE0000000);
}
LMB_TEST(lmb_test_lmb_alloc_addr_norun, UT_TESTF_MANUAL);

/* Simulate 512 MiB RAM, reserve 3 blocks, check addresses in between */
static int test_get_unreserved_size(struct unit_test_state *uts,
				    const phys_addr_t ram)
{
	struct lmb_region *mem, *used;
	struct alist *mem_lst, *used_lst;
	const phys_size_t ram_size = 0x20000000;
	const phys_addr_t ram_end = ram + ram_size;
	const phys_size_t alloc_addr_a = ram + 0x8000000;
	const phys_size_t alloc_addr_b = ram + 0x8000000 * 2;
	const phys_size_t alloc_addr_c = ram + 0x8000000 * 3;
	long ret;
	phys_size_t s;

	/* check for overflow */
	ut_assert(ram_end == 0 || ram_end > ram);

	ut_asserteq(lmb_init(&mem_lst, &used_lst), 0);
	mem = mem_lst->data;
	used = used_lst->data;

	ret = lmb_add(ram, ram_size);
	ut_asserteq(ret, 0);

	/*  reserve 3 blocks */
	ret = lmb_reserve(alloc_addr_a, 0x10000);
	ut_asserteq(ret, 0);
	ret = lmb_reserve(alloc_addr_b, 0x10000);
	ut_asserteq(ret, 0);
	ret = lmb_reserve(alloc_addr_c, 0x10000);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 3, alloc_addr_a, 0x10000,
		   alloc_addr_b, 0x10000, alloc_addr_c, 0x10000);

	/* check addresses in between blocks */
	s = lmb_get_free_size(ram);
	ut_asserteq(s, alloc_addr_a - ram);
	s = lmb_get_free_size(ram + 0x10000);
	ut_asserteq(s, alloc_addr_a - ram - 0x10000);
	s = lmb_get_free_size(alloc_addr_a - 4);
	ut_asserteq(s, 4);

	s = lmb_get_free_size(alloc_addr_a + 0x10000);
	ut_asserteq(s, alloc_addr_b - alloc_addr_a - 0x10000);
	s = lmb_get_free_size(alloc_addr_a + 0x20000);
	ut_asserteq(s, alloc_addr_b - alloc_addr_a - 0x20000);
	s = lmb_get_free_size(alloc_addr_b - 4);
	ut_asserteq(s, 4);

	s = lmb_get_free_size(alloc_addr_c + 0x10000);
	ut_asserteq(s, ram_end - alloc_addr_c - 0x10000);
	s = lmb_get_free_size(alloc_addr_c + 0x20000);
	ut_asserteq(s, ram_end - alloc_addr_c - 0x20000);
	s = lmb_get_free_size(ram_end - 4);
	ut_asserteq(s, 4);

	lmb_uninit(mem_lst, used_lst);

	return 0;
}

static int lmb_test_lmb_get_free_size_norun(struct unit_test_state *uts)
{
	int ret;

	/* simulate 512 MiB RAM beginning at 1GiB */
	ret = test_get_unreserved_size(uts, 0x40000000);
	if (ret)
		return ret;

	/* simulate 512 MiB RAM beginning at 1.5GiB */
	return test_get_unreserved_size(uts, 0xE0000000);
}
LMB_TEST(lmb_test_lmb_get_free_size_norun, UT_TESTF_MANUAL);

static int lmb_test_lmb_flags_norun(struct unit_test_state *uts)
{
	struct lmb_region *mem, *used;
	struct alist *mem_lst, *used_lst;
	const phys_addr_t ram = 0x40000000;
	const phys_size_t ram_size = 0x20000000;
	long ret;

	ut_asserteq(lmb_init(&mem_lst, &used_lst), 0);
	mem = mem_lst->data;
	used = used_lst->data;

	ret = lmb_add(ram, ram_size);
	ut_asserteq(ret, 0);

	/* reserve, same flag */
	ret = lmb_reserve_flags(0x40010000, 0x10000, LMB_NOMAP);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, 0x40010000, 0x10000,
		   0, 0, 0, 0);

	/* reserve again, same flag */
	ret = lmb_reserve_flags(0x40010000, 0x10000, LMB_NOMAP);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, 0x40010000, 0x10000,
		   0, 0, 0, 0);

	/* reserve again, new flag */
	ret = lmb_reserve_flags(0x40010000, 0x10000, LMB_NONE);
	ut_asserteq(ret, -1);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, 0x40010000, 0x10000,
		   0, 0, 0, 0);

	ut_asserteq(lmb_is_nomap(&used[0]), 1);

	/* merge after */
	ret = lmb_reserve_flags(0x40020000, 0x10000, LMB_NOMAP);
	ut_asserteq(ret, 1);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, 0x40010000, 0x20000,
		   0, 0, 0, 0);

	/* merge before */
	ret = lmb_reserve_flags(0x40000000, 0x10000, LMB_NOMAP);
	ut_asserteq(ret, 1);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 1, 0x40000000, 0x30000,
		   0, 0, 0, 0);

	ut_asserteq(lmb_is_nomap(&used[0]), 1);

	ret = lmb_reserve_flags(0x40030000, 0x10000, LMB_NONE);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 2, 0x40000000, 0x30000,
		   0x40030000, 0x10000, 0, 0);

	ut_asserteq(lmb_is_nomap(&used[0]), 1);
	ut_asserteq(lmb_is_nomap(&used[1]), 0);

	/* test that old API use LMB_NONE */
	ret = lmb_reserve(0x40040000, 0x10000);
	ut_asserteq(ret, 1);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 2, 0x40000000, 0x30000,
		   0x40030000, 0x20000, 0, 0);

	ut_asserteq(lmb_is_nomap(&used[0]), 1);
	ut_asserteq(lmb_is_nomap(&used[1]), 0);

	ret = lmb_reserve_flags(0x40070000, 0x10000, LMB_NOMAP);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 3, 0x40000000, 0x30000,
		   0x40030000, 0x20000, 0x40070000, 0x10000);

	ret = lmb_reserve_flags(0x40050000, 0x10000, LMB_NOMAP);
	ut_asserteq(ret, 0);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 4, 0x40000000, 0x30000,
		   0x40030000, 0x20000, 0x40050000, 0x10000);

	/* merge with 2 adjacent regions */
	ret = lmb_reserve_flags(0x40060000, 0x10000, LMB_NOMAP);
	ut_asserteq(ret, 2);
	ASSERT_LMB(mem_lst, used_lst, ram, ram_size, 3, 0x40000000, 0x30000,
		   0x40030000, 0x20000, 0x40050000, 0x30000);

	ut_asserteq(lmb_is_nomap(&used[0]), 1);
	ut_asserteq(lmb_is_nomap(&used[1]), 0);
	ut_asserteq(lmb_is_nomap(&used[2]), 1);

	lmb_uninit(mem_lst, used_lst);

	return 0;
}
LMB_TEST(lmb_test_lmb_flags_norun, UT_TESTF_MANUAL);

int do_ut_lmb(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct unit_test *tests = UNIT_TEST_SUITE_START(lmb_test);
	const int n_ents = UNIT_TEST_SUITE_COUNT(lmb_test);

	return cmd_ut_category("lmb", "lmb_test_", tests, n_ents, argc, argv);
}