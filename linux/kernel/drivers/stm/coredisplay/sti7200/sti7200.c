/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplay/sti7200/sti7200.c
 * Copyright (c) 2007,2008,2010 STMicroelectronics Limited.
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
#include <linux/gpio.h>
#include <linux/stm/gpio.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/stx7200.h>

#include <asm/irq-ilc.h>

#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>

#include "soc/sti7200/sti7200reg.h"
#include "soc/sti7200/sti7200device.h"

#if defined(CONFIG_SH_ST_MB519) || defined(CONFIG_SH_ST_MB671)
#define HAS_DSUB 1
#else
#define HAS_DSUB 0
#endif


static const unsigned long local_whitelist[] = {
    STi7200_REGISTER_BASE + STi7200C2_LOCAL_DISPLAY_BASE,
    STi7200_REGISTER_BASE + STi7200C2_LOCAL_DISPLAY_BASE+PAGE_SIZE,
    STi7200_REGISTER_BASE + STi7200C2_LOCAL_DISPLAY_BASE+(PAGE_SIZE*2),
    STi7200_REGISTER_BASE + STi7200C2_HDMI_BASE,
    _ALIGN_DOWN(STi7200_REGISTER_BASE + STi7200_BLITTER_BASE, PAGE_SIZE)
};

static const unsigned long remote_whitelist[] = {
    STi7200_REGISTER_BASE + STi7200C2_REMOTE_DENC_BASE,
    _ALIGN_DOWN(STi7200_REGISTER_BASE + STi7200_BLITTER_BASE, PAGE_SIZE)
};


/* BDisp IRQs on 7200c2: aq1 50, aq2 142, aq3 143, aq4 144, cq1 51, cq2 52 */
static struct stmcore_display_pipeline_data platform_data[] = {
  {
    .owner                    = THIS_MODULE,
    .name                     = "STi7200-main",
    .device                   = 0,
    .vtg_irq                  = ILC_IRQ(68),
    .blitter_irq              = ILC_IRQ(142),
    .blitter_irq_kernel       = ILC_IRQ(50),
    .hdmi_irq                 = ILC_IRQ(60),
    .hdmi_i2c_adapter_id      = 0,
    .main_output_id           = STi7200_OUTPUT_IDX_VDP0_MAIN,
    .hdmi_output_id           = STi7200_OUTPUT_IDX_VDP0_HDMI,
    .dvo_output_id            = STi7200_OUTPUT_IDX_VDP0_DVO0,

    .blitter_id               = STi7200c2_BLITTER_IDX_VDP0_MAIN,
    .blitter_id_kernel        = STi7200_BLITTER_IDX_KERNEL,
    .blitter_type             = STMCORE_BLITTER_BDISPII,

    .preferred_graphics_plane = OUTPUT_GDP1,
    .preferred_video_plane    = OUTPUT_VID1,
    .planes                   = {
       { OUTPUT_GDP1, STMCORE_PLANE_GFX   | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_ANY},
       { OUTPUT_GDP2, STMCORE_PLANE_GFX   | STMCORE_PLANE_MEM_ANY},
       { OUTPUT_GDP3, STMCORE_PLANE_GFX   | STMCORE_PLANE_SHARED    | STMCORE_PLANE_MEM_ANY},
       { OUTPUT_VID1, STMCORE_PLANE_VIDEO | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_ANY},
       { OUTPUT_VID2, STMCORE_PLANE_VIDEO | STMCORE_PLANE_SHARED    | STMCORE_PLANE_MEM_ANY},
       { OUTPUT_CUR , STMCORE_PLANE_GFX   | STMCORE_PLANE_MEM_ANY}
    },
    .whitelist                = local_whitelist,
    .whitelist_size           = ARRAY_SIZE(local_whitelist),
    .io_offset                = 0,

