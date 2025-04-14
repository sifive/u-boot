// SPDX-License-Identifier: GPL-2.0+

#include <clk-uclass.h>
#include <dm.h>
#include <asm/io.h>
#include <dm/device-internal.h>

#include "clk-eswin.h"

static struct clk *
eswin_clk_get_current_parent(const struct clk *clk)
{
	struct clk *current_parents = dev_get_priv(clk->dev);

	return &current_parents[clk->id];
}

static const struct eswin_clk_data *
eswin_clk_get_data(const struct eswin_clk_plat *plat, const struct clk *clk)
{
	const struct eswin_clk_desc *desc = plat->desc;

	if (clk->id >= desc->num_clks)
		return NULL;

	return &desc->clks[clk->id];
}

static int eswin_clk_request(struct clk *clk)
{
	const struct eswin_clk_plat *plat = dev_get_plat(clk->dev);

	return eswin_clk_get_data(plat, clk) ? 0 : -EINVAL;
}

static u32 eswin_clk_count_field(u32 mask)
{
	if (!mask)
		return 1;

	return mask / (mask & -mask) + 1;
}

static u32 eswin_clk_get_field(const void __iomem *reg, u32 mask)
{
	if (!mask)
		return 0;

	return (readl(reg) & mask) / (mask & -mask);
}

static void eswin_clk_set_field(void __iomem *reg, u32 mask, u32 val)
{
	if (!mask)
		return;

	clrsetbits_le32(reg, mask, val * (mask & -mask));
}

static int eswin_clk_resolve_parent(struct udevice *dev, const struct eswin_clk_data *data,
				    u32 sel, struct clk *out)
{
	const struct eswin_clk_plat *plat = dev_get_plat(dev);
	struct clk parent = {};
	u8 id;

	/* Zero is a valid clock ID, so exclude uninitialized clocks by checking the name */
	if (!data->name)
		return -ENOENT;

	id = data->parents[sel];
	if (id == NO_PARENT) {
		return -ENOENT;
	} else if (id < FW_PARENT_BASE) {
		parent.dev = dev;
		parent.id = id;
	} else if (id < FW_PARENT_BASE + plat->desc->num_fw_parents) {
		const char *name = plat->desc->fw_parents[id - FW_PARENT_BASE];
		int ret;

		if (name)
			ret = clk_get_by_name(dev, name, &parent);
		else
			ret = clk_get_by_index(dev, id - FW_PARENT_BASE, &parent);
		if (ret)
			return ret;
	} else {
		return -EINVAL;
	}

	*out = parent;

	return 0;
}

static ulong eswin_clk_calc_rate(struct clk *clk, ulong req, u32 *best_sel, u32 *best_div)
{
	const struct eswin_clk_plat *plat = dev_get_plat(clk->dev);
	const struct eswin_clk_data *data = eswin_clk_get_data(plat, clk);
	u32 num_divs = eswin_clk_count_field(data->div_mask);
	u32 num_sels = eswin_clk_count_field(data->sel_mask);
	ulong best_rate = 0;

	for (u32 sel = 0; sel < num_sels; ++sel) {
		struct clk parent;
		ulong parent_rate;
		int ret;

		ret = eswin_clk_resolve_parent(clk->dev, data, sel, &parent);
		if (ret)
			continue;

		parent_rate = clk_get_rate(&parent);
		if (IS_ERR_VALUE(parent_rate))
			continue;

		if (data->fixed_div)
			parent_rate /= data->fixed_div;

		for (u32 div = 1; div < num_divs; ++div) {
			ulong rate = parent_rate / div;

			if (rate > req || rate <= best_rate)
				continue;

			*best_sel = sel;
			*best_div = div;
			best_rate = rate;
		}
	}

	return best_rate;
}

static ulong eswin_clk_round_rate(struct clk *clk, ulong req)
{
	u32 sel, div;

	return eswin_clk_calc_rate(clk, req, &sel, &div);
}

