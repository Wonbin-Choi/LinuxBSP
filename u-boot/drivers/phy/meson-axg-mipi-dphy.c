// SPDX-License-Identifier: GPL-2.0+
/*
 * Meson AXG MIPI DPHY driver
 *
 * Copyright (C) 2018 Amlogic, Inc. All rights reserved
 * Copyright (C) 2020 BayLibre, SAS
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 */

#include <common.h>
#include <log.h>
#include <malloc.h>
#include <asm/io.h>
#include <bitfield.h>
#include <dm.h>
#include <errno.h>
#include <generic-phy.h>
#include <regmap.h>
#include <linux/delay.h>
#include <power/regulator.h>
#include <reset.h>
#include <clk.h>
#include <phy-mipi-dphy.h>

#include <linux/bitops.h>
#include <linux/compat.h>
#include <linux/bitfield.h>
#include <linux/time.h>

/* [31] soft reset for the phy.
 *		1: reset. 0: dessert the reset.
 * [30] clock lane soft reset.
 * [29] data byte lane 3 soft reset.
 * [28] data byte lane 2 soft reset.
 * [27] data byte lane 1 soft reset.
 * [26] data byte lane 0 soft reset.
 * [25] mipi dsi pll clock selection.
 *		1:  clock from fixed 850Mhz clock source. 0: from VID2 PLL.
 * [12] mipi HSbyteclk enable.
 * [11] mipi divider clk selection.
 *		1: select the mipi DDRCLKHS from clock divider.
 *		0: from PLL clock.
 * [10] mipi clock divider control.
 *		1: /4. 0: /2.
 * [9]  mipi divider output enable.
 * [8]  mipi divider counter enable.
 * [7]  PLL clock enable.
 * [5]  LPDT data endian.
 *		1 = transfer the high bit first. 0 : transfer the low bit first.
 * [4]  HS data endian.
 * [3]  force data byte lane in stop mode.
 * [2]  force data byte lane 0 in receiver mode.
 * [1]  write 1 to sync the txclkesc input. the internal logic have to
 *	use txclkesc to decide Txvalid and Txready.
 * [0]  enalbe the MIPI DPHY TxDDRClk.
 */
#define MIPI_DSI_PHY_CTRL				0x0

/* [31] clk lane tx_hs_en control selection.
 *		1: from register. 0: use clk lane state machine.
 * [30] register bit for clock lane tx_hs_en.
 * [29] clk lane tx_lp_en contrl selection.
 *		1: from register. 0: from clk lane state machine.
 * [28] register bit for clock lane tx_lp_en.
 * [27] chan0 tx_hs_en control selection.
 *		1: from register. 0: from chan0 state machine.
 * [26] register bit for chan0 tx_hs_en.
 * [25] chan0 tx_lp_en control selection.
 *		1: from register. 0: from chan0 state machine.
 * [24] register bit from chan0 tx_lp_en.
 * [23] chan0 rx_lp_en control selection.
 *		1: from register. 0: from chan0 state machine.
 * [22] register bit from chan0 rx_lp_en.
 * [21] chan0 contention detection enable control selection.
 *		1: from register. 0: from chan0 state machine.
 * [20] register bit from chan0 contention dectection enable.
 * [19] chan1 tx_hs_en control selection.
 *		1: from register. 0: from chan0 state machine.
 * [18] register bit for chan1 tx_hs_en.
 * [17] chan1 tx_lp_en control selection.
 *		1: from register. 0: from chan0 state machine.
 * [16] register bit from chan1 tx_lp_en.
 * [15] chan2 tx_hs_en control selection.
 *		1: from register. 0: from chan0 state machine.
 * [14] register bit for chan2 tx_hs_en.
 * [13] chan2 tx_lp_en control selection.
 *		1: from register. 0: from chan0 state machine.
 * [12] register bit from chan2 tx_lp_en.
 * [11] chan3 tx_hs_en control selection.
 *		1: from register. 0: from chan0 state machine.
 * [10] register bit for chan3 tx_hs_en.
 * [9]  chan3 tx_lp_en control selection.
 *		1: from register. 0: from chan0 state machine.
 * [8]  register bit from chan3 tx_lp_en.
 * [4]  clk chan power down. this bit is also used as the power down
 *	of the whole MIPI_DSI_PHY.
 * [3]  chan3 power down.
 * [2]  chan2 power down.
 * [1]  chan1 power down.
 * [0]  chan0 power down.
 */
