// SPDX-License-Identifier: GPL-2.0-only
/*
 * Host check for the GFW sample validator: build it against the shipped
 * source, which compiles for userspace too.
 *
 * cc -std=c11 -Wall -Wextra -Werror -I kernel/gpu_unlock \
 *    kernel/gpu_unlock/gfw_eval.c tests/test_gfw_eval.c -o /tmp/test_gfw_eval
 * /tmp/test_gfw_eval
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>

#include "gfw_eval.h"

static int complete(const u32 plm, const u32 boot, const u32 cpuctl)
{
	const struct gpu_unlock_gfw_sample sample = {
		.config_ok = true, .plm = plm, .boot = boot, .cpuctl = cpuctl,
	};

	return gpu_unlock_gfw_assess(&sample);
}

int main(void)
{
	const u32 done = GU_GFW_BOOT_COMPLETED, halted = GU_GFW_CPUCTL_HALTED;
	const u32 level0 = GU_GFW_PLM_LEVEL0;

	assert(complete(level0, done, halted) == 0);
	assert(complete(level0, done, halted | 0x1u) == 0);
	assert(complete(level0, done, 0u) == -EAGAIN);
	assert(complete(0u, done, halted) == -EAGAIN);
	assert(complete(level0, done ^ 0xffu, halted) == -EAGAIN);
	assert(complete(level0, done, halted) == 0);

	/* A powered-down engine reads all ones and a Falcon PRI error reads back
	 * 0xbadfxxxx; both carry the level-0, completed and halted bits. */
	assert(complete(~0u, done, halted) == -ENODEV);
	assert(complete(0xbadf0001u, 0xbadf00ffu, 0xbadf0010u) == -ENODEV);
	assert(complete(0xbadf0001u, done, halted) == -ENODEV);
	assert(complete(level0, 0xbadf00ffu, halted) == -ENODEV);
	assert(complete(level0, done, 0xbadf0010u) == -ENODEV);

	{
		const struct gpu_unlock_gfw_sample off = {
			.config_ok = false, .plm = level0, .boot = done, .cpuctl = halted,
		};

		assert(gpu_unlock_gfw_assess(&off) == -ENODEV);
		assert(gpu_unlock_gfw_assess(NULL) == -ENODEV);
	}

	/* Memory and master come back from the saved value; every other command
	 * bit stays as the card has it. */
	assert(gpu_unlock_command_restore(0x0004u, 0x0002u) == 0x0004u);
	assert(gpu_unlock_command_restore(0x0006u, 0x0400u) == 0x0406u);
	assert(gpu_unlock_pmcsr_d0(0x0008u) == true);
	assert(gpu_unlock_pmcsr_d0(0x000bu) == false);

	printf("gfw_eval: ok\n");
	return 0;
}
