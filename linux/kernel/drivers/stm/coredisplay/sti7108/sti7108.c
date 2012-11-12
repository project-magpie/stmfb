/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplay/sti7108/sti7108.c
 * Copyright (c) 2009-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/stm/pad.h>
#include <linux/stm/stx7108.h>

#include <asm/irq-ilc.h>

#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>

#include <soc/sti7108/sti7108reg.h>
#include <soc/sti7108/sti7108device.h>
#include <STMCommon/stmhdmiregs.h>


static const unsigned long whitelist[] = {
    STi7108_REGISTER_BASE + STi7108_DENC_BASE,
    STi7108_REGISTER_BASE + STi7108_DENC_BASE+PAGE_SIZE,
    STi7108_REGISTER_BASE + STi7108_DENC_BASE+(PAGE_SIZE*2),
    STi7108_REGISTER_BASE + STi7108_HDMI_BASE,
    _ALIGN_DOWN(STi7108_REGISTER_BASE + STi7108_BLITTER_BASE, PAGE_SIZE),
};


static struct stmcore_display_pipeline_data platform_data[] = {
  {
    .owner                    = THIS_MODULE,
    .name                     = "STi7108-main",
    .device                   = 0,
    .vtg_irq                  = ILC_IRQ(92),
    .blitter_irq              = ILC_IRQ(82),
    .blitter_irq_kernel       = ILC_IRQ(81),
    .hdmi_irq                 = ILC_IRQ(107),
#if defined(CONFIG_SH_ST_MB837)
    .hdmi_i2c_adapter_id      = 4, // NOTE: this can change if the board setup is changed, bewarned.
#elif defined(CONFIG_SH_ST_HDK7108)
    .hdmi_i2c_adapter_id      = 3, // NOTE: this can change if the board setup is changed, bewarned.
#else
    .hdmi_i2c_adapter_id      = 0,
#endif
    .main_output_id           = STi7108_OUTPUT_IDX_VDP0_MAIN,
    .hdmi_output_id           = STi7108_OUTPUT_IDX_VDP0_HDMI,
    .dvo_output_id            = STi7108_OUTPUT_IDX_DVO0,

    .blitter_id               = STi7108_BLITTER_IDX_VDP0_MAIN,
    .blitter_id_kernel        = STi7108_BLITTER_IDX_KERNEL,
    .blitter_type             = STMCORE_BLITTER_BDISPII,

    .preferred_graphics_plane = OUTPUT_GDP4,
    .preferred_video_plane    = OUTPUT_VID1,
    .planes                   = {
       { OUTPUT_GDP1, STMCORE_PLANE_GFX | STMCORE_PLANE_SHARED | STMCORE_PLANE_MEM_ANY },
       { OUTPUT_GDP2, STMCORE_PLANE_GFX | STMCORE_PLANE_SHARED | STMCORE_PLANE_MEM_ANY },
       { OUTPUT_GDP3, STMCORE_PLANE_GFX | STMCORE_PLANE_MEM_ANY },
       { OUTPUT_GDP4, STMCORE_PLANE_GFX | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_ANY },
       { OUTPUT_VID1, STMCORE_PLANE_VIDEO | STMCORE_PLANE_SHARED |STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_ANY },
       { OUTPUT_CUR , STMCORE_PLANE_GFX | STMCORE_PLANE_MEM_SYS }
    },
    .whitelist                = whitelist,
    .whitelist_size           = ARRAY_SIZE(whitelist),
    .io_offset                = 0,

    /* this is later mmap()ed, so it must be page aligned! */
    .mmio                     = _ALIGN_DOWN (STi7108_REGISTER_BASE + STi7108_BLITTER_BASE, PAGE_SIZE),
    .mmio_len                 = PAGE_SIZE,
  },
  {
    .owner                    = THIS_MODULE,
    .name                     = "STi7108-aux",
    .device                   = 0,
    .vtg_irq                  = ILC_IRQ(90),
    .blitter_irq              = ILC_IRQ(83),
    .blitter_irq_kernel       = -1, /* only one instance of kernel blitter, handled above */
    .hdmi_irq                 = -1,
    .hdmi_i2c_adapter_id      = -1,
    .main_output_id           = STi7108_OUTPUT_IDX_VDP1_MAIN,
    .hdmi_output_id           = -1,
    .dvo_output_id            = -1,

    .blitter_id               = STi7108_BLITTER_IDX_VDP1_MAIN,
    .blitter_id_kernel        = STi7108_BLITTER_IDX_KERNEL,
    .blitter_type             = STMCORE_BLITTER_BDISPII,

    .preferred_graphics_plane = OUTPUT_GDP2,
    .preferred_video_plane    = OUTPUT_VID1,
    .planes                   = {
       { OUTPUT_GDP1, STMCORE_PLANE_GFX | STMCORE_PLANE_SHARED | STMCORE_PLANE_MEM_ANY },
       { OUTPUT_GDP2, STMCORE_PLANE_GFX | STMCORE_PLANE_SHARED | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_ANY },
       { OUTPUT_VID1, STMCORE_PLANE_VIDEO | STMCORE_PLANE_SHARED | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_ANY }
    },
    .whitelist                = whitelist,
    .whitelist_size           = ARRAY_SIZE(whitelist),
    .io_offset                = 0,

    /* this is later mmap()ed, so it must be page aligned! */
    .mmio                     = _ALIGN_DOWN (STi7108_REGISTER_BASE + STi7108_BLITTER_BASE, PAGE_SIZE),
    .mmio_len                 = PAGE_SIZE,
  }
};