#define MIPI_DSI_CHAN_CTRL				0x4

/* [24]   rx turn watch dog triggered.
 * [23]   rx esc watchdog  triggered.
 * [22]   mbias ready.
 * [21]   txclkesc  synced and ready.
 * [20:17] clk lane state. {mbias_ready, tx_stop, tx_ulps, tx_hs_active}
 * [16:13] chan3 state{0, tx_stop, tx_ulps, tx_hs_active}
 * [12:9]  chan2 state.{0, tx_stop, tx_ulps, tx_hs_active}
 * [8:5]   chan1 state. {0, tx_stop, tx_ulps, tx_hs_active}
 * [4:0]   chan0 state. {TX_STOP, tx_ULPS, hs_active, direction, rxulpsesc}
 */
#define MIPI_DSI_CHAN_STS				0x8

/* [31:24] TCLK_PREPARE.
 * [23:16] TCLK_ZERO.
 * [15:8]  TCLK_POST.
 * [7:0]   TCLK_TRAIL.
 */
#define MIPI_DSI_CLK_TIM				0xc

/* [31:24] THS_PREPARE.
 * [23:16] THS_ZERO.
 * [15:8]  THS_TRAIL.
 * [7:0]   THS_EXIT.
 */
#define MIPI_DSI_HS_TIM					0x10

/* [31:24] tTA_GET.
 * [23:16] tTA_GO.
 * [15:8]  tTA_SURE.
 * [7:0]   tLPX.
 */
#define MIPI_DSI_LP_TIM					0x14

/* wait time to  MIPI DIS analog ready. */
#define MIPI_DSI_ANA_UP_TIM				0x18

/* TINIT. */
#define MIPI_DSI_INIT_TIM				0x1c

/* TWAKEUP. */
#define MIPI_DSI_WAKEUP_TIM				0x20

/* when in RxULPS check state, after the the logic enable the analog,
 *	how long we should wait to check the lP state .
 */
#define MIPI_DSI_LPOK_TIM				0x24

/* Watchdog for RX low power state no finished. */
#define MIPI_DSI_LP_WCHDOG				0x28

/* tMBIAS,  after send power up signals to analog,
 *	how long we should wait for analog powered up.
 */
#define MIPI_DSI_ANA_CTRL				0x2c

/* [31:8]  reserved for future.
 * [7:0]   tCLK_PRE.
 */
#define MIPI_DSI_CLK_TIM1				0x30

/* watchdog for turn around waiting time. */
#define MIPI_DSI_TURN_WCHDOG				0x34

/* When in RxULPS state, how frequency we should to check
 *	if the TX side out of ULPS state.
 */
#define MIPI_DSI_ULPS_CHECK				0x38
#define MIPI_DSI_TEST_CTRL0				0x3c
#define MIPI_DSI_TEST_CTRL1				0x40

struct phy_meson_axg_mipi_dphy_priv {
	struct regmap *regmap;
#if CONFIG_IS_ENABLED(CLK)
	struct clk clk;
#endif
	struct reset_ctl reset;
	struct phy analog;
	struct phy_configure_opts_mipi_dphy config;
};

static int phy_meson_axg_mipi_dphy_configure(struct phy *phy, void *params)
{
	struct udevice *dev = phy->dev;
	struct phy_meson_axg_mipi_dphy_priv *priv = dev_get_priv(dev);
	struct phy_configure_opts_mipi_dphy *config = params;
	int ret;

	ret = phy_mipi_dphy_config_validate(config);
	if (ret)
		return ret;

	ret = generic_phy_configure(&priv->analog, config);
	if (ret)
		return ret;

	memcpy(&priv->config, config, sizeof(priv->config));

	return 0;
}

