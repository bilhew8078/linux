// SPDX-License-Identifier: GPL-2.0
/*
 * E-Touch JD9365 MIPI-DSI panel driver
 * I JUST FUCKED WITH THIS FILE @ 8:45am
 * Copyright 2020 ImageCue LLC
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <drm/drm_crtc.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>

// These values are based upon data received from E-Touch
// They do not match the median values suggested for the chip
#define H_DISPLAY	800
#define HS_START	840
#define HS_END		860
#define H_TOTAL		900
#define V_DISPLAY	1280
#define VS_START	1296
#define VS_END		1305
#define V_TOTAL		1311
#define V_REFRESH	60
#define WIDTHMM		94
#define HEIGHTMM	151
#define PIX_CLOCK	70794	//(H_TOTAL*V_TOTAL*V_REFRESH)/1000

#define TEAR_SCANLINE 1280 // I have no idea what value should be...

/* Panel specific color-format bits - possibly not used*/
#define COL_FMT_16BPP 0x55
#define COL_FMT_18BPP 0x66
#define COL_FMT_24BPP 0x77

/* Write Manufacture Command Set Control */
#define WRMAUCCTR 0xFE

/* Manufacturer Command Set pages (CMD2) */
struct cmd_set_entry {
	u8 cmd;
	u8 param;
};

/*
 * There is no description in the E-Touch docs about these commands.
 * We received them from E-Touch, so just use them as is.
 */
static const struct cmd_set_entry manufacturer_cmd_set[] = {
		{0xE0,0x00},
		//password
		{0xE1,0x93},
		{0xE2,0x65},
		{0xE3,0xF8},
		//sequence control
		{0x80,0x03}, //0x01 MIPI 2 LANE  03 4 lane
		{0xE0,0x04},
		{0x2D,0x03},
		{0xE0,0x01},
		{0x00,0x00},
		{0x01,0x8E},
		{0x03,0x00},
		{0x04,0x8E},
		{0x17,0x00},
		{0x18,0xD8},
		{0x19,0x01},
		{0x1A,0x00},
		{0x1B,0xD8},
		{0x1C,0x01},
		{0x1F,0x74},
		{0x20,0x19},
		{0x21,0x19},
		{0x22,0x0E},
		{0x37,0x29},
		{0x38,0x05},
		{0x39,0x08},
		{0x3A,0x18},
		{0x3B,0x18},
		{0x3C,0x72},
		{0x3D,0xFF},
		{0x3E,0xFF},
		{0x3F,0xFF},
		{0x40,0x06},
		{0x41,0xA0},
		{0x43,0x10},
		{0x44,0x0E},
		{0x45,0x3C},
		{0x55,0x01},
		{0x56,0x01},
		{0x57,0x65},//VCL=2.5V
		{0x58,0x0A},
		{0x59,0x0A},
		{0x5A,0x28},
		{0x5B,0x10},
		{0x5D,0x7C},
		{0x5E,0x60},
		{0x5F,0x4E},
		{0x60,0x40},
		{0x61,0x39},
		{0x62,0x28},
		{0x63,0x2A},
		{0x64,0x11},
		{0x65,0x27},
		{0x66,0x23},
		{0x67,0x21},
		{0x68,0x3D},
		{0x69,0x2B},
		{0x6A,0x33},
		{0x6B,0x26},
		{0x6C,0x24},
		{0x6D,0x18},
		{0x6E,0x0A},
		{0x6F,0x00},
		{0x70,0x7C},
		{0x71,0x60},
		{0x72,0x4E},
		{0x73,0x40},
		{0x74,0x39},
		{0x75,0x29},
		{0x76,0x2B},
		{0x77,0x12},
		{0x78,0x27},
		{0x79,0x24},
		{0x7A,0x21},
		{0x7B,0x3E},
		{0x7C,0x2B},
		{0x7D,0x33},
		{0x7E,0x26},
		{0x7F,0x24},
		{0x80,0x19},
		{0x81,0x0A},
		{0x82,0x00},
		{0xE0,0x02},
		{0x00,0x0E},
		{0x01,0x06},
		{0x02,0x0C},
		{0x03,0x04},
		{0x04,0x08},
		{0x05,0x19},
		{0x06,0x0A},
		{0x07,0x1B},
		{0x08,0x00},
		{0x09,0x1D},
		{0x0A,0x1F},
		{0x0B,0x1F},
		{0x0C,0x1D},
		{0x0D,0x1D},
		{0x0E,0x1D},
		{0x0F,0x17},
		{0x10,0x37},
		{0x11,0x1D},
		{0x12,0x1F},
		{0x13,0x1E},
		{0x14,0x10},
		{0x15,0x1D},
		{0x16,0x0F},
		{0x17,0x07},
		{0x18,0x0D},
		{0x19,0x05},
		{0x1A,0x09},
		{0x1B,0x1A},
		{0x1C,0x0B},
		{0x1D,0x1C},
		{0x1E,0x01},
		{0x1F,0x1D},
		{0x20,0x1F},
		{0x21,0x1F},
		{0x22,0x1D},
		{0x23,0x1D},
		{0x24,0x1D},
		{0x25,0x18},
		{0x26,0x38},
		{0x27,0x1D},
		{0x28,0x1F},
		{0x29,0x1E},
		{0x2A,0x11},
		{0x2B,0x1D},
		{0x2C,0x09},
		{0x2D,0x1A},
		{0x2E,0x0B},
		{0x2F,0x1C},
		{0x30,0x0F},
		{0x31,0x07},
		{0x32,0x0D},
		{0x33,0x05},
		{0x34,0x11},
		{0x35,0x1D},
		{0x36,0x1F},
		{0x37,0x1F},
		{0x38,0x1D},
		{0x39,0x1D},
		{0x3A,0x1D},
		{0x3B,0x18},
		{0x3C,0x38},
		{0x3D,0x1D},
		{0x3E,0x1E},
		{0x3F,0x1F},
		{0x40,0x01},
		{0x41,0x1D},
		{0x42,0x08},
		{0x43,0x19},
		{0x44,0x0A},
		{0x45,0x1B},
		{0x46,0x0E},
		{0x47,0x06},
		{0x48,0x0C},
		{0x49,0x04},
		{0x4A,0x10},
		{0x4B,0x1D},
		{0x4C,0x1F},
		{0x4D,0x1F},
		{0x4E,0x1D},
		{0x4F,0x1D},
		{0x50,0x1D},
		{0x51,0x17},
		{0x52,0x37},
		{0x53,0x1D},
		{0x54,0x1E},
		{0x55,0x1F},
		{0x56,0x00},
		{0x57,0x1D},
		{0x58,0x10},
		{0x59,0x00},
		{0x5A,0x00},
		{0x5B,0x10},
		{0x5C,0x00},
		{0x5D,0xD0},
		{0x5E,0x01},
		{0x5F,0x02},
		{0x60,0x60},
		{0x61,0x01},
		{0x62,0x02},
		{0x63,0x06},
		{0x64,0x6A},
		{0x65,0x55},
		{0x66,0x0F},
		{0x67,0xF7},
		{0x68,0x08},
		{0x69,0x08},
		{0x6A,0x6A},
		{0x6B,0x10},
		{0x6C,0x00},
		{0x6D,0x00},
		{0x6E,0x00},
		{0x6F,0x88},
		{0x70,0x00},
		{0x71,0x17},
		{0x72,0x06},
		{0x73,0x7B},
		{0x74,0x00},
		{0x75,0x80},
		{0x76,0x01},
		{0x77,0x5D},
		{0x78,0x18},
		{0x79,0x00},
		{0x7A,0x00},
		{0x7B,0x00},
		{0x7C,0x00},
		{0x7D,0x03},
		{0x7E,0x7B},
		{0xE0,0x04},
		{0x09,0x10},
		{0x0E,0x38},
		{0x2B,0x2B},
		{0x2D,0x03},
		{0x2E,0x44},
		{0xE0,0x00},
		{0xE6,0x02},
		{0xE7,0x06},
		{0x11,0x00},
};


