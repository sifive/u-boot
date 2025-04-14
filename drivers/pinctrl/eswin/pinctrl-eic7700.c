// SPDX-License-Identifier: GPL-2.0

#define DEBUG
#include <asm/io.h>
#include <dm.h>
#include <dm/pinctrl.h>
#include <errno.h>
#include <linux/bitfield.h>
#include <power/regulator.h>

#define EIC7700_PIN_REG(i)		(4 * (i))
#define EIC7700_IE			BIT(0)
#define EIC7700_PU			BIT(1)
#define EIC7700_PD			BIT(2)
#define EIC7700_DS			GENMASK(6, 3)
#define EIC7700_ST			BIT(7)
#define EIC7700_FUNC_SEL		GENMASK(18, 16)

#define EIC7700_BIAS			(EIC7700_PD | EIC7700_PU)

#define EIC7700_RGMII0_SEL_MODE		(0x310 - 0x80)
#define EIC7700_RGMII1_SEL_MODE		(0x314 - 0x80)
#define EIC7700_MS			GENMASK(1, 0)
#define EIC7700_MS_3V3			0x0
#define EIC7700_MS_1V8			0x3

#define EIC7700_FUNCTIONS_PER_PIN	8

struct eic7700_pin {
	const char *name;
	u8 functions[EIC7700_FUNCTIONS_PER_PIN];
};

struct eic7700_plat {
	void __iomem *base;
	struct udevice *rgmii0_supply;
	struct udevice *rgmii1_supply;
};

enum {
	F_DISABLED,
	F_BOOT_SEL,
	F_CHIP_MODE,
	F_EMMC,
	F_FAN_TACH,
	F_GPIO,
	F_HDMI,
	F_I2C,
	F_I2S,
	F_JTAG,
	F_LPDDR_REF_CLK_SEL,
	F_MIPI_CSI,
	F_OSC,
	F_PCIE,
	F_PWM,
	F_RGMII,
	F_RESET,
	F_SATA,
	F_SDIO,
	F_SPI,
	F_S_MODE,
	F_UART,
	F_USB,
	EIC7700_FUNCTIONS_COUNT
};

static const char *const eic7700_functions[EIC7700_FUNCTIONS_COUNT] = {
	[F_DISABLED]		= "disabled",
	[F_BOOT_SEL]		= "boot_sel",
	[F_CHIP_MODE]		= "chip_mode",
	[F_EMMC]		= "emmc",
	[F_FAN_TACH]		= "fan_tach",
	[F_GPIO]		= "gpio",
	[F_HDMI]		= "hdmi",
	[F_I2C]			= "i2c",
	[F_I2S]			= "i2s",
	[F_JTAG]		= "jtag",
	[F_LPDDR_REF_CLK_SEL]	= "lpddr_ref_clk_sel",
	[F_MIPI_CSI]		= "mipi_csi",
	[F_OSC]			= "osc",
	[F_PCIE]		= "pcie",
	[F_PWM]			= "pwm",
	[F_RGMII]		= "rgmii",
	[F_RESET]		= "reset",
	[F_SATA]		= "sata",
	[F_SDIO]		= "sdio",
	[F_SPI]			= "spi",
	[F_S_MODE]		= "s_mode",
	[F_UART]		= "uart",
	[F_USB]			= "usb",
};

#define EIC7700_PIN(_number, _name, ...) { .name = _name, .functions = { __VA_ARGS__ } }

