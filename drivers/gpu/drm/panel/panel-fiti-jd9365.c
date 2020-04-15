// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020, ImageCue LLC
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>

struct jd9365 {
	struct drm_panel	panel;
	struct mipi_dsi_device	*dsi;
	struct backlight_device	*backlight
	struct regulator	*power;
	struct gpio_desc	*reset;
};

enum jd9365_op {
	JD9365_SWITCH_PAGE,
	JD9365_COMMAND,
};

struct jd9365_instr {
	enum jd9365_op	op;

	union arg {
		struct cmd {
			u8	cmd;
			u8	data;
		} cmd;
		u8	page;
	} arg;
};

#define JD9365_SWITCH_PAGE_INSTR(_page)	\
	{					\
		.op = JD9365_SWITCH_PAGE,	\
		.arg = {			\
			.page = (_page),	\
		},				\
	}

#define JD9365_COMMAND_INSTR(_cmd, _data)		\
	{						\
		.op = JD9365_COMMAND,		\
		.arg = {				\
			.cmd = {			\
				.cmd = (_cmd),		\
				.data = (_data),	\
			},				\
		},					\
	}

// ****** THE VALUES BELOW WERE SUPPLIED BY E-TOUCH

static const struct jd9365_instr jd9365_init[] = {
JD9365_SWITCH_PAGE_INSTR(0),
//password
JD9365_COMMAND_INSTR(0xE1,0x93),
JD9365_COMMAND_INSTR(0xE2,0x65),
JD9365_COMMAND_INSTR(0xE3,0xF8),
//sequence control
JD9365_COMMAND_INSTR(0x80,0x03), //0x01 MIPI 2 LANE  03 4 lane
JD9365_SWITCH_PAGE_INSTR(4),
JD9365_COMMAND_INSTR(0x2D,0x03),
JD9365_SWITCH_PAGE_INSTR(1),
JD9365_COMMAND_INSTR(0x00,0x00),
JD9365_COMMAND_INSTR(0x01,0x8E),
JD9365_COMMAND_INSTR(0x03,0x00),
JD9365_COMMAND_INSTR(0x04,0x8E),
JD9365_COMMAND_INSTR(0x17,0x00),
JD9365_COMMAND_INSTR(0x18,0xD8),
JD9365_COMMAND_INSTR(0x19,0x01),
JD9365_COMMAND_INSTR(0x1A,0x00),
JD9365_COMMAND_INSTR(0x1B,0xD8),
JD9365_COMMAND_INSTR(0x1C,0x01),
JD9365_COMMAND_INSTR(0x1F,0x74),
JD9365_COMMAND_INSTR(0x20,0x19),
JD9365_COMMAND_INSTR(0x21,0x19),
JD9365_COMMAND_INSTR(0x22,0x0E),
JD9365_COMMAND_INSTR(0x37,0x29),
JD9365_COMMAND_INSTR(0x38,0x05),
JD9365_COMMAND_INSTR(0x39,0x08),
JD9365_COMMAND_INSTR(0x3A,0x18),
JD9365_COMMAND_INSTR(0x3B,0x18),
JD9365_COMMAND_INSTR(0x3C,0x72),
JD9365_COMMAND_INSTR(0x3D,0xFF),
JD9365_COMMAND_INSTR(0x3E,0xFF),
JD9365_COMMAND_INSTR(0x3F,0xFF),
JD9365_COMMAND_INSTR(0x40,0x06),
JD9365_COMMAND_INSTR(0x41,0xA0),
JD9365_COMMAND_INSTR(0x43,0x10),
JD9365_COMMAND_INSTR(0x44,0x0E),
JD9365_COMMAND_INSTR(0x45,0x3C),
JD9365_COMMAND_INSTR(0x55,0x01),
JD9365_COMMAND_INSTR(0x56,0x01),
JD9365_COMMAND_INSTR(0x57,0x65),//VCL=2.5V
JD9365_COMMAND_INSTR(0x58,0x0A),
JD9365_COMMAND_INSTR(0x59,0x0A),
JD9365_COMMAND_INSTR(0x5A,0x28),
JD9365_COMMAND_INSTR(0x5B,0x10),
JD9365_COMMAND_INSTR(0x5D,0x7C),
JD9365_COMMAND_INSTR(0x5E,0x60),
JD9365_COMMAND_INSTR(0x5F,0x4E),
JD9365_COMMAND_INSTR(0x60,0x40),
JD9365_COMMAND_INSTR(0x61,0x39),
JD9365_COMMAND_INSTR(0x62,0x28),
JD9365_COMMAND_INSTR(0x63,0x2A),
JD9365_COMMAND_INSTR(0x64,0x11),
JD9365_COMMAND_INSTR(0x65,0x27),
JD9365_COMMAND_INSTR(0x66,0x23),
JD9365_COMMAND_INSTR(0x67,0x21),
JD9365_COMMAND_INSTR(0x68,0x3D),
JD9365_COMMAND_INSTR(0x69,0x2B),
JD9365_COMMAND_INSTR(0x6A,0x33),
JD9365_COMMAND_INSTR(0x6B,0x26),
JD9365_COMMAND_INSTR(0x6C,0x24),
JD9365_COMMAND_INSTR(0x6D,0x18),
JD9365_COMMAND_INSTR(0x6E,0x0A),
JD9365_COMMAND_INSTR(0x6F,0x00),
JD9365_COMMAND_INSTR(0x70,0x7C),
JD9365_COMMAND_INSTR(0x71,0x60),
JD9365_COMMAND_INSTR(0x72,0x4E),
JD9365_COMMAND_INSTR(0x73,0x40),
JD9365_COMMAND_INSTR(0x74,0x39),
JD9365_COMMAND_INSTR(0x75,0x29),
JD9365_COMMAND_INSTR(0x76,0x2B),
JD9365_COMMAND_INSTR(0x77,0x12),
JD9365_COMMAND_INSTR(0x78,0x27),
JD9365_COMMAND_INSTR(0x79,0x24),
JD9365_COMMAND_INSTR(0x7A,0x21),
JD9365_COMMAND_INSTR(0x7B,0x3E),
JD9365_COMMAND_INSTR(0x7C,0x2B),
JD9365_COMMAND_INSTR(0x7D,0x33),
JD9365_COMMAND_INSTR(0x7E,0x26),
JD9365_COMMAND_INSTR(0x7F,0x24),
JD9365_COMMAND_INSTR(0x80,0x19),
JD9365_COMMAND_INSTR(0x81,0x0A),
JD9365_COMMAND_INSTR(0x82,0x00),
JD9365_SWITCH_PAGE_INSTR(2),
JD9365_COMMAND_INSTR(0x00,0x0E),
JD9365_COMMAND_INSTR(0x01,0x06),
JD9365_COMMAND_INSTR(0x02,0x0C),
JD9365_COMMAND_INSTR(0x03,0x04),
JD9365_COMMAND_INSTR(0x04,0x08),
JD9365_COMMAND_INSTR(0x05,0x19),
JD9365_COMMAND_INSTR(0x06,0x0A),
JD9365_COMMAND_INSTR(0x07,0x1B),
JD9365_COMMAND_INSTR(0x08,0x00),
JD9365_COMMAND_INSTR(0x09,0x1D),
JD9365_COMMAND_INSTR(0x0A,0x1F),
JD9365_COMMAND_INSTR(0x0B,0x1F),
JD9365_COMMAND_INSTR(0x0C,0x1D),
JD9365_COMMAND_INSTR(0x0D,0x1D),
JD9365_COMMAND_INSTR(0x0E,0x1D),
JD9365_COMMAND_INSTR(0x0F,0x17),
JD9365_COMMAND_INSTR(0x10,0x37),
JD9365_COMMAND_INSTR(0x11,0x1D),
JD9365_COMMAND_INSTR(0x12,0x1F),
JD9365_COMMAND_INSTR(0x13,0x1E),
JD9365_COMMAND_INSTR(0x14,0x10),
JD9365_COMMAND_INSTR(0x15,0x1D),
JD9365_COMMAND_INSTR(0x16,0x0F),
JD9365_COMMAND_INSTR(0x17,0x07),
JD9365_COMMAND_INSTR(0x18,0x0D),
JD9365_COMMAND_INSTR(0x19,0x05),
JD9365_COMMAND_INSTR(0x1A,0x09),
JD9365_COMMAND_INSTR(0x1B,0x1A),
JD9365_COMMAND_INSTR(0x1C,0x0B),
JD9365_COMMAND_INSTR(0x1D,0x1C),
JD9365_COMMAND_INSTR(0x1E,0x01),
JD9365_COMMAND_INSTR(0x1F,0x1D),
JD9365_COMMAND_INSTR(0x20,0x1F),
JD9365_COMMAND_INSTR(0x21,0x1F),
JD9365_COMMAND_INSTR(0x22,0x1D),
JD9365_COMMAND_INSTR(0x23,0x1D),
JD9365_COMMAND_INSTR(0x24,0x1D),
JD9365_COMMAND_INSTR(0x25,0x18),
JD9365_COMMAND_INSTR(0x26,0x38),
JD9365_COMMAND_INSTR(0x27,0x1D),
JD9365_COMMAND_INSTR(0x28,0x1F),
JD9365_COMMAND_INSTR(0x29,0x1E),
JD9365_COMMAND_INSTR(0x2A,0x11),
JD9365_COMMAND_INSTR(0x2B,0x1D),
JD9365_COMMAND_INSTR(0x2C,0x09),
JD9365_COMMAND_INSTR(0x2D,0x1A),
JD9365_COMMAND_INSTR(0x2E,0x0B),
JD9365_COMMAND_INSTR(0x2F,0x1C),
JD9365_COMMAND_INSTR(0x30,0x0F),
JD9365_COMMAND_INSTR(0x31,0x07),
JD9365_COMMAND_INSTR(0x32,0x0D),
JD9365_COMMAND_INSTR(0x33,0x05),
JD9365_COMMAND_INSTR(0x34,0x11),
JD9365_COMMAND_INSTR(0x35,0x1D),
JD9365_COMMAND_INSTR(0x36,0x1F),
JD9365_COMMAND_INSTR(0x37,0x1F),
JD9365_COMMAND_INSTR(0x38,0x1D),
JD9365_COMMAND_INSTR(0x39,0x1D),
JD9365_COMMAND_INSTR(0x3A,0x1D),
JD9365_COMMAND_INSTR(0x3B,0x18),
JD9365_COMMAND_INSTR(0x3C,0x38),
JD9365_COMMAND_INSTR(0x3D,0x1D),
JD9365_COMMAND_INSTR(0x3E,0x1E),
JD9365_COMMAND_INSTR(0x3F,0x1F),
JD9365_COMMAND_INSTR(0x40,0x01),
JD9365_COMMAND_INSTR(0x41,0x1D),
JD9365_COMMAND_INSTR(0x42,0x08),
JD9365_COMMAND_INSTR(0x43,0x19),
JD9365_COMMAND_INSTR(0x44,0x0A),
JD9365_COMMAND_INSTR(0x45,0x1B),
JD9365_COMMAND_INSTR(0x46,0x0E),
JD9365_COMMAND_INSTR(0x47,0x06),
JD9365_COMMAND_INSTR(0x48,0x0C),
JD9365_COMMAND_INSTR(0x49,0x04),
JD9365_COMMAND_INSTR(0x4A,0x10),
JD9365_COMMAND_INSTR(0x4B,0x1D),
JD9365_COMMAND_INSTR(0x4C,0x1F),
JD9365_COMMAND_INSTR(0x4D,0x1F),
JD9365_COMMAND_INSTR(0x4E,0x1D),
JD9365_COMMAND_INSTR(0x4F,0x1D),
JD9365_COMMAND_INSTR(0x50,0x1D),
JD9365_COMMAND_INSTR(0x51,0x17),
JD9365_COMMAND_INSTR(0x52,0x37),
JD9365_COMMAND_INSTR(0x53,0x1D),
JD9365_COMMAND_INSTR(0x54,0x1E),
JD9365_COMMAND_INSTR(0x55,0x1F),
JD9365_COMMAND_INSTR(0x56,0x00),
JD9365_COMMAND_INSTR(0x57,0x1D),
JD9365_COMMAND_INSTR(0x58,0x10),
JD9365_COMMAND_INSTR(0x59,0x00),
JD9365_COMMAND_INSTR(0x5A,0x00),
JD9365_COMMAND_INSTR(0x5B,0x10),
JD9365_COMMAND_INSTR(0x5C,0x00),
JD9365_COMMAND_INSTR(0x5D,0xD0),
JD9365_COMMAND_INSTR(0x5E,0x01),
JD9365_COMMAND_INSTR(0x5F,0x02),
JD9365_COMMAND_INSTR(0x60,0x60),
JD9365_COMMAND_INSTR(0x61,0x01),
JD9365_COMMAND_INSTR(0x62,0x02),
JD9365_COMMAND_INSTR(0x63,0x06),
JD9365_COMMAND_INSTR(0x64,0x6A),
JD9365_COMMAND_INSTR(0x65,0x55),
JD9365_COMMAND_INSTR(0x66,0x0F),
JD9365_COMMAND_INSTR(0x67,0xF7),
JD9365_COMMAND_INSTR(0x68,0x08),
JD9365_COMMAND_INSTR(0x69,0x08),
JD9365_COMMAND_INSTR(0x6A,0x6A),
JD9365_COMMAND_INSTR(0x6B,0x10),
JD9365_COMMAND_INSTR(0x6C,0x00),
JD9365_COMMAND_INSTR(0x6D,0x00),
JD9365_COMMAND_INSTR(0x6E,0x00),
JD9365_COMMAND_INSTR(0x6F,0x88),
JD9365_COMMAND_INSTR(0x70,0x00),
JD9365_COMMAND_INSTR(0x71,0x17),
JD9365_COMMAND_INSTR(0x72,0x06),
JD9365_COMMAND_INSTR(0x73,0x7B),
JD9365_COMMAND_INSTR(0x74,0x00),
JD9365_COMMAND_INSTR(0x75,0x80),
JD9365_COMMAND_INSTR(0x76,0x01),
JD9365_COMMAND_INSTR(0x77,0x5D),
JD9365_COMMAND_INSTR(0x78,0x18),
JD9365_COMMAND_INSTR(0x79,0x00),
JD9365_COMMAND_INSTR(0x7A,0x00),
JD9365_COMMAND_INSTR(0x7B,0x00),
JD9365_COMMAND_INSTR(0x7C,0x00),
JD9365_COMMAND_INSTR(0x7D,0x03),
JD9365_COMMAND_INSTR(0x7E,0x7B),
JD9365_SWITCH_PAGE_INSTR(4),
JD9365_COMMAND_INSTR(0x09,0x10),
JD9365_COMMAND_INSTR(0x0E,0x38),
JD9365_COMMAND_INSTR(0x2B,0x2B),
JD9365_COMMAND_INSTR(0x2D,0x03),
JD9365_COMMAND_INSTR(0x2E,0x44),
JD9365_SWITCH_PAGE_INSTR(0),
JD9365_COMMAND_INSTR(0xE6,0x02),
JD9365_COMMAND_INSTR(0xE7,0x06),
JD9365_COMMAND_INSTR(0x11,0x00),
};

