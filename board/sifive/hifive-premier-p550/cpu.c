// SPDX-License-Identifier: GPL-2.0

#include <asm/io.h>

void *map_physmem(phys_addr_t paddr, unsigned long len, unsigned long flags)
{
	void *vaddr = phys_to_virt(paddr);

	if (flags == MAP_NOCACHE && (uintptr_t)vaddr >= 0x80000000UL)
		vaddr += 0xbf80000000UL;

	return vaddr;
}

void unmap_physmem(void *vaddr, unsigned long flags)
{
}