static const u32 jd_bus_formats[] = {
	MEDIA_BUS_FMT_RGB888_1X24,
	MEDIA_BUS_FMT_RGB666_1X18,
	MEDIA_BUS_FMT_RGB565_1X16,
};

static const u32 jd_bus_flags = DRM_BUS_FLAG_DE_LOW |
				 DRM_BUS_FLAG_PIXDATA_NEGEDGE;

struct jd_panel {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;

	struct gpio_desc *reset;
	struct backlight_device *backlight;

	bool prepared;
	bool enabled;
};

static const struct drm_display_mode default_mode = {
	.clock = PIX_CLOCK,
	.hdisplay = H_DISPLAY,
	.hsync_start = HS_START,
	.hsync_end = HS_END,
	.htotal = H_TOTAL,
	.vdisplay = V_DISPLAY,
	.vsync_start = VS_START,
	.vsync_end = VS_END,
	.vtotal = V_TOTAL,
	.vrefresh = V_REFRESH,
	.width_mm = WIDTHMM,
	.height_mm = HEIGHTMM,
	.flags = DRM_MODE_FLAG_NHSYNC |
		 DRM_MODE_FLAG_NVSYNC,
};

static inline struct jd_panel *to_jd_panel(struct drm_panel *panel)
{
	return container_of(panel, struct jd_panel, panel);
}