static inline struct jd9365 *panel_to_jd9365(struct drm_panel *panel)
{
	return container_of(panel, struct jd9365, panel);
}

/*
 * The panel seems to accept some private DCS commands that map
 * directly to registers.
 *
 * It is organised by page, with each page having its own set of
 * registers, and the first page looks like it's holding the standard
 * DCS commands.
 *
 * So before any attempt at sending a command or data, we have to be
 * sure if we're in the right page or not.
 */
static int jd9365_switch_page(struct jd9365 *ctx, u8 page)
{
	u8 buf[4] = { 0xff, 0x98, 0x81, page };
	int ret;

	ret = mipi_dsi_dcs_write_buffer(ctx->dsi, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	return 0;
}

static int jd9365_send_cmd_data(struct jd9365 *ctx, u8 cmd, u8 data)
{
	u8 buf[2] = { cmd, data };
	int ret;

	ret = mipi_dsi_dcs_write_buffer(ctx->dsi, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	return 0;
}

static int jd9365_prepare(struct drm_panel *panel)
{
	struct jd9365 *ctx = panel_to_jd9365(panel);
	unsigned int i;
	int ret;
	printk(KERN_NOTICE "JD9365 Module Prepare started\n");
	/* Power the panel */
	ret = regulator_enable(ctx->power);
	if (ret)
		return ret;

	msleep(5);

	/* And reset it */
	gpiod_set_value(ctx->reset, 0);
	msleep(5);

	gpiod_set_value(ctx->reset, 1);
	msleep(10);

	gpiod_set_value(ctx->reset, 0);
	msleep(120);

	for (i = 0; i < ARRAY_SIZE(jd9365_init); i++) {
		const struct jd9365_instr *instr = &jd9365_init[i];

		if (instr->op == JD9365_SWITCH_PAGE)
			ret = jd9365_switch_page(ctx, instr->arg.page);
		else if (instr->op == JD9365_COMMAND)
			ret = jd9365_send_cmd_data(ctx, instr->arg.cmd.cmd,
						      instr->arg.cmd.data);

		if (ret)
			return ret;
	}
	msleep(120);
	ret =jd9365_send_cmd_data(ctx, 0x29, 0x00);
	if (ret)
		return ret;
	msleep(5);
	ret =jd9365_send_cmd_data(ctx, 0x35, 0x00);
	if (ret)
		return ret;

	ret = jd9365_switch_page(ctx, 0);
	if (ret)
		return ret;

	ret = mipi_dsi_dcs_set_tear_on(ctx->dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret)
		return ret;

	ret = mipi_dsi_dcs_exit_sleep_mode(ctx->dsi);
	if (ret)
		return ret;

	return 0;
}

static int jd9365_enable(struct drm_panel *panel)
{
	struct jd9365 *ctx = panel_to_jd9365(panel);

	msleep(120);

	mipi_dsi_dcs_set_display_on(ctx->dsi);
	backlight_enable(ctx->backlight);

	return 0;
}

static int jd9365_disable(struct drm_panel *panel)
{
	struct jd9365 *ctx = panel_to_jd9365(panel);

	backlight_disable(ctx->backlight);
	return mipi_dsi_dcs_set_display_off(ctx->dsi);
}

static int jd9365_unprepare(struct drm_panel *panel)
{
	struct jd9365 *ctx = panel_to_jd9365(panel);

	mipi_dsi_dcs_enter_sleep_mode(ctx->dsi);
	regulator_disable(ctx->power);
	gpiod_set_value(ctx->reset, 1);

	return 0;
}

static const struct drm_display_mode bananapi_default_mode = {
	.clock		= 67330,
	.vrefresh	= 60,

	.hdisplay	= 800,
	.hsync_start	= 800 + 10,
	.hsync_end	= 800 + 10 + 20,
	.htotal		= 800 + 10 + 20 + 30,

	.vdisplay	= 1280,
	.vsync_start	= 1280 + 10,
	.vsync_end	= 1280 + 10 + 10,
	.vtotal		= 1280 + 10 + 10 + 20,
};

static int jd9365_get_modes(struct drm_panel *panel)
{
	struct drm_connector *connector = panel->connector;
	struct jd9365 *ctx = panel_to_jd9365(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, &bananapi_default_mode);
	if (!mode) {
		dev_err(&ctx->dsi->dev, "failed to add mode %ux%ux@%u\n",
			bananapi_default_mode.hdisplay,
			bananapi_default_mode.vdisplay,
			bananapi_default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	panel->connector->display_info.width_mm = 94;
	panel->connector->display_info.height_mm = 151;

	return 1;
}

static const struct drm_panel_funcs jd9365_funcs = {
	.prepare	= jd9365_prepare,
	.unprepare	= jd9365_unprepare,
	.enable		= jd9365_enable,
	.disable	= jd9365_disable,
	.get_modes	= jd9365_get_modes,
};

static int jd9365_dsi_probe(struct mipi_dsi_device *dsi)
{
	struct device_node *np;
	struct jd9365 *ctx;
	int ret;

	ctx = devm_kzalloc(&dsi->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dsi = dsi;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = &dsi->dev;
	ctx->panel.funcs = &jd9365_funcs;

	ctx->power = devm_regulator_get(&dsi->dev, "power");
	if (IS_ERR(ctx->power)) {
		dev_err(&dsi->dev, "Couldn't get our power regulator\n");
		return PTR_ERR(ctx->power);
	}

	ctx->reset = devm_gpiod_get(&dsi->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset)) {
		dev_err(&dsi->dev, "Couldn't get our reset GPIO\n");
		return PTR_ERR(ctx->reset);
	}

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->lanes = 4;

	return mipi_dsi_attach(dsi);
}

static int jd9365_dsi_remove(struct mipi_dsi_device *dsi)
{
	struct jd9365 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);


	return 0;
}

static const struct of_device_id jd9365_of_match[] = {
	{ .compatible = "fiti,jd9365" },
	{ }
};
MODULE_DEVICE_TABLE(of, jd9365_of_match);

static struct mipi_dsi_driver jd9365_dsi_driver = {
	.probe		= jd9365_dsi_probe,
	.remove		= jd9365_dsi_remove,
	.driver = {
		.name		= "jd9365-dsi",
		.of_match_table	= jd9365_of_match,
	},
};
module_mipi_dsi_driver(jd9365_dsi_driver);

MODULE_AUTHOR("Bill Hewlett <bill@imagecue.lighting>");
MODULE_DESCRIPTION("Fiti JD9365 Controller Driver");
MODULE_LICENSE("GPL v2");