    /* this is later mmap()ed, so it must be page aligned! */
    .mmio                     = _ALIGN_DOWN (STi7200_REGISTER_BASE + STi7200_BLITTER_BASE, PAGE_SIZE),
    .mmio_len                 = PAGE_SIZE,
  },
  {
    .owner                    = THIS_MODULE,
    .name                     = "STi7200-aux",
    .device                   = 0,
    .vtg_irq                  = ILC_IRQ(70),
    .blitter_irq              = ILC_IRQ(143),
    .blitter_irq_kernel       = -1, /* only one instance of kernel blitter, handled above */
    .hdmi_irq                 = -1,
    .hdmi_i2c_adapter_id      = -1,
    .main_output_id           = STi7200_OUTPUT_IDX_VDP1_MAIN,
    .hdmi_output_id           = -1,
    .dvo_output_id            = -1,

    .blitter_id               = STi7200c2_BLITTER_IDX_VDP1_MAIN,
    .blitter_id_kernel        = STi7200_BLITTER_IDX_KERNEL,
    .blitter_type             = STMCORE_BLITTER_BDISPII,

    .preferred_graphics_plane = OUTPUT_GDP4,
    .preferred_video_plane    = OUTPUT_VID2,
    .planes                   = {
       { OUTPUT_GDP3, STMCORE_PLANE_GFX   | STMCORE_PLANE_SHARED    | STMCORE_PLANE_MEM_ANY},
       { OUTPUT_GDP4, STMCORE_PLANE_GFX   | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_ANY},
       { OUTPUT_VID2, STMCORE_PLANE_VIDEO | STMCORE_PLANE_SHARED    | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_ANY}
    },
    .whitelist                = local_whitelist,
    .whitelist_size           = ARRAY_SIZE(local_whitelist),
    .io_offset                = 0,

    /* this is later mmap()ed, so it must be page aligned! */
    .mmio                     = _ALIGN_DOWN (STi7200_REGISTER_BASE + STi7200_BLITTER_BASE, PAGE_SIZE),
    .mmio_len                 = PAGE_SIZE,
  },
  {
    .owner                    = THIS_MODULE,
    .name                     = "STi7200-remote",
    .device                   = (void*)1,
    .vtg_irq                  = ILC_IRQ(72),
    .blitter_irq              = ILC_IRQ(144),
    .blitter_irq_kernel       = -1, /* only one instance of kernel blitter, handled above */
    .hdmi_irq                 = -1,
    .hdmi_i2c_adapter_id      = -1,
    .main_output_id           = STi7200_OUTPUT_IDX_VDP2_MAIN,
    .hdmi_output_id           = -1,
    .dvo_output_id            = -1,

    .blitter_id               = STi7200c2_BLITTER_IDX_VDP2_MAIN,
    .blitter_id_kernel        = STi7200_BLITTER_IDX_KERNEL,
    .blitter_type             = STMCORE_BLITTER_BDISPII,

    .preferred_graphics_plane = OUTPUT_GDP2,
    .preferred_video_plane    = OUTPUT_VID1,
    .planes                   = {
       { OUTPUT_GDP1, STMCORE_PLANE_GFX   | STMCORE_PLANE_MEM_ANY},
       { OUTPUT_GDP2, STMCORE_PLANE_GFX   | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_ANY},
       { OUTPUT_VID1, STMCORE_PLANE_VIDEO | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_ANY}
    },
    .whitelist                = remote_whitelist,
    .whitelist_size           = ARRAY_SIZE(remote_whitelist),
    .io_offset                = 0,

    /* this is later mmap()ed, so it must be page aligned! */
    .mmio                     = _ALIGN_DOWN (STi7200_REGISTER_BASE + STi7200_BLITTER_BASE, PAGE_SIZE),
    .mmio_len                 = PAGE_SIZE,
  }
};


#if defined(CONFIG_SH_ST_MB519)
/*
 * This error has been observed on two MB519 boards, one RevC and one RevD, so
 * it seems systematic. The error is enough to disturb colour burst and sync in
 * HD modes (particularly 1080i/60Hz) on some very sensitive TVs.
 */
static const int refClockError = 102; /* ppm */
#else
static const int refClockError;
#endif

/*
 * For ST boards only. Customers will need to tune this for their board designs.
 */
#if defined(THIS_IS_WHAT_THE_REFERENCE_DESIGN_SHOULD_PRODUCE)
static const int maxDAC123Voltage = 1400;   // Rref=7.81Kohm Rload=140ohm, Vmax=(78.093/7.810)*140
static const int maxDAC456Voltage = 1400;
#else
static const int maxDAC123Voltage = 1350;
static const int maxDAC456Voltage = 1350;
#endif

static const int DAC123SaturationPoint; // Use Default (1023 for a 10bit DAC)
static const int DAC456SaturationPoint;

#define GPIO_PIN_HOTPLUG stm_gpio(5,7)
static bool claimed_gpio_hotplug;

/*
 * Setup DSub connector H & V sync outputs on MB520
 * NOTE: These signals use pio ports that on production boards are almost
 *       certainly dedicated to something else. So don't just copy this
 *       blindly for your own board.
 *
 *
 *       On MB520 J35 & J36 must be set to the 1-2 postion to get the
 *       signals routed to the DSub connector.
 */
/* The pins must have been set to STPIO_ALT_OUT by some code. */
#define GPIO_PIN_VSYNC stm_gpio(3,7)
#define GPIO_PIN_HSYNC stm_gpio(4,0)
static bool claimed_gpio_vsync;
static bool claimed_gpio_hsync;
static struct sysconf_field *syscfg7_24;

