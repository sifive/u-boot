// SPDX-License-Identifier: GPL-2.0+

#ifndef _CLK_ESWIN_H_
#define _CLK_ESWIN_H_

/* Boundary between internal specifier numbers and OF consumer IDs */
#define FW_PARENT_BASE			0xc0

/* Used for gaps in selector values */
#define NO_PARENT			0xff

#define MAX_PARENTS			4

struct udevice;

struct eswin_clk_data {
	const char	*name;
	const u8 	parents[MAX_PARENTS];
	ulong		rate;
	u32		sel_mask;
	u32		div_mask;
	u32		en_mask;
	u16		sel_reg;
	u16		div_reg;
	u16		en_reg;
	u16 		fixed_div;
};

struct eswin_clk_desc {
	int				(*init)(struct udevice *dev);
	const struct eswin_clk_data	*clks;
	const char *const		*fw_parents;
	u8				num_clks;
	u8				num_fw_parents;
};

struct eswin_clk_plat {
	void __iomem			*base;
	const struct eswin_clk_desc	*desc;
};

#define ESWIN_PLL(_id, _parent, _rate)			\
	[_id] = {					\
		.name		= #_id,			\
		.parents	= { _parent },		\
		.rate		= _rate,		\
	}

#define ESWIN_FIXED_DIV(_id, _parent, _div)		\
	[_id] = {					\
		.name		= #_id,			\
		.parents	= { _parent },		\
		.fixed_div	= _div,			\
	}

#define ESWIN_GATE(_id, _en_reg, _en_bit, _parent)	\
	[_id] = {					\
		.name		= #_id,			\
		.en_reg		= _en_reg,		\
		.en_mask	= BIT(_en_bit),		\
		.parents	= { _parent },		\
	}

#define ESWIN_GATE_SEL(_id, _en_reg, _en_bit, _sel_reg, _sel_mask, ...)	\
	[_id] = {					\
		.name		= #_id,			\
		.en_reg		= _en_reg,		\
		.en_mask	= BIT(_en_bit),		\
		.sel_reg	= _sel_reg,		\
		.sel_mask	= _sel_mask,		\
		.parents	= { __VA_ARGS__ },	\
	}


#define ESWIN_SEL(_id, _sel_reg, _sel_mask, ...)	\
	[_id] = {					\
		.name		= #_id,			\
		.sel_reg	= _sel_reg,		\
		.sel_mask	= _sel_mask,		\
		.parents	= { __VA_ARGS__ },	\
	}

#endif /* _CLK_ESWIN_H_ */