static const struct eic7700_pin eic7700_pins[] = {
	EIC7700_PIN(0,   "chip_mode",		[0] = F_CHIP_MODE),
	EIC7700_PIN(1,   "mode_set0",		[0] = F_SDIO, [2] = F_GPIO),
	EIC7700_PIN(2,   "mode_set1",		[0] = F_SDIO, [2] = F_GPIO),
	EIC7700_PIN(3,   "mode_set2",		[0] = F_SDIO, [2] = F_GPIO),
	EIC7700_PIN(4,   "mode_set3",		[0] = F_SDIO, [2] = F_GPIO),
	EIC7700_PIN(5,   "xin",			[0] = F_OSC),
	EIC7700_PIN(6,   "rtc_xin",		[0] = F_OSC),
	EIC7700_PIN(7,   "rst_out_n",		[0] = F_RESET),
	EIC7700_PIN(8,   "key_reset_n",		[0] = F_RESET),
	EIC7700_PIN(9,   "rst_in_n",		[0] = F_RESET),
	EIC7700_PIN(10,  "por_in_n",		[0] = F_RESET),
	EIC7700_PIN(11,  "por_out_n",		[0] = F_RESET),
	EIC7700_PIN(12,  "gpio0",		[0] = F_GPIO),
	EIC7700_PIN(13,  "por_sel",		[0] = F_RESET),
	EIC7700_PIN(14,  "jtag0_tck",		[0] = F_JTAG, [1] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(15,  "jtag0_tms",		[0] = F_JTAG, [1] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(16,  "jtag0_tdi",		[0] = F_JTAG, [1] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(17,  "jtag0_tdo",		[0] = F_JTAG, [1] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(18,  "gpio5",		[0] = F_GPIO, [1] = F_SPI),
	EIC7700_PIN(19,  "spi2_cs0_n",		[0] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(20,  "jtag1_tck",		[0] = F_JTAG, [2] = F_GPIO),
	EIC7700_PIN(21,  "jtag1_tms",		[0] = F_JTAG, [2] = F_GPIO),
	EIC7700_PIN(22,  "jtag1_tdi",		[0] = F_JTAG, [2] = F_GPIO),
	EIC7700_PIN(23,  "jtag1_tdo",		[0] = F_JTAG, [2] = F_GPIO),
	EIC7700_PIN(24,  "gpio11",		[0] = F_GPIO),
	EIC7700_PIN(25,  "spi2_cs1_n",		[0] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(26,  "pcie_clkreq_n",	[0] = F_PCIE),
	EIC7700_PIN(27,  "pcie_wake_n",		[0] = F_PCIE),
	EIC7700_PIN(28,  "pcie_perst_n",	[0] = F_PCIE),
	EIC7700_PIN(29,  "hdmi_scl",		[0] = F_HDMI),
	EIC7700_PIN(30,  "hdmi_sda",		[0] = F_HDMI),
	EIC7700_PIN(31,  "hdmi_cec",		[0] = F_HDMI),
	EIC7700_PIN(32,  "jtag2_trst",		[0] = F_JTAG, [2] = F_GPIO),
	EIC7700_PIN(33,  "rgmii0_clk_125",	[0] = F_RGMII),
	EIC7700_PIN(34,  "rgmii0_txen",		[0] = F_RGMII),
	EIC7700_PIN(35,  "rgmii0_txclk",	[0] = F_RGMII),
	EIC7700_PIN(36,  "rgmii0_txd0",		[0] = F_RGMII),
	EIC7700_PIN(37,  "rgmii0_txd1",		[0] = F_RGMII),
	EIC7700_PIN(38,  "rgmii0_txd2",		[0] = F_RGMII),
	EIC7700_PIN(39,  "rgmii0_txd3",		[0] = F_RGMII),
	EIC7700_PIN(40,  "i2s0_bclk",		[0] = F_I2S, [2] = F_GPIO),
	EIC7700_PIN(41,  "i2s0_wclk",		[0] = F_I2S, [2] = F_GPIO),
	EIC7700_PIN(42,  "i2s0_sdi",		[0] = F_I2S, [2] = F_GPIO),
	EIC7700_PIN(43,  "i2s0_sdo",		[0] = F_I2S, [2] = F_GPIO),
	EIC7700_PIN(44,  "i2s_mclk",		[0] = F_I2S, [2] = F_GPIO),
	EIC7700_PIN(45,  "rgmii0_rxclk",	[0] = F_RGMII),
	EIC7700_PIN(46,  "rgmii0_rxdv",		[0] = F_RGMII),
	EIC7700_PIN(47,  "rgmii0_rxd0",		[0] = F_RGMII),
	EIC7700_PIN(48,  "rgmii0_rxd1",		[0] = F_RGMII),
	EIC7700_PIN(49,  "rgmii0_rxd2",		[0] = F_RGMII),
	EIC7700_PIN(50,  "rgmii0_rxd3",		[0] = F_RGMII),
	EIC7700_PIN(51,  "i2s2_bclk",		[0] = F_I2S, [2] = F_GPIO),
	EIC7700_PIN(52,  "i2s2_wclk",		[0] = F_I2S, [2] = F_GPIO),
	EIC7700_PIN(53,  "i2s2_sdi",		[0] = F_I2S, [2] = F_GPIO),
	EIC7700_PIN(54,  "i2s2_sdo",		[0] = F_I2S, [2] = F_GPIO),
	EIC7700_PIN(55,  "gpio27",		[0] = F_GPIO, [1] = F_SATA),
	EIC7700_PIN(56,  "gpio28",		[0] = F_GPIO),
	EIC7700_PIN(57,  "gpio29",		[0] = F_RESET, [1] = F_EMMC, [2] = F_GPIO),
	EIC7700_PIN(58,  "rgmii0_mdc",		[0] = F_RGMII),
	EIC7700_PIN(59,  "rgmii0_mdio",		[0] = F_RGMII),
	EIC7700_PIN(60,  "rgmii0_intb",		[0] = F_RGMII),
	EIC7700_PIN(61,  "rgmii1_clk_125",	[0] = F_RGMII),
	EIC7700_PIN(62,  "rgmii1_txen",		[0] = F_RGMII),
	EIC7700_PIN(63,  "rgmii1_txclk",	[0] = F_RGMII),
	EIC7700_PIN(64,  "rgmii1_txd0",		[0] = F_RGMII),
	EIC7700_PIN(65,  "rgmii1_txd1",		[0] = F_RGMII),
	EIC7700_PIN(66,  "rgmii1_txd2",		[0] = F_RGMII),
	EIC7700_PIN(67,  "rgmii1_txd3",		[0] = F_RGMII),
	EIC7700_PIN(68,  "i2s1_bclk",		[0] = F_I2S, [2] = F_GPIO),
	EIC7700_PIN(69,  "i2s1_wclk",		[0] = F_I2S, [2] = F_GPIO),
	EIC7700_PIN(70,  "i2s1_sdi",		[0] = F_I2S, [2] = F_GPIO),
	EIC7700_PIN(71,  "i2s1_sdo",		[0] = F_I2S, [2] = F_GPIO),
	EIC7700_PIN(72,  "gpio34",		[0] = F_RESET, [1] = F_SDIO, [2] = F_GPIO),
	EIC7700_PIN(73,  "rgmii1_rxclk",	[0] = F_RGMII),
	EIC7700_PIN(74,  "rgmii1_rxdv",		[0] = F_RGMII),
	EIC7700_PIN(75,  "rgmii1_rxd0",		[0] = F_RGMII),
	EIC7700_PIN(76,  "rgmii1_rxd1",		[0] = F_RGMII),
	EIC7700_PIN(77,  "rgmii1_rxd2",		[0] = F_RGMII),
	EIC7700_PIN(78,  "rgmii1_rxd3",		[0] = F_RGMII),
	EIC7700_PIN(79,  "spi1_cs0_n",		[0] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(80,  "spi1_clk",		[0] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(81,  "spi1_d0",		[0] = F_SPI, [1] = F_I2C, [2] = F_GPIO, [3] = F_UART),
	EIC7700_PIN(82,  "spi1_d1",		[0] = F_SPI, [1] = F_I2C, [2] = F_GPIO, [3] = F_UART),
	EIC7700_PIN(83,  "spi1_d2",		[0] = F_SPI, [1] = F_SDIO, [2] = F_GPIO),
	EIC7700_PIN(84,  "spi1_d3",		[0] = F_SPI, [1] = F_PWM, [2] = F_GPIO),
	EIC7700_PIN(85,  "spi1_cs1_n",		[0] = F_SPI, [1] = F_PWM, [2] = F_GPIO),
	EIC7700_PIN(86,  "rgmii1_mdc",		[0] = F_RGMII),
	EIC7700_PIN(87,  "rgmii1_mdio",		[0] = F_RGMII),
	EIC7700_PIN(88,  "rgmii1_intb",		[0] = F_RGMII),
	EIC7700_PIN(89,  "usb0_pwren",		[0] = F_USB, [2] = F_GPIO),
	EIC7700_PIN(90,  "usb1_pwren",		[0] = F_USB, [2] = F_GPIO),
	EIC7700_PIN(91,  "i2c0_scl",		[0] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(92,  "i2c0_sda",		[0] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(93,  "i2c1_scl",		[0] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(94,  "i2c1_sda",		[0] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(95,  "i2c2_scl",		[0] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(96,  "i2c2_sda",		[0] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(97,  "i2c3_scl",		[0] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(98,  "i2c3_sda",		[0] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(99,  "i2c4_scl",		[0] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(100, "i2c4_sda",		[0] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(101, "i2c5_scl",		[0] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(102, "i2c5_sda",		[0] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(103, "uart0_tx",		[0] = F_UART, [2] = F_GPIO),
	EIC7700_PIN(104, "uart0_rx",		[0] = F_UART, [2] = F_GPIO),
	EIC7700_PIN(105, "uart1_tx",		[0] = F_UART, [2] = F_GPIO),
	EIC7700_PIN(106, "uart1_rx",		[0] = F_UART, [2] = F_GPIO),
	EIC7700_PIN(107, "uart1_cts",		[0] = F_UART, [1] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(108, "uart1_rts",		[0] = F_UART, [1] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(109, "uart2_tx",		[0] = F_UART, [1] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(110, "uart2_rx",		[0] = F_UART, [1] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(111, "jtag2_tck",		[0] = F_JTAG, [2] = F_GPIO),
	EIC7700_PIN(112, "jtag2_tms",		[0] = F_JTAG, [2] = F_GPIO),
	EIC7700_PIN(113, "jtag2_tdi",		[0] = F_JTAG, [2] = F_GPIO),
	EIC7700_PIN(114, "jtag2_tdo",		[0] = F_JTAG, [2] = F_GPIO),
	EIC7700_PIN(115, "fan_pwm",		[0] = F_PWM, [2] = F_GPIO),
	EIC7700_PIN(116, "fan_tach",		[0] = F_FAN_TACH, [2] = F_GPIO),
	EIC7700_PIN(117, "mipi_csi0_xvs",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(118, "mipi_csi0_xhs",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(119, "mipi_csi0_mclk",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(120, "mipi_csi1_xvs",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(121, "mipi_csi1_xhs",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(122, "mipi_csi1_mclk",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(123, "mipi_csi2_xvs",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(124, "mipi_csi2_xhs",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(125, "mipi_csi2_mclk",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(126, "mipi_csi3_xvs",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(127, "mipi_csi3_xhs",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(128, "mipi_csi3_mclk",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(129, "mipi_csi4_xvs",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(130, "mipi_csi4_xhs",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(131, "mipi_csi4_mclk",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(132, "mipi_csi5_xvs",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(133, "mipi_csi5_xhs",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(134, "mipi_csi5_mclk",	[0] = F_MIPI_CSI, [2] = F_GPIO),
	EIC7700_PIN(135, "spi3_cs_n",		[0] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(136, "spi3_clk",		[0] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(137, "spi3_di",		[0] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(138, "spi3_do",		[0] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(139, "gpio92",		[0] = F_I2C, [1] = F_MIPI_CSI, [2] = F_GPIO, [3] = F_UART),
	EIC7700_PIN(140, "gpio93",		[0] = F_I2C, [1] = F_MIPI_CSI, [2] = F_GPIO, [3] = F_UART),
	EIC7700_PIN(141, "s_mode",		[0] = F_S_MODE, [2] = F_GPIO),
	EIC7700_PIN(142, "gpio95",		[0] = F_LPDDR_REF_CLK_SEL, [2] = F_GPIO),
	EIC7700_PIN(143, "spi0_cs_n",		[0] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(144, "spi0_clk",		[0] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(145, "spi0_d0",		[0] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(146, "spi0_d1",		[0] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(147, "spi0_d2",		[0] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(148, "spi0_d3",		[0] = F_SPI, [2] = F_GPIO),
	EIC7700_PIN(149, "i2c10_scl",		[0] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(150, "i2c10_sda",		[0] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(151, "i2c11_scl",		[0] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(152, "i2c11_sda",		[0] = F_I2C, [2] = F_GPIO),
	EIC7700_PIN(153, "gpio106",		[0] = F_GPIO),
	EIC7700_PIN(154, "boot_sel0",		[0] = F_BOOT_SEL, [2] = F_GPIO),
	EIC7700_PIN(155, "boot_sel1",		[0] = F_BOOT_SEL, [2] = F_GPIO),
	EIC7700_PIN(156, "boot_sel2",		[0] = F_BOOT_SEL, [2] = F_GPIO),
	EIC7700_PIN(157, "boot_sel3",		[0] = F_BOOT_SEL, [2] = F_GPIO),
	EIC7700_PIN(158, "gpio111",		[0] = F_GPIO),
};

static int eic7700_get_pins_count(struct udevice *dev)
{
	return ARRAY_SIZE(eic7700_pins);
}

static const char *eic7700_get_pin_name(struct udevice *dev, uint pin_selector)
{
	return eic7700_pins[pin_selector].name;
}

static int eic7700_get_functions_count(struct udevice *dev)
{
	return ARRAY_SIZE(eic7700_functions);
}

static const char *eic7700_get_function_name(struct udevice *dev,
					     uint func_selector)
{
	return eic7700_functions[func_selector];
}

static int eic7700_pinmux_set(struct udevice *dev, uint pin_selector,
			      uint func_selector)
{
	const struct eic7700_pin *pin = &eic7700_pins[pin_selector];
	struct eic7700_plat *plat = dev_get_plat(dev);
	uint fs;

	if (pin->functions[0] == F_OSC)
		return -EINVAL;

	for (fs = 0; fs < EIC7700_FUNCTIONS_PER_PIN; fs++)
		if (pin->functions[fs] == func_selector)
			break;

	debug("%s(%s, %s) => %u\n", __func__, pin->name,
	      eic7700_functions[func_selector], fs);

	if (fs == EIC7700_FUNCTIONS_PER_PIN)
		return -EINVAL;

	clrsetbits_le32(plat->base + EIC7700_PIN_REG(pin_selector),
			EIC7700_FUNC_SEL, FIELD_PREP(EIC7700_FUNC_SEL, fs));

	return 0;
}

static const struct pinconf_param eic7700_pinconf_params[] = {
	{ "bias-disable",		PIN_CONFIG_BIAS_DISABLE,	 0 },
	{ "bias-pull-down",		PIN_CONFIG_BIAS_PULL_DOWN,	 2 },
	{ "bias-pull-up",		PIN_CONFIG_BIAS_PULL_UP,	 1 },
	{ "drive-strength-microamp",	PIN_CONFIG_DRIVE_STRENGTH_UA,	 0 },
	{ "input-enable",		PIN_CONFIG_INPUT_ENABLE,	 1 },
	{ "input-disable",		PIN_CONFIG_INPUT_ENABLE,	 0 },
	{ "input-schmitt-disable",	PIN_CONFIG_INPUT_SCHMITT_ENABLE, 0 },
	{ "input-schmitt-enable",	PIN_CONFIG_INPUT_SCHMITT_ENABLE, 1 },
};

static int eic7700_pinconf_set(struct udevice *dev, uint pin_selector,
			       uint param, uint val)
{
	const struct eic7700_pin *pin = &eic7700_pins[pin_selector];
	struct eic7700_plat *plat = dev_get_plat(dev);
	u32 mask;

	if (pin->functions[0] == F_OSC)
		return -EINVAL;

	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_DOWN:
	case PIN_CONFIG_BIAS_PULL_UP:
		mask = EIC7700_BIAS;
		val = FIELD_PREP(EIC7700_BIAS, val);
		break;
	case PIN_CONFIG_DRIVE_STRENGTH_UA:
		if (val < 3000 || val > 24000)
			return -EINVAL;

		mask = EIC7700_DS;
		val = FIELD_PREP(EIC7700_DS, (val - 3000) / 3000);
		break;
	case PIN_CONFIG_INPUT_ENABLE:
		mask = EIC7700_IE;
		val = FIELD_PREP(EIC7700_IE, val);
		break;
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		mask = EIC7700_ST;
		val = FIELD_PREP(EIC7700_ST, val);
		break;
	default:
		return -EINVAL;
	}

	clrsetbits_le32(plat->base + EIC7700_PIN_REG(pin_selector), mask, val);

	return 0;
}

static int eic7700_get_pin_muxing(struct udevice *dev, uint pin_selector,
				  char *buf, int size)
{
	const struct eic7700_pin *pin = &eic7700_pins[pin_selector];
	struct eic7700_plat *plat = dev_get_plat(dev);
	uint func_selector;
	u32 val;

	val = readl(plat->base + EIC7700_PIN_REG(pin_selector));
	func_selector = pin->functions[FIELD_GET(EIC7700_FUNC_SEL, val)];
	strlcpy(buf, eic7700_functions[func_selector], size);

	return 0;
}

static const struct pinctrl_ops eic7700_pinctrl_ops = {
	.get_pins_count		= eic7700_get_pins_count,
	.get_pin_name		= eic7700_get_pin_name,
	.get_functions_count	= eic7700_get_functions_count,
	.get_function_name	= eic7700_get_function_name,
	.pinmux_set		= eic7700_pinmux_set,
	.pinconf_num_params	= ARRAY_SIZE(eic7700_pinconf_params),
	.pinconf_params		= eic7700_pinconf_params,
	.pinconf_set		= eic7700_pinconf_set,
	.set_state		= pinctrl_generic_set_state,
	.get_pin_muxing		= eic7700_get_pin_muxing,
};

static int eic7700_pinctrl_probe(struct udevice *dev)
{
	struct eic7700_plat *plat = dev_get_plat(dev);
	int val;

	val = regulator_get_value(plat->rgmii0_supply);
	writel(val > 0 && val < 1900000 ? EIC7700_MS_1V8 : EIC7700_MS_3V3,
	       plat->base + EIC7700_RGMII0_SEL_MODE);

	val = regulator_get_value(plat->rgmii1_supply);
	writel(val > 0 && val < 1900000 ? EIC7700_MS_1V8 : EIC7700_MS_3V3,
	       plat->base + EIC7700_RGMII1_SEL_MODE);

	return 0;
}

static int eic7700_pinctrl_of_to_plat(struct udevice *dev)
{
	struct eic7700_plat *plat = dev_get_plat(dev);

	plat->base = dev_read_addr_ptr(dev);
	if (!plat->base)
		return -EINVAL;

	device_get_supply_regulator(dev, "rgmii0-supply", &plat->rgmii0_supply);
	device_get_supply_regulator(dev, "rgmii1-supply", &plat->rgmii1_supply);

	return 0;
}

static const struct udevice_id eic7700_pinctrl_ids[] = {
	{ .compatible = "eswin,eic7700-pinctrl" },
	{}
};

U_BOOT_DRIVER(eic7700_pinctrl) = {
	.name		= "eic7700-pinctrl",
	.id		= UCLASS_PINCTRL,
	.of_match	= eic7700_pinctrl_ids,
	.probe		= eic7700_pinctrl_probe,
	.of_to_plat	= eic7700_pinctrl_of_to_plat,
	.plat_auto	= sizeof(struct eic7700_plat),
	.ops		= &eic7700_pinctrl_ops,
};