/*
 * The theoretical DAC voltage range is 1.4v, but different board designs
 * effect this.
 */
#if defined(CONFIG_SH_ST_MB837)

static const int maxDAC123Voltage = 1490;
static const int maxDAC456Voltage = 1370;
/*
 * TODO: awaiting HDMI PHY pre-empasis setup for standard 1080p 50/60Hz modes
 * and both pre-emphasis and source termination for deepcolour modes with a
 * TMDS clock >165MHz.
 */
static stm_display_hdmi_phy_config_t hdmiphy_config[] = {
    { 148000000, 148250000, { 0, 0, 0, STM_HDMI_SRZ_CTRL_SRC_TERM(STM_HDMI_SRZ_CTRL_SRC_TERM_OFF) } },
    { 165000001, 225000000, { 0, 0, 0, STM_HDMI_SRZ_CTRL_SRC_TERM(STM_HDMI_SRZ_CTRL_SRC_TERM_65MV) } },
    { 0, 0 }
};

#elif defined(CONFIG_SH_ST_HDK7108)

static const int maxDAC123Voltage = 1350;
static const int maxDAC456Voltage = 1350;

/* copied from mb837, likely to be incorrect */
static stm_display_hdmi_phy_config_t hdmiphy_config[] = {
    { 148000000, 148250000, { 0, 0, 0, STM_HDMI_SRZ_CTRL_SRC_TERM(STM_HDMI_SRZ_CTRL_SRC_TERM_OFF) } },
    { 165000001, 225000000, { 0, 0, 0, STM_HDMI_SRZ_CTRL_SRC_TERM(STM_HDMI_SRZ_CTRL_SRC_TERM_65MV) } },
    { 0, 0 }
};

#else
static const int maxDAC123Voltage = 1400;
static const int maxDAC456Voltage = 1400;

static stm_display_hdmi_phy_config_t hdmiphy_config[] = {
    { 0, 0 }
};
#endif

static const int DAC123SaturationPoint; // Use Default (1023 for a 10bit DAC)
static const int DAC456SaturationPoint;

static struct stm_pad_config stx7108_hotplug_pad_config = {
    .gpios_num = 1,
    .gpios = (struct stm_pad_gpio []) {
            STM_PAD_PIO_IN(15, 1, 1),   /* Hotplug,PIO15.1 function alt input */
    }
};

static struct stm_pad_state *hotplug_pad;

enum _clocks { CLOCK_PCM0, CLOCK_SPDIF };
struct coredisplay_clk {
  struct clk *clk;
  const char *name;
};
static struct coredisplay_clk coredisplay_clks[] = {
  [CLOCK_PCM0]  = { .name = "CLKC_FS0_CH1" },
  [CLOCK_SPDIF] = { .name = "CLKC_FS0_CH3" }
};


int __init stmcore_probe_device(struct stmcore_display_pipeline_data **pd,
                                int *nr_platform_devices)
{
  if(boot_cpu_data.type == CPU_STX7108)
  {
    int i;

    *pd = platform_data;
    *nr_platform_devices = ARRAY_SIZE(platform_data);

    /*
     * Setup HDMI hotplug
     */
    hotplug_pad = stm_pad_claim(&stx7108_hotplug_pad_config,"HDMI Hotplug");

    if(!hotplug_pad)
      printk(KERN_WARNING "stmcore-display: HDMI hotplug not available\n");

    for(i = 0; i < N_ELEMENTS (coredisplay_clks); ++i)
    {
      coredisplay_clks[i].clk = clk_get_sys ("hdmi", coredisplay_clks[i].name);
      if(coredisplay_clks[i].clk)
        clk_enable (coredisplay_clks[i].clk);
    }

    printk(KERN_INFO "stmcore-display: STi7108 display: probed\n");
    return 0;
  }

  printk(KERN_WARNING "stmcore-display: STi7108 display: platform unknown\n");

  return -ENODEV;
}


int __init stmcore_display_postinit(struct stmcore_display *p)
{
  /*
   * Setup internal configuration controls
   */
  if(maxDAC123Voltage != 0)
    stm_display_output_set_control(p->main_output, STM_CTRL_DAC123_MAX_VOLTAGE, maxDAC123Voltage);

  if(maxDAC456Voltage != 0)
    stm_display_output_set_control(p->main_output, STM_CTRL_DAC456_MAX_VOLTAGE, maxDAC456Voltage);

  if(DAC123SaturationPoint != 0)
    stm_display_output_set_control(p->main_output, STM_CTRL_DAC123_SATURATION_POINT, DAC123SaturationPoint);

  if(DAC456SaturationPoint != 0)
    stm_display_output_set_control(p->main_output, STM_CTRL_DAC456_SATURATION_POINT, DAC456SaturationPoint);

  if(p->hdmi_output)
    stm_display_output_set_control(p->hdmi_output, STM_CTRL_HDMI_PHY_CONF_TABLE, (ULONG)hdmiphy_config);

  return 0;
}


void stmcore_cleanup_device(void)
{
  int i;

  for(i = 0; i < N_ELEMENTS (coredisplay_clks); ++i)
  {
    if(coredisplay_clks[i].clk)
    {
      clk_disable (coredisplay_clks[i].clk);
      clk_put (coredisplay_clks[i].clk);
    }
  }

  if(hotplug_pad)
  {
    stm_pad_release(hotplug_pad);
    hotplug_pad = NULL;
  }
}