static int phy_meson_axg_mipi_dphy_power_on(struct phy *phy)
{
	struct udevice *dev = phy->dev;
	struct phy_meson_axg_mipi_dphy_priv *priv = dev_get_priv(dev);
	unsigned long temp;
	int ret;

	ret = generic_phy_power_on(&priv->analog);
	if (ret)
		return ret;

	/* enable phy clock */
	regmap_write(priv->regmap, MIPI_DSI_PHY_CTRL,  0x1);
	regmap_write(priv->regmap, MIPI_DSI_PHY_CTRL,
		     BIT(0) | /* enable the DSI PLL clock . */
		     BIT(7) | /* enable pll clock which connected to DDR clock path */
		     BIT(8)); /* enable the clock divider counter */

	/* enable the divider clock out */
	regmap_update_bits(priv->regmap, MIPI_DSI_PHY_CTRL, BIT(9), BIT(9));

	/* enable the byte clock generation. */
	regmap_update_bits(priv->regmap, MIPI_DSI_PHY_CTRL, BIT(12), BIT(12));
	regmap_update_bits(priv->regmap, MIPI_DSI_PHY_CTRL, BIT(31), BIT(31));
	regmap_update_bits(priv->regmap, MIPI_DSI_PHY_CTRL, BIT(31), 0);

	/* Calculate lanebyteclk period in ps */
	temp = (1000000 * 100) / (priv->config.hs_clk_rate / 1000);
	temp = temp * 8 * 10;

	regmap_write(priv->regmap, MIPI_DSI_CLK_TIM,
		     DIV_ROUND_UP(priv->config.clk_trail, temp) |
		     (DIV_ROUND_UP(priv->config.clk_post +
				   priv->config.hs_trail, temp) << 8) |
		     (DIV_ROUND_UP(priv->config.clk_zero, temp) << 16) |
		     (DIV_ROUND_UP(priv->config.clk_prepare, temp) << 24));
	regmap_write(priv->regmap, MIPI_DSI_CLK_TIM1,
		     DIV_ROUND_UP(priv->config.clk_pre, temp));

	regmap_write(priv->regmap, MIPI_DSI_HS_TIM,
		     DIV_ROUND_UP(priv->config.hs_exit, temp) |
		     (DIV_ROUND_UP(priv->config.hs_trail, temp) << 8) |
		     (DIV_ROUND_UP(priv->config.hs_zero, temp) << 16) |
		     (DIV_ROUND_UP(priv->config.hs_prepare, temp) << 24));

	regmap_write(priv->regmap, MIPI_DSI_LP_TIM,
		     DIV_ROUND_UP(priv->config.lpx, temp) |
		     (DIV_ROUND_UP(priv->config.ta_sure, temp) << 8) |
		     (DIV_ROUND_UP(priv->config.ta_go, temp) << 16) |
		     (DIV_ROUND_UP(priv->config.ta_get, temp) << 24));

	regmap_write(priv->regmap, MIPI_DSI_ANA_UP_TIM, 0x0100);
	regmap_write(priv->regmap, MIPI_DSI_INIT_TIM,
		     DIV_ROUND_UP(priv->config.init * NSEC_PER_MSEC, temp));
	regmap_write(priv->regmap, MIPI_DSI_WAKEUP_TIM,
		     DIV_ROUND_UP(priv->config.wakeup * NSEC_PER_MSEC, temp));
	regmap_write(priv->regmap, MIPI_DSI_LPOK_TIM, 0x7C);
	regmap_write(priv->regmap, MIPI_DSI_ULPS_CHECK, 0x927C);
	regmap_write(priv->regmap, MIPI_DSI_LP_WCHDOG, 0x1000);
	regmap_write(priv->regmap, MIPI_DSI_TURN_WCHDOG, 0x1000);

	/* Powerup the analog circuit */
	switch (priv->config.lanes) {
	case 1:
		regmap_write(priv->regmap, MIPI_DSI_CHAN_CTRL, 0xe);
		break;
	case 2:
		regmap_write(priv->regmap, MIPI_DSI_CHAN_CTRL, 0xc);
		break;
	case 3:
		regmap_write(priv->regmap, MIPI_DSI_CHAN_CTRL, 0x8);
		break;
	case 4:
	default:
		regmap_write(priv->regmap, MIPI_DSI_CHAN_CTRL, 0);
		break;
	}

	/* Trigger a sync active for esc_clk */
	regmap_update_bits(priv->regmap, MIPI_DSI_PHY_CTRL, BIT(1), BIT(1));

	return 0;
}

