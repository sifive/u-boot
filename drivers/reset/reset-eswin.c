// SPDX-License-Identifier: GPL-2.0+

#include <dm.h>
#include <reset-uclass.h>
#include <asm/io.h>
#include <linux/bitops.h>

struct eswin_reset_data {
	u16	reg;
	u8	bit;
	u8	valid;
};

#define ESWIN_RESET(_reg, _bit) { .reg = _reg, .bit = _bit, .valid = true }

struct eswin_reset_desc {
	const struct eswin_reset_data	*resets;
	u8				num_resets;
	bool				active_high;
};

struct eswin_reset_plat {
	void __iomem			*base;
	const struct eswin_reset_desc	*desc;
};

static const struct eswin_reset_data *eswin_reset_get_data(const struct eswin_reset_plat *plat,
							   const struct reset_ctl *reset_ctl)
{
	const struct eswin_reset_desc *desc = plat->desc;
	const struct eswin_reset_data *data;

	if (reset_ctl->id >= desc->num_resets)
		return NULL;

	data = &desc->resets[reset_ctl->id];
	if (!data->valid)
		return NULL;

	return data;
}

static int eswin_reset_request(struct reset_ctl *reset_ctl)
{
	const struct eswin_reset_plat *plat = dev_get_plat(reset_ctl->dev);

	return eswin_reset_get_data(plat, reset_ctl) ? 0 : -EINVAL;
}

static int eswin_reset_set(struct reset_ctl *reset_ctl, bool assert)
{
	const struct eswin_reset_plat *plat = dev_get_plat(reset_ctl->dev);
	const struct eswin_reset_data *data = eswin_reset_get_data(plat, reset_ctl);
	u32 mask = BIT(data->bit);

	debug("%s(%s.%ld, %s)\n", __func__, reset_ctl->dev->name, reset_ctl->id,
	      assert ? "assert" : "deassert");

	clrsetbits_le32(plat->base + data->reg, mask,
			assert ^ plat->desc->active_high ? 0 : mask);

	return 0;
}

static int eswin_reset_assert(struct reset_ctl *reset_ctl)
{
	return eswin_reset_set(reset_ctl, true);
}

static int eswin_reset_deassert(struct reset_ctl *reset_ctl)
{
	return eswin_reset_set(reset_ctl, false);
}

static int eswin_reset_status(struct reset_ctl *reset_ctl)
{
	const struct eswin_reset_plat *plat = dev_get_plat(reset_ctl->dev);
	const struct eswin_reset_data *data = eswin_reset_get_data(plat, reset_ctl);
	u32 mask = BIT(data->bit);

	return !!(readl(plat->base + data->reg) & mask);
}

static const struct reset_ops eswin_reset_ops = {
	.request	= eswin_reset_request,
	.rst_assert	= eswin_reset_assert,
	.rst_deassert	= eswin_reset_deassert,
	.rst_status	= eswin_reset_status,
};

static const struct udevice_id eswin_reset_ids[] = {
	{ }
};

static int eswin_reset_of_to_plat(struct udevice *dev)
{
	struct eswin_reset_plat *plat = dev_get_plat(dev);

	plat->base = dev_read_addr_ptr(dev);
	if (!plat->base)
		return -ENOMEM;

	plat->desc = (const struct eswin_reset_desc *)dev_get_driver_data(dev);
	if (!plat->desc)
		return -EINVAL;

	return 0;
}

U_BOOT_DRIVER(eswin_reset) = {
	.name		= "eswin_reset",
	.id		= UCLASS_RESET,
	.of_match	= eswin_reset_ids,
	.of_to_plat	= eswin_reset_of_to_plat,
	.plat_auto	= sizeof(struct eswin_reset_plat),
	.ops		= &eswin_reset_ops,
};
