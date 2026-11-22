/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef GPU_UNLOCK_DEVICE_H
#define GPU_UNLOCK_DEVICE_H

struct pci_dev;
struct gu_card;

/* pci_dev_lock() is only exported from 5.16. Older trees need the two locks it
 * takes, in the same order: config access stopped, then the device lock, which
 * is what every transaction here holds. Ubuntu exports it in its own 5.15,
 * which is why the 5.15 build in CI never showed this.
 *
 * The host tests compile pm.c with their own pci_dev and their own lock, so
 * they declare the pair instead of building one out of kernel calls.
 */
#ifdef __KERNEL__
#include <linux/pci.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
static inline void gu_pci_dev_lock(struct pci_dev *pdev)
{
	pci_cfg_access_lock(pdev);
	device_lock(&pdev->dev);
}

static inline void gu_pci_dev_unlock(struct pci_dev *pdev)
{
	device_unlock(&pdev->dev);
	pci_cfg_access_unlock(pdev);
}
#else
static inline void gu_pci_dev_lock(struct pci_dev *pdev)
{
	pci_dev_lock(pdev);
}

static inline void gu_pci_dev_unlock(struct pci_dev *pdev)
{
	pci_dev_unlock(pdev);
}
#endif
#else
void gu_pci_dev_lock(struct pci_dev *pdev);
void gu_pci_dev_unlock(struct pci_dev *pdev);
#endif

/* Resolved once during enumeration. The coordinator owns the PCI reference.
 * Firmware files and launch procedure come from recipe->family. Round-scoped
 * firmware assets are never stored here. */
struct gu_device {
	struct pci_dev *pdev;
	const struct gu_card *recipe;
};

#endif
