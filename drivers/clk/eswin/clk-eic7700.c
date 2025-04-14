// SPDX-License-Identifier: GPL-2.0+

#include <compiler.h>
#include <dt-bindings/clock/eswin,eic7700-clock.h>
#include <linux/bitops.h>

#include "clk-eswin.h"

enum {
	FW_XTAL24M		= FW_PARENT_BASE,

	FW_PARENT_MAX,
};

static const char *const eic7700_fw_parents[FW_PARENT_MAX-FW_PARENT_BASE] = {
	NULL,
};

static const struct eswin_clk_data eic7700_clks[] = {
	ESWIN_GATE(EIC7700_CLK_GATE_HSP_ACLK,		0x148, 31, FW_XTAL24M),
	ESWIN_GATE(EIC7700_CLK_GATE_HSP_CFG_CLK,	0x14c, 31, FW_XTAL24M),
	ESWIN_GATE(EIC7700_CLK_GATE_HSP_ETH0_CORE_CLK,	0x158,  0, FW_XTAL24M),
	ESWIN_GATE(EIC7700_CLK_GATE_HSP_RMII_REF_0,	0x158, 31, FW_XTAL24M),
	ESWIN_GATE(EIC7700_CLK_GATE_HSP_ETH1_CORE_CLK,	0x15c,  0, FW_XTAL24M),
	ESWIN_GATE(EIC7700_CLK_GATE_HSP_RMII_REF_1,	0x15c, 31, FW_XTAL24M),
};

const struct eswin_clk_desc eic7700_clk_desc = {
	.clks		= eic7700_clks,
	.fw_parents	= eic7700_fw_parents,
	.num_clks	= ARRAY_SIZE(eic7700_clks),
	.num_fw_parents	= ARRAY_SIZE(eic7700_fw_parents),
};