static int phy_meson_axg_mipi_dphy_power_off(struct phy *phy)
{
	struct udevice *dev = phy->dev;
	struct phy_meson_axg_mipi_dphy_priv *priv = dev_get_priv(dev);

	regmap_write(priv->regmap, MIPI_DSI_CHAN_CTRL, 0xf);
	regmap_write(priv->regmap, MIPI_DSI_PHY_CTRL, BIT(31));

	return generic_phy_power_off(&priv->analog);
}

static int phy_meson_axg_mipi_dphy_init(struct phy *phy)
{
	struct udevice *dev = phy->dev;
	struct phy_meson_axg_mipi_dphy_priv *priv = dev_get_priv(dev);
	int ret;

	ret = generic_phy_init(&priv->analog);
	if (ret)
		return ret;

	ret = reset_assert(&priv->reset);
	udelay(1);
	ret |= reset_deassert(&priv->reset);
	if (ret)
		return ret;

	return 0;
}

static int phy_meson_axg_mipi_dphy_exit(struct phy *phy)
{
	struct udevice *dev = phy->dev;
	struct phy_meson_axg_mipi_dphy_priv *priv = dev_get_priv(dev);
	int ret;

	ret = generic_phy_exit(&priv->analog);
	if (ret)
		return ret;

	return reset_assert(&priv->reset);
}

struct phy_ops meson_axg_mipi_dphy_ops = {
	.init = phy_meson_axg_mipi_dphy_init,
	.exit = phy_meson_axg_mipi_dphy_exit,
	.power_on = phy_meson_axg_mipi_dphy_power_on,
	.power_off = phy_meson_axg_mipi_dphy_power_off,
	.configure = phy_meson_axg_mipi_dphy_configure,
};

int meson_axg_mipi_dphy_probe(struct udevice *dev)
{
	struct phy_meson_axg_mipi_dphy_priv *priv = dev_get_priv(dev);
	int ret;

	ret = regmap_init_mem(dev_ofnode(dev), &priv->regmap);
	if (ret)
		return ret;

	ret = generic_phy_get_by_index(dev, 0, &priv->analog);
	if (ret)
		return ret;

	ret = reset_get_by_index(dev, 0, &priv->reset);
	if (ret == -ENOTSUPP)
		return 0;
	else if (ret)
		return ret;

	ret = reset_deassert(&priv->reset);
	if (ret) {
		reset_release_all(&priv->reset, 1);
		return ret;
	}

#if CONFIG_IS_ENABLED(CLK)
	ret = clk_get_by_index(dev, 0, &priv->clk);
	if (ret < 0)
		return ret;

	ret = clk_enable(&priv->clk);
	if (ret && ret != -ENOSYS && ret != -ENOTSUPP) {
		pr_err("failed to enable PHY clock\n");
		return ret;
	}
#endif

	return 0;
}

static const struct udevice_id meson_axg_mipi_dphy_ids[] = {
	{ .compatible = "amlogic,axg-mipi-dphy" },
	{ }
};

U_BOOT_DRIVER(meson_axg_mipi_dphy) = {
	.name = "meson_axg_mipi_dphy",
	.id = UCLASS_PHY,
	.of_match = meson_axg_mipi_dphy_ids,
	.probe = meson_axg_mipi_dphy_probe,
	.ops = &meson_axg_mipi_dphy_ops,
	.priv_auto = sizeof(struct phy_meson_axg_mipi_dphy_priv),
};