static int jd_panel_push_cmd_list(struct mipi_dsi_device *dsi)
{
	size_t i;
	size_t count = ARRAY_SIZE(manufacturer_cmd_set);
	printk(KERN_NOTICE "BILL: sending init commands: count=%d\n", count);
	int ret = 0;

	for (i = 0; i < count; i++) {
		const struct cmd_set_entry *entry = &manufacturer_cmd_set[i];
		u8 buffer[2] = { entry->cmd, entry->param };

		ret = mipi_dsi_generic_write(dsi, &buffer, sizeof(buffer));
		if (ret < 0)
			return ret;
	}
	printk(KERN_NOTICE "BILL: init commands sent.\n");
	return ret;
};

/*
static int color_format_from_dsi_format(enum mipi_dsi_pixel_format format)
{
	switch (format) {
	case MIPI_DSI_FMT_RGB565:
		return COL_FMT_16BPP;
	case MIPI_DSI_FMT_RGB666:
	case MIPI_DSI_FMT_RGB666_PACKED:
		return COL_FMT_18BPP;
	case MIPI_DSI_FMT_RGB888:
		return COL_FMT_24BPP;
	default:
		return COL_FMT_24BPP; // for backward compatibility
	}
}; */

static int jd_panel_prepare(struct drm_panel *panel)
{
	struct jd_panel *jd = to_jd_panel(panel);
	int ret;

	if (jd->prepared)
		return 0;

	if (jd->reset) {
		gpiod_set_value_cansleep(jd->reset, 0); //make sure the line is HIGH
		usleep_range(10000, 15000);
		gpiod_set_value_cansleep(jd->reset, 1);  //take line LOW to reset
		usleep_range(10000, 15000);
		gpiod_set_value_cansleep(jd->reset, 0); //return the line HIGH
		usleep_range(120000, 150000);
	}

	jd->prepared = true;

	return 0;
}

static int jd_panel_unprepare(struct drm_panel *panel)
{
	struct jd_panel *jd = to_jd_panel(panel);
	int ret;

	if (!jd->prepared)
		return 0;

	/*
	 * Right after asserting the reset, we need to release it, so that the
	 * touch driver can have an active connection with the touch controller
	 * even after the display is turned off.
	 */
	if (jd->reset) {
		gpiod_set_value_cansleep(jd->reset, 1);
		usleep_range(15000, 17000);
		gpiod_set_value_cansleep(jd->reset, 0);
	}

	jd->prepared = false;

	return 0;
}

static int jd_panel_enable(struct drm_panel *panel)
{
	struct jd_panel *jd = to_jd_panel(panel);
	struct mipi_dsi_device *dsi = jd->dsi;
	struct device *dev = &dsi->dev;
	//int color_format = color_format_from_dsi_format(dsi->format);
	int ret;

	if (jd->enabled)
		return 0;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;  //change the mode for instruction push

	ret = jd_panel_push_cmd_list(dsi);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to send MCS (%d)\n", ret);
		goto fail;
	}
	// last command sent took display out of sleep mode - now delay
	usleep_range(120000, 150000); //per the E-Touch init instructions

	/* Turn on display with command */
	ret = mipi_dsi_generic_write(dsi, (u8[]) {0x29, 0x00}, 2);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to turn on display (%d)\n", ret);
		goto fail;
	}
	usleep_range(5000,7500); //per the E-Touch init instructions

	/* Set tear ON */
	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to set tear ON (%d)\n", ret);
		goto fail;
	}
	// Set tear scanline  THIS MIGHT NOT BE NEEDED (setup in early commands?)
	ret = mipi_dsi_dcs_set_tear_scanline(dsi, TEAR_SCANLINE);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to set tear scanline (%d)\n", ret);
		goto fail;
	}
/*	THIS CODE IS LEFT OVER FROM RAYDIUM PANEL DRIVER
	// Set pixel format
	ret = mipi_dsi_dcs_set_pixel_format(dsi, color_format);
	DRM_DEV_DEBUG_DRIVER(dev, "Interface color format set to 0x%x\n",
			     color_format);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to set pixel format (%d)\n", ret);
		goto fail;
	}
	// Exit sleep mode
	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to exit sleep mode (%d)\n", ret);
		goto fail;
	}

	usleep_range(5000, 7000);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to set display ON (%d)\n", ret);
		goto fail;
	}
*/
	backlight_enable(jd->backlight);

	jd->enabled = true;

	return 0;

fail:
	gpiod_set_value_cansleep(jd->reset, 1);

	return ret;
}

static int jd_panel_disable(struct drm_panel *panel)
{
	struct jd_panel *jd = to_jd_panel(panel);
	struct mipi_dsi_device *dsi = jd->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	if (!jd->enabled)
		return 0;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	backlight_disable(jd->backlight);

	usleep_range(10000, 12000);

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to set display OFF (%d)\n", ret);
		return ret;
	}

	usleep_range(5000, 10000);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "Failed to enter sleep mode (%d)\n", ret);
		return ret;
	}

	jd->enabled = false;

	return 0;
}