static ulong eswin_clk_get_rate(struct clk *clk)
{
	const struct eswin_clk_plat *plat = dev_get_plat(clk->dev);
	const struct eswin_clk_data *data = eswin_clk_get_data(plat, clk);
	ulong rate;
	u32 div;

	if (data->rate)
		return data->rate;

	rate = clk_get_parent_rate(clk);
	if (IS_ERR_VALUE(rate))
		return rate;

	if (data->fixed_div)
		rate /= data->fixed_div;

	div = eswin_clk_get_field(plat->base + data->div_reg, data->div_mask);
	if (!div)
		div = 1;

	return rate / div;
}

static ulong eswin_clk_set_rate(struct clk *clk, ulong req)
{
	const struct eswin_clk_plat *plat = dev_get_plat(clk->dev);
	const struct eswin_clk_data *data = eswin_clk_get_data(plat, clk);
	u32 sel, div;
	ulong rate;

	rate = eswin_clk_calc_rate(clk, req, &sel, &div);
	if (!rate)
		return rate;

	debug("%s(%s, %lu)\n", __func__, data->name, rate);

	eswin_clk_set_field(plat->base + data->sel_reg, data->sel_mask, sel);
	eswin_clk_set_field(plat->base + data->div_reg, data->div_mask, div);

	return 0;
}

static struct clk *eswin_clk_get_parent(struct clk *clk)
{
	const struct eswin_clk_plat *plat = dev_get_plat(clk->dev);
	const struct eswin_clk_data *data = eswin_clk_get_data(plat, clk);
	struct clk *current_parent = eswin_clk_get_current_parent(clk);

	if (!clk_valid(current_parent)) {
		u32 sel = eswin_clk_get_field(plat->base + data->sel_reg, data->sel_mask);
		int ret;

		ret = eswin_clk_resolve_parent(clk->dev, data, sel, current_parent);
		if (ret)
			return ERR_PTR(ret);
	}

	return current_parent;
}

static int eswin_clk_set_parent(struct clk *clk, struct clk *parent)
{
	const struct eswin_clk_plat *plat = dev_get_plat(clk->dev);
	const struct eswin_clk_data *data = eswin_clk_get_data(plat, clk);
	struct clk *current_parent = eswin_clk_get_current_parent(clk);
	u32 num_sels = eswin_clk_count_field(data->sel_mask), sel;
	struct clk p;
	int ret;

	/* Find the selector value corresponding to the requested parent. */
	for (sel = 0; sel < num_sels; ++sel) {
		ret = eswin_clk_resolve_parent(clk->dev, data, sel, &p);
		if (!ret && p.dev == parent->dev && p.id == parent->id)
			break;
	}
	if (sel == num_sels)
		return -ENOENT;

	debug("%s(%s, %d)\n", __func__, data->name, sel);

	eswin_clk_set_field(plat->base + data->sel_reg, data->sel_mask, sel);

	*current_parent = p;

	return 0;
}

static int eswin_clk_set_gate(struct clk *clk, bool enable)
{
	const struct eswin_clk_plat *plat = dev_get_plat(clk->dev);
	const struct eswin_clk_data *data = eswin_clk_get_data(plat, clk);

	debug("%s(%s, %s)\n", __func__, data->name, enable ? "enable" : "disable");

	clrsetbits_le32(plat->base + data->en_reg,
			data->en_mask, enable ? data->en_mask : 0);

	return 0;
}

static int eswin_clk_enable(struct clk *clk)
{
	struct clk *parent;
	int ret;

	parent = eswin_clk_get_parent(clk);
	if (clk_valid(parent)) {
		ret = clk_enable(parent);
		if (ret)
			return ret;
	}

	return eswin_clk_set_gate(clk, true);
}

static int eswin_clk_disable(struct clk *clk)
{
	return eswin_clk_set_gate(clk, false);
}

