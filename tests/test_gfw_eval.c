// SPDX-License-Identifier: GPL-2.0-only
/*
 * Host check for the pure evaluator, built from the shipped source, which
 * compiles for userspace too:
 *
 * cc -std=c11 -Wall -Wextra -Werror -I kernel/gpu_unlock \
 *    kernel/gpu_unlock/gfw_eval.c tests/test_gfw_eval.c -o /tmp/test_gfw_eval
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>

#include "gfw_eval.h"

int main(void)
{
	struct gpu_unlock_gfw_sample sample = {
		.config_ok = true,
		.plm = GU_GFW_PLM_LEVEL0,
		.boot = GU_GFW_BOOT_COMPLETED,
		.cpuctl = GU_GFW_CPUCTL_HALTED,
	};

	assert(gpu_unlock_gfw_assess(&sample) == 0);
	sample.cpuctl = 0;
	assert(gpu_unlock_gfw_assess(&sample) == -EAGAIN);
	sample.cpuctl = GU_GFW_CPUCTL_HALTED;
	sample.plm = 0;
	assert(gpu_unlock_gfw_assess(&sample) == -EAGAIN);
	sample.plm = GU_GFW_PLM_LEVEL0;
	sample.boot = 0xfe;
	assert(gpu_unlock_gfw_assess(&sample) == -EAGAIN);
	sample.boot = (u32)~0U;
	sample.plm = (u32)~0U;
	sample.cpuctl = (u32)~0U;
	assert(gpu_unlock_gfw_assess(&sample) == -ENODEV);
	sample.boot = 0xbadf00ff;
	sample.plm = 0xbadf0001;
	sample.cpuctl = 0xbadf0010;
	assert(gpu_unlock_gfw_assess(&sample) == -EAGAIN);
	sample.boot = GU_GFW_BOOT_COMPLETED;
	sample.plm = GU_GFW_PLM_LEVEL0;
	sample.cpuctl = GU_GFW_CPUCTL_HALTED;
	/* One inaccessible sample is enough: a poll must not accept a read that
	 * only happens to carry the completed, halted, level-0 bits. */
	sample.boot = 0xbadf00ff;
	assert(gpu_unlock_gfw_assess(&sample) == -EAGAIN);
	sample.boot = GU_GFW_BOOT_COMPLETED;
	sample.plm = 0xbadf0001;
	assert(gpu_unlock_gfw_assess(&sample) == -EAGAIN);
	sample.plm = GU_GFW_PLM_LEVEL0;
	sample.cpuctl = 0xbadf0010;
	assert(gpu_unlock_gfw_assess(&sample) == -EAGAIN);
	sample.cpuctl = GU_GFW_CPUCTL_HALTED;
	assert(gpu_unlock_gfw_assess(&sample) == 0);
	assert(gpu_unlock_gfw_assess(NULL) == -ENODEV);
	sample.config_ok = false;
	assert(gpu_unlock_gfw_assess(&sample) == -ENODEV);
	sample.config_ok = true;

	assert(gpu_unlock_command_restore(0, GU_PCI_COMMAND_MEMORY |
					  GU_PCI_COMMAND_MASTER) == 0);
	assert(gpu_unlock_command_restore(GU_PCI_COMMAND_MEMORY,
					  GU_PCI_COMMAND_MEMORY |
					  GU_PCI_COMMAND_MASTER) ==
	       GU_PCI_COMMAND_MEMORY);
	assert(gpu_unlock_command_restore(GU_PCI_COMMAND_IO,
					  GU_PCI_COMMAND_IO | GU_PCI_COMMAND_MEMORY) ==
	       GU_PCI_COMMAND_IO);

	/* The state field is the low two bits: 0b00 is D0, 0b01 D1, 0b11 D3hot. */
	assert(gpu_unlock_pmcsr_d0(0x0008u) == true);
	assert(gpu_unlock_pmcsr_d0(0x0009u) == false);
	assert(gpu_unlock_pmcsr_d0(0x000bu) == false);
	assert(gpu_unlock_pmcsr_d0(0x000cu) == true);

	puts("gfw predicate and command restore checks passed");
	return 0;
}
