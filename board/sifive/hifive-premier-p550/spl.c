// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2020-2021 SiFive, Inc
 *
 * Authors:
 *   Pragnesh Patel <pragnesh.patel@sifive.com>
 */

#include <image.h>
#include <asm/spl.h>

int board_fdt_blob_setup(void **fdtp)
{
	return -EEXIST;
}

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	/* boot using first FIT config */
	return 0;
}
#endif

u32 spl_boot_device(void)
{
	return BOOT_DEVICE_UART;
}