#if IS_ENABLED(CONFIG_CMD_CLK)
static void eswin_clk_dump(struct udevice *dev)
{
	const struct eswin_clk_plat *plat = dev_get_plat(dev);
	const struct eswin_clk_desc *desc = plat->desc;
	struct clk clk, *parent;

	printf(" ID            NAME                      PARENT              RATE    SEL DIV EN\n");
	printf("---+--------------------------+--------------------------+----------+---+---+--\n");

	clk.dev = dev;
	for (size_t id = 0; id < desc->num_clks; ++id) {
		const struct eswin_clk_data *data = &desc->clks[id];
		const char *parent_name;

		if (!data->name)
			continue;

		clk.id = id;
		parent = clk_get_parent(&clk);

		if (IS_ERR(parent))
			parent_name = "(none)";
		else if (parent->dev->driver != dev->driver)
			parent_name = parent->dev->name;
		else {
			const struct eswin_clk_plat *parent_plat;
			const struct eswin_clk_data *parent_data;

			parent_plat = dev_get_plat(parent->dev);
			parent_data = eswin_clk_get_data(parent_plat, parent);
			parent_name = parent_data->name;
			if (!parent_name)
				parent_name = "(null)";
		}

		printf("%3zd %26s %26s %10ld",
		       id, data->name, parent_name, clk_get_rate(&clk));
		if (data->sel_mask)
			printf(" %3d", eswin_clk_get_field(plat->base + data->sel_reg,
							   data->sel_mask));
		else if (data->div_mask || data->en_mask)
			puts("    ");
		if (data->div_mask)
			printf(" %3d", eswin_clk_get_field(plat->base + data->div_reg,
							   data->div_mask));
		else if (data->en_mask)
			puts("    ");
		if (data->en_mask)
			printf(" %2d", eswin_clk_get_field(plat->base + data->en_reg,
							   data->en_mask));
		puts("\n");
	}
	puts("\n");
}
#endif

static struct clk_ops eswin_clk_ops = {
	.request	= eswin_clk_request,
	.round_rate	= eswin_clk_round_rate,
	.get_rate	= eswin_clk_get_rate,
	.set_rate	= eswin_clk_set_rate,
	.get_parent	= eswin_clk_get_parent,
	.set_parent	= eswin_clk_set_parent,
	.enable		= eswin_clk_enable,
	.disable	= eswin_clk_disable,
#if IS_ENABLED(CONFIG_CMD_CLK)
	.dump		= eswin_clk_dump,
#endif
};

static int eswin_clk_probe(struct udevice *dev)
{
	const struct eswin_clk_plat *plat = dev_get_plat(dev);
	const struct eswin_clk_desc *desc = plat->desc;
	struct clk *current_parents;
	int ret;

	current_parents = calloc(sizeof(struct clk), plat->desc->num_clks);
	if (!current_parents)
		return -ENOMEM;

	dev_set_priv(dev, current_parents);

	if (desc->init) {
		ret = desc->init(dev);
		if (ret)
			return ret;
	}

	return 0;
}

static int eswin_clk_of_to_plat(struct udevice *dev)
{
	struct eswin_clk_plat *plat = dev_get_plat(dev);

	plat->base = dev_read_addr_ptr(dev);
	if (!plat->base)
		return -ENOMEM;

	plat->desc = (const struct eswin_clk_desc *)dev_get_driver_data(dev);
	if (!plat->desc)
		return -EINVAL;

	return 0;
}

extern const struct eswin_clk_desc eic7700_clk_desc;

static const struct udevice_id eswin_clk_ids[] = {
#if IS_ENABLED(CONFIG_CLK_ESWIN_EIC7700)
	{
		.compatible = "eswin,eic7700-clock",
		.data = (ulong)&eic7700_clk_desc,
	},
#endif
	{ }
};


U_BOOT_DRIVER(eswin_clk) = {
	.name		= "eswin_clk",
	.id		= UCLASS_CLK,
	.of_match	= eswin_clk_ids,
	.probe		= eswin_clk_probe,
	.of_to_plat	= eswin_clk_of_to_plat,
	.plat_auto	= sizeof(struct eswin_clk_plat),
	.ops		= &eswin_clk_ops,
};