static int jd_panel_get_modes(struct drm_panel *panel,
			       struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		DRM_DEV_ERROR(panel->dev, "failed to add mode %ux%ux@%u\n",
			      default_mode.hdisplay, default_mode.vdisplay,
			      default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	connector->display_info.bus_flags = jd_bus_flags;

	drm_display_info_set_bus_formats(&connector->display_info,
					 jd_bus_formats,
					 ARRAY_SIZE(jd_bus_formats));
	return 1;
}

static int jd_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	struct jd_panel *jd = mipi_dsi_get_drvdata(dsi);
	u16 brightness;
	int ret;

	if (!jd->prepared)
		return 0;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_get_display_brightness(dsi, &brightness);
	if (ret < 0)
		return ret;

	bl->props.brightness = brightness;

	return brightness & 0xff;
}

static int jd_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	struct jd_panel *jd = mipi_dsi_get_drvdata(dsi);
	int ret = 0;

	if (!jd->prepared)
		return 0;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness(dsi, bl->props.brightness);
	if (ret < 0)
		return ret;

	return 0;
}

static const struct backlight_ops jd_bl_ops = {
	.update_status = jd_bl_update_status,
	.get_brightness = jd_bl_get_brightness,
};

static const struct drm_panel_funcs jd_panel_funcs = {
	.prepare = jd_panel_prepare,
	.unprepare = jd_panel_unprepare,
	.enable = jd_panel_enable,
	.disable = jd_panel_disable,
	.get_modes = jd_panel_get_modes,
};

static int jd_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *np = dev->of_node;
	struct jd_panel *panel;
	struct backlight_properties bl_props;
	int ret;
	u32 video_mode;

	panel = devm_kzalloc(&dsi->dev, sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, panel);

	panel->dsi = dsi;

	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags =  MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO |
			   MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ret = of_property_read_u32(np, "video-mode", &video_mode);
	if (!ret) {
		switch (video_mode) {
		case 0:
			/* burst mode */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_BURST;
			break;
		case 1:
			/* non-burst mode with sync event */
			break;
		case 2:
			/* non-burst mode with sync pulse */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
			break;
		default:
			dev_warn(dev, "invalid video mode %d\n", video_mode);
			break;
		}
	}

	ret = of_property_read_u32(np, "dsi-lanes", &dsi->lanes);
	if (ret) {
		dev_err(dev, "Failed to get dsi-lanes property (%d)\n", ret);
		return ret;
	}

	panel->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(panel->reset))
		return PTR_ERR(panel->reset);

	memset(&bl_props, 0, sizeof(bl_props));
	bl_props.type = BACKLIGHT_RAW;
	bl_props.brightness = 255;
	bl_props.max_brightness = 255;

	panel->backlight = devm_backlight_device_register(dev, dev_name(dev),
							  dev, dsi, &jd_bl_ops,
							  &bl_props);
	if (IS_ERR(panel->backlight)) {
		ret = PTR_ERR(panel->backlight);
		dev_err(dev, "Failed to register backlight (%d)\n", ret);
		return ret;
	}

	drm_panel_init(&panel->panel, dev, &jd_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	dev_set_drvdata(dev, panel);

	ret = drm_panel_add(&panel->panel);
	if (ret)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret)
		drm_panel_remove(&panel->panel);

	return ret;
}

static int jd_panel_remove(struct mipi_dsi_device *dsi)
{
	struct jd_panel *jd = mipi_dsi_get_drvdata(dsi);
	struct device *dev = &dsi->dev;
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret)
		DRM_DEV_ERROR(dev, "Failed to detach from host (%d)\n",
			      ret);

	drm_panel_remove(&jd->panel);

	return 0;
}

static void jd_panel_shutdown(struct mipi_dsi_device *dsi)
{
	struct jd_panel *jd = mipi_dsi_get_drvdata(dsi);

	rad_panel_disable(&jd->panel);
	rad_panel_unprepare(&jd->panel);
}

static const struct of_device_id jd_of_match[] = {
	{ .compatible = "etouch,jd9365", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, jd_of_match);

static struct mipi_dsi_driver jd_panel_driver = {
	.driver = {
		.name = "panel-etouch-jd9365",
		.of_match_table = jd_of_match,
	},
	.probe = jd_panel_probe,
	.remove = jd_panel_remove,
	.shutdown = jd_panel_shutdown,
};
module_mipi_dsi_driver(jd_panel_driver);

MODULE_AUTHOR("Bill Hewlett <bill@imagecuellc.com>");
MODULE_DESCRIPTION("DRM Driver for E-Touch JD9365 MIPI DSI panel");
MODULE_LICENSE("GPL v2");