/*
 * An example set of filter coefficients for the HD DAC sample rate converter.
 * These are the default luma coefficients for 1080i/720p. If you have different
 * coefficients, provided by ST, for your board you can set those using a
 * structure like this and code shown below in the post init callback.
 *
static stm_display_filter_setup_t luma_filter_2x_default = {
  HDF_COEFF_2X_LUMA,
  { .hdf = { STM_FLT_DIV_512,
             {
               0x00FD7BFD,
               0x03A88613,
               0x08b,
               0x00000000,
               0x00000000,
               0x0001FBFA,
               0x0428BC29,
               0x00000000,
               0x00000000
             }
           }
  }
};
*/

enum _clocks { CLOCK_PCM0_HDMI };
struct coredisplay_clk {
  struct clk *clk;
  const char *name;
};
/* fixme */
static struct coredisplay_clk coredisplay_clks[] = {
  [CLOCK_PCM0_HDMI] = { .name = "CLKC_FS1_CH3" },
};


int __init stmcore_probe_device(struct stmcore_display_pipeline_data **pd,
                                int *nr_platform_devices)
{
  if(boot_cpu_data.type == CPU_STX7200 && ( boot_cpu_data.cut_major == 2 || boot_cpu_data.cut_major == 3 ) )
  {
    int i;

    *pd = platform_data;
    *nr_platform_devices = ARRAY_SIZE(platform_data);

    /*
     * Setup HDMI hotplug:
     * 1) request pin
     * 2) set direction
     *
     * If 2) fails, we release the GPIO pin.
     */
    if(gpio_request(GPIO_PIN_HOTPLUG, "HDMI Hotplug") >= 0)
    {
      if(gpio_direction_input(GPIO_PIN_HOTPLUG) >= 0)
        claimed_gpio_hotplug = true;

      if(!claimed_gpio_hotplug)
        gpio_free(GPIO_PIN_HOTPLUG);
    }

    if(claimed_gpio_hotplug)
      printk(KERN_INFO "stmcore-display: using HDMI hotplug\n");
    else
      printk(KERN_WARNING "stmcore-display: HDMI hotplug not available\n");

    if(HAS_DSUB != 0)
    {
      if(gpio_request(GPIO_PIN_VSYNC, "DSub VSync") >= 0)
        claimed_gpio_vsync = true;
      if(gpio_request(GPIO_PIN_HSYNC, "DSub HSync") >= 0)
        claimed_gpio_hsync = true;

      if(claimed_gpio_vsync && claimed_gpio_hsync)
      {
        syscfg7_24 = sysconf_claim(SYS_CFG, 7, 24, 24, "DVO Syncs");
        if(syscfg7_24)
          sysconf_write(syscfg7_24, 1);
      }

      if(!syscfg7_24)
      {
        printk(KERN_WARNING "stmcore-display: External H&V sync signals already in use\n");
        printk(KERN_WARNING "stmcore-display: DSub connector output will not be functional.\n");

        if(claimed_gpio_hsync)
        {
          gpio_free(GPIO_PIN_HSYNC);
          claimed_gpio_hsync = false;
        }

        if(claimed_gpio_vsync)
        {
          gpio_free(GPIO_PIN_VSYNC);
          claimed_gpio_vsync = false;
        }
      }
    }

    for(i = 0; i < N_ELEMENTS (coredisplay_clks); ++i)
    {
      coredisplay_clks[i].clk = clk_get_sys ("hdmi", coredisplay_clks[i].name);
      if(coredisplay_clks[i].clk)
        clk_enable (coredisplay_clks[i].clk);
    }

    printk(KERN_INFO "stmcore-display: STi7200 display probed\n");
    return 0;
  }

  printk(KERN_WARNING "stmcore-display: STi7200 platform unknown\n");

  return -ENODEV;
}


int __init stmcore_display_postinit(struct stmcore_display *p)
{
  stm_display_output_setup_clock_reference(p->main_output, STM_CLOCK_REF_30MHZ, refClockError);

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

  /*
   * An example of changing the HD DAC samplerate converter luma filter
   * coefficients for 1080i/720p.
   *
  stm_display_output_set_filter_coefficients(p->main_output, &luma_filter_2x_default);
   */

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

  if(claimed_gpio_hotplug)
  {
    gpio_free(GPIO_PIN_HOTPLUG);
    claimed_gpio_hotplug = false;
  }

  if(claimed_gpio_vsync)
  {
    gpio_free(GPIO_PIN_VSYNC);
    claimed_gpio_vsync = false;
  }

  if(claimed_gpio_hsync)
  {
    gpio_free(GPIO_PIN_HSYNC);
    claimed_gpio_hsync = false;
  }

  if(syscfg7_24)
  {
    sysconf_release(syscfg7_24);
    syscfg7_24 = NULL;
  }
}
