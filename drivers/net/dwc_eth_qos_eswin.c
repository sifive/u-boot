// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 StarFive Technology Co., Ltd.
 * Author: Yanhong Wang<yanhong.wang@starfivetech.com>
 */

#include <clk.h>
#include <dm.h>
#include <eth_phy.h>
#include <net.h>
#include <regmap.h>
#include <reset.h>
#include <syscon.h>
#include <asm/io.h>
#include <linux/bitfield.h>

#include "dwc_eth_qos.h"

#define ETH_TX_CLK_SEL			BIT(16)
#define ETH_PHY_INTF_SEL		BIT(0)

#define ETH_CSYSREQ_VAL			BIT(0)

#define ETH_TX_ADJ_DELAY		GENMASK(14, 8)
#define ETH_RX_ADJ_DELAY		GENMASK(30, 24)

struct eswin_platform_data {
	struct clk_bulk clks;
	struct reset_ctl_bulk resets;
	struct regmap *regmap;
	phy_interface_t phy_interface;
	u32 phy_ctrl_offset;
	u32 axi_lp_ctrl_offset;
	u32 txd_offset;
	u32 delay_offset;
	u32 rxd_offset;
	u32 delay;
};

static int eqos_probe_resources_eswin(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct eswin_platform_data *data;
	struct ofnode_phandle_args args;
	u32 delay;
	int ret;

	ret = eqos_get_base_addr_dt(dev);
	if (ret) {
		pr_err("eqos_get_base_addr_dt failed: %d\n", ret);
		return ret;
	}

	data = calloc(1, sizeof(struct eswin_platform_data));
	if (!data)
		return -ENOMEM;

	pdata->priv_pdata = data;

	data->phy_interface = eqos->config->interface(dev);
	if (data->phy_interface == PHY_INTERFACE_MODE_NA) {
		pr_err("Invalid PHY interface\n");
		return -EINVAL;
	}

	ret = dev_read_phandle_with_args(dev, "eswin,hsp-sp-csr", NULL, 5, 0, &args);
	if (ret)
		return ret;

	data->regmap = syscon_node_to_regmap(args.node);
	if (IS_ERR(data->regmap)) {
		ret = PTR_ERR(data->regmap);
		pr_err("Failed to get regmap: %d\n", ret);
		return ret;
	}

	data->phy_ctrl_offset = args.args[0];
	data->axi_lp_ctrl_offset = args.args[1];
	data->txd_offset = args.args[2];
	data->delay_offset = args.args[3];
	data->rxd_offset = args.args[4];

	ret = dev_read_u32(dev, "rx-internal-delay-ps", &delay);
	if (!ret)
		data->delay |= FIELD_PREP(ETH_RX_ADJ_DELAY, delay / 20);

	ret = dev_read_u32(dev, "tx-internal-delay-ps", &delay);
	if (!ret)
		data->delay |= FIELD_PREP(ETH_TX_ADJ_DELAY, delay / 20);

	ret = clk_get_by_name(dev, "stmmaceth", &eqos->clk_tx);
	if (ret)
		return ret;

	ret = clk_get_bulk(dev, &data->clks);
	if (ret)
		return ret;

	ret = reset_get_bulk(dev, &data->resets);
	if (ret && ret != -ENOENT)
		return ret;

	return 0;
}

static int eqos_remove_resources_eswin(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct eswin_platform_data *data = pdata->priv_pdata;

	reset_release_bulk(&data->resets);
	clk_release_bulk(&data->clks);

	return 0;
}

static int eqos_stop_resets_eswin(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct eswin_platform_data *data = pdata->priv_pdata;

	return reset_assert_bulk(&data->resets);
}

static int eqos_start_resets_eswin(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct eswin_platform_data *data = pdata->priv_pdata;

	return reset_deassert_bulk(&data->resets);
}

static int eqos_stop_clks_eswin(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct eswin_platform_data *data = pdata->priv_pdata;

	return clk_disable_bulk(&data->clks);
}

static int eqos_start_clks_eswin(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct eswin_platform_data *data = pdata->priv_pdata;
	int ret;

	ret = clk_enable_bulk(&data->clks);
	if (ret)
		return ret;

	regmap_set_bits(data->regmap, data->phy_ctrl_offset,
			ETH_TX_CLK_SEL | ETH_PHY_INTF_SEL);
	regmap_write(data->regmap, data->axi_lp_ctrl_offset,
		     ETH_CSYSREQ_VAL);
	regmap_write(data->regmap, data->txd_offset, 0);
	regmap_write(data->regmap, data->rxd_offset, 0);

	return 0;
}

static int eqos_set_tx_clk_speed_eswin(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct eswin_platform_data *data = pdata->priv_pdata;
	ulong rate;
	int ret;

	switch (eqos->phy->speed) {
	case SPEED_1000:
		rate = 125 * 1000 * 1000;
		break;
	case SPEED_100:
		rate = 25 * 1000 * 1000;
		break;
	case SPEED_10:
		rate = 2.5 * 1000 * 1000;
		break;
	default:
		return -EINVAL;
	}

	ret = clk_set_rate(&eqos->clk_tx, rate);
	if (ret < 0)
		return ret;

	regmap_write(data->regmap, data->delay_offset, data->delay);

	return 0;
}

static ulong eqos_get_tick_clk_rate_eswin(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	return clk_get_rate(&eqos->clk_tx);
}

static struct eqos_ops eqos_eswin_ops = {
	.eqos_inval_desc = eqos_inval_desc_generic,
	.eqos_flush_desc = eqos_flush_desc_generic,
	.eqos_inval_buffer = eqos_inval_buffer_generic,
	.eqos_flush_buffer = eqos_flush_buffer_generic,
	.eqos_probe_resources = eqos_probe_resources_eswin,
	.eqos_remove_resources = eqos_remove_resources_eswin,
	.eqos_stop_resets = eqos_stop_resets_eswin,
	.eqos_start_resets = eqos_start_resets_eswin,
	.eqos_stop_clks = eqos_stop_clks_eswin,
	.eqos_start_clks = eqos_start_clks_eswin,
	.eqos_calibrate_pads = eqos_null_ops,
	.eqos_disable_calibration = eqos_null_ops,
	.eqos_set_tx_clk_speed = eqos_set_tx_clk_speed_eswin,
	.eqos_get_enetaddr = eqos_null_ops,
	.eqos_get_tick_clk_rate = eqos_get_tick_clk_rate_eswin
};

struct eqos_config __maybe_unused eqos_eswin_config = {
	.reg_access_always_ok = false,
	.mdio_wait = 10000,
	.swr_wait = 50,
	.config_mac = EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_DCB,
	.config_mac_mdio = EQOS_MAC_MDIO_ADDRESS_CR_250_300,
	.axi_bus_width = EQOS_AXI_WIDTH_32,
	.interface = dev_read_phy_mode,
	.ops = &eqos_eswin_ops
};
