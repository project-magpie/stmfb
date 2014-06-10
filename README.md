What is stmfb?
=================

This is a framebuffer driver for certain STMicroelectronics SoCs. Where a
device supports multiple independent display pipelines, the driver is
multi-instance and can be configured to register multiple framebuffers. This
driver internally registers as a platform driver for the device name
"stmcore-display". Nothing will happen until an "stmcore-display-*" module is
loaded into the system (see linux/Documentation/stmcoredisplay.txt)

How to use it?
==============

To enable a framebuffer on an available display pipeline a module parameter
describing the default configuration must be provided. For instance, on the
STx7100:

modprobe stmfb display0=1280x720-24@60:10M:0:NTSC:YUV:YUV

Initialises a framebuffer on the main (HD) display pipeline, but not on the
aux (SD) pipeline.

modprobe stmfb display0=1280x720-24@60:10M:0:NTSC:YUV:YUV \
               display1=720x576-16@50i:3M:0:PAL:CVBS
                    
Initialises two framebuffers, one on each available display pipeline.

Once loaded, fbset can be used to change the framebuffer display mode, within
the constraints of the amount of video memory specified in the module parameter.

Regardless of the name of the display pipelines in use, the created framebuffer
devices are numbered from zero. E.g. if only "display1" is configured, this will
appear as the framebuffer device /dev/fb0, not /dev/fb1.

The list of registered framebuffers can be found by the command:

cat /proc/fb

Display pipeline capabilities
=============================

Each display pipeline on a chip has different capabilities and there are 
circumstances where they share resources, restricting the possible use cases.

         |   7100/7109   |   7200Cut2    | 7111/7141/7105 |    5202    |
         |---------------|---------------|----------------|------------|
display0 | VGA/HD/ED/SD* | VGA/HD/ED/SD* | VGA/HD/ED/SD*  | VGA/ED/SD* |
display1 | SD*           | SD*           | SD*            | SD*        |
display2 |   N/A         | SD            |      N/A       | N/A        |

* SD modes cannot be started simultaneously on display0 and display1


Parameter Strings
=================

The stmfb module supports up to four display pipelines, configured using
the module parameters display[0-3]. On STx7100 and STx7109 only
display0 and display1 are available for use. The parameter strings are colon
or comma separated of which only the first item, the default display mode string
is required. Although in practice the second item, specifying the amount of
memory to reserve for this framebuffer, will almost always be present. The
full parameter string is as follows:

paramstring := <mode>:<memsize>:<auxmemsize>:<sdmode>:<analoguecolour>:<hdmicolour>:<bdispapiversion>

mode := <xres>x<yres>[-<bpp>][@<refresh>][i]

memsize := <num>[M|K]

auxmemsize := <num>[M][K]

sdmode := N[TSC-M] | n[tsc-m] | NTSC-J | NTSC-443 |
          P[AL] | p[al] | PAL-M | PAL-N | PAL-Nc |
          S[ECAM] | s[ecam]
          
analoguecolour := R[GB] | r[gb] | Y[UV] | y[uv] | C[VBS] | c[vbs]

hdmicolour := R[GB] | r[gb] | Y[UV] | y[uv]

bdispapiversion := 0 | 1

If <sdmode>,<analoguecolour> and <hdmicolour> are not specified their default
values are "NTSC","RGB" and "RGB" respectively. If the <memsize> specified
is smaller than that required by the display mode specified in <mode>,
the allocated memory is adjusted upwards to meet the minimum requirements of
that mode. If <-bpp> is omitted from the mode description, the colour will
default to 16bit RGB565.

If the specified <mode> cannot be supported on the display pipeline, a close
match or default mode is picked instead. The actual mode can be determined
after the framebuffer module has been loaded using the fbset command.

Framebuffer Memory
==================

The framebuffer allocates memory from a BPA2 partition named 'bigphysarea', 
which is used for backwards compatibility with the deprecated BPA (bigphysarea)
memory allocator. This is necessary as the display hardware requires buffers
that are single physically contiguous areas of memory; hence we cannot use
Linux kernel virtual memory for this purpose.

Sufficient free pages must be available in the bigphysarea partition,
defined either in your kernel's board setup or on the command line, for the
framebuffer allocation.

How much memory should you configure for a framebuffer? For just a framebuffer
that is a simple calculation of the maximum resolution to be used multiplied 
by the colour depth required. For example:

PAL  : 16bit colour = 720*576*2   = 829940 bytes
1080i: 32bit colour = 1920x1080*4 = 8299400 bytes (nearly 8Mbytes!)

However it gets a bit more complicated if you want to take advantage of 
DirectFB double or tripple buffering and hardware accelerated graphics. DirectFB 
takes all of the memory assigned to the framebuffer and uses any not required
for the primary surface as its offscreen surface cache. Hardware accelerated
drawing can only use surfaces (either source or destination) that have been
allocated from this cache, or the primary surface. So let us take an example:

 - a 1920x1080 tripple buffered primary surface
 - 8Mb of cached decoded JPEGs which contain pre-rendered graphics used
   compose the final display of the application.
   
That is going to require ~ 8*3 + 8 = 32Mb of framebuffer memory. This is 
approximate because surface alignment and stride restrictions may increase this.

To help manage memory partitioning of a system, DirectFB is able to use
'auxiliary memory' for its offscreen surfaces. So, for example, the cached 
decoded JPEGs from above could be placed into the second LMI on SoCs that
support more than one memory interface. This can reduce the memory requirements
of the main framebuffer memory to 24MB in the above example. This partitioning
would free up space for Linux virtual memory in the first LMI, which is
typically where the 'bigphysarea' partition is located.

To make use of this feature, the module options to stmfb.ko should contain a
non-zero value for the <auxmemsize> field and your kernel configuration has to
specify a sufficiently big enough BPA2 partition named 'gfx-memory' to 
allocate it from. However, to be able to cope with hardware restrictions we have
to make sure memory allocated from the 'gfx-memory' partition does not cross a
64MB address boundary. So while this single partition may be useful in many
cases, where large amounts of graphics memory is required another method is also
available.

To give the system integrator finer grained control of the system's memory
partitioning, we support the use of multiple 'gfx-memory-%d' BPA2 partitions to
satisfy the memory requested by the <auxmemsize> parameter. In this case we
assume that the partitions have been correctly aligned & sized so that the
partition itself does not cross a 64MB address boundary; hence any allocation
from these partitions are assumed safe for the hardware to use.


The SD Mode Parameter
=====================

The <sdmode> parameter is used to pick a preferred SD TV encoding standard,
when the framebuffer is switched to a display mode that can support more than
one. It will typically reflect the country the platform is being deployed in.
This parameter does not set the default display mode, that is done using <mode>.

Example1: a platform for the Japanese market may set <sdmode> to NTSC-J. When
          the display mode is changed to 720x480@59i then the encoding used for
          CVBS output and the levels for YPrPb will match the NTSC-J standard. 
          If this platform also supports multi-standard output, then changing
          the display mode to 720x576@50i will default to PAL.
          
Example2: a platform for the French market may set <sdmode> to SECAM. When
          the display mode is changed to 720x576@50i then the encoding used for
          CVBS output and the levels for YPrPb will match the SECAM standard. 
          If this platform also supports multi-standard output, then changing
          the display mode to 720x480@59i will default to NTSC-M.

While in many cases the system's output standards can be statically specified
like this, it is also possible for an application to change the SD encoding
standard dynamically. The recommended way of doing this is to use the
IDirectFBScreen::SetEncoderConfiguration() interface in DirectFB. However 
non-DirectFB applications could use the provided framebuffer device IOCTL
STMFBIO_SET_OUTPUT_CONFIG.

The Analogue Colour Parameter
=============================

The effect of this parameter is partially dependent on the cababilities of a
display pipeline, the chip and what mode is active. The following are the
available settings.

         |   7100/7109       |     7200Cut2      |
         |-------------------|-------------------|
display0 |    YUV/RGB/CVBS   | YUV/RGB/CVBS      |
display1 |      CVBS         | YUV/RGB/CVBS      |
display2 |       N/A         | YUV/CVBS          |
         |---------------------------------------|
         |  7111/7141/7105   |        5202       |
         |-------------------|-------------------|
display0 | YUV/RGB/CVBS      |     YUV/RGB/CVBS  |
display1 | YUV/RGB/CVBS      |       CVBS        |
display2 |       N/A         |        N/A        |

* for VESA VGA or HD/ED modes with sync on green only

If YUV or RGB is specified and the device supports simultaneous CVBS in SD
modes then CVBS will be enabled by default as well. The CVBS option also 
enables Y/C (S-Video) outputs. This is intended to provide a useful starting
point, but the settings can be changed after the module is loaded using
the "stfbset" application (see Runtime Configuration section below) or using
the IDirectFBScreen::SetOutputConfiguration() interface in an application.

In addition the 7111,7141,7105 and 7200Cut2 "display1" can output YUV or RGB
through the HD DACs. This allows SD output on RGB SCART and HD output
(via display0) on HDMI at the same time. The arbitration of the use of the
HD DACs between "display0" and "display1" is simply the most recent thing that
asked to use the DACs will be displayed. If you do not wish the outputs to clash
with each other you should correctly setup the analogue colour parameter or
post install use the "stfbset" application, to make sure only one display
pipeline is configured to use the HD DACs at any time.

The HDMI Colour Parameter
=========================

The <hdmicolour> parameter selects the colourspace used for output to HDMI
devices, if they report support for YUV in their EDID. DVI devices only use RGB,
this parameter is ignored if a DVI device is detected. This can be changed
after the module has loaded using "stfbset" or DirectFB.

The BDisp API version Parameter
===============================

The <bdispapiversion> parameter selects which API version this driver should
support for accessing the BDispII blitter hardware. The default is 1 where
possible (depending on the SoC), which will add support for an mmap() based
approach to access the hardware, which, depending on the use case, can result
in a notable blitter performance gain and/or reduction of host CPU usage.
Specifying 0 here will enable the traditional (old) STMFBIO_BLT based ioctl() api.

API version 1 requires a different DirectFB graphics driver, which is
available in the STLinux-2.3 distribution updates starting with version 1.0.1-31
of the stm-target-directfb package. If enabled, <memsize> may need to be
adjusted, since the userspace BDispII driver needs some extra memory for its
own use. If necessary add another 16384 bytes and make sure that <memsize> is an
integer multiple of 4096 bytes.

Runtime Configuration
=====================

The registered framebuffers can be controlled from userspace using the standard
Linux program "fbset" and the ST provided "stfbset" (installed by the
stlinux23-sh4-stmfb-utils RPM) .

"fbset" is used to change the display mode and colour depth of the framebuffer.

"stfbset" is used to control other properties of a framebuffer's display plane,
the display mixer and the analogue and digital outputs of the board:

1. enable/disable individual analogue and digital outputs, including HDMI;

2. change the global transparency of the framebuffer (note that DirectFB users
   should use IDirectFBDisplayLayer::SetOpacity to do this);

3. change the global gain/brightness of the framebuffer from 0-100% (note that
   DirectFB users should use IDirectFBDisplayLayer::SetColorAdjustment );

4. change the mixer background colour (note that DirectFB users should do this
   with the IDirectFBScreen::SetMixerConfiguration interface instead);

5. enable/disable the flicker filter(*) on graphics planes that have it (note
   that DirectFB users should enable this by configuring the primary layer);

6. change the framebuffer plane blend mode (again DirectFB users should set this
   when configuring the primary layer, and should almost always configure
   the primary surface as DSCAPS_PREMULTIPLIED);

7. enable/disable rescaling framebuffer RGB values from 0-255 to 16-235 (**)

8. Change the display output YUV colourspace from its default, where supported.
   Normally the choice of ITU-R Rec. BT.601 or BT.709 is determined by the
   output display mode, but this can be overridden.

9. Change the data format of DVO output between various 8,16 and 24bit YUV and
   RGB configurations.

10. Change the HDMI output data format between RGB, YUV444 and YUV422.

11. Change the HDMI output colour depth, i.e. deepcolor formats.

12. Select the clipping range of analogue and digital outputs between 0-255 to
    16-235/240, where the hardware supports this. The range can be set
    individually for analogue, HDMI and DVO outputs.

"stfbset" uses several IOCTLs specific to this framebuffer driver to access all
of the above functionality. The IOCTLs are defined in the header file
/usr/include/linux/stmfb.h, installed in the STLinux target filesystem by the
stlinux23-sh4-stmfb-headers RPM. If you have an application that needs to
access these directly, then inspecting the stfbset source code (see
linux/tests/stfbset in this source tree) would be a good guide.
 
Notes:
(*) the flicker filter is only available in RGB (Truecolor) display modes, it
    is always disabled if the framebuffer is set to 8bpp.

(**) if you are using DirectFB or some other graphics package to generate a
     user interface with per pixel alpha (e.g. 32bit ARGB) then you have to be
     carefull about what you generate. In the case of DirectFB you are almost
     certainly going to end up with graphics in a pre-multiplied alpha form.
     That is, you actually have (A, R*A, G*A, B*A) and you will have set up 
     the graphics plane so it is blended with, for instance, video below it on
     this basis.

     In this mode you must NOT use the graphics plane's rescaling to ensure 
     the pixel values are in the sRGB range for TV applications i.e. 16-235.
     Lets take a totally transparent pixel, by definition this _must_ have the
     value (A=0,Ra=0,Ga=0,Ba=0). If rescaling is switched on, this will become
     (A=0,Ra=16,Ga=16,Ba=16) and this is NOT a VALID alpha pre-multiplied pixel.
     What happens when this is blended, say with video below, is that the pixels
     get "lighter" as the invalid RGB values get added to the colour of the
     video pixels; the totally transparent RGB pixel should have had no effect.

     If you are producing a purely graphics system, that does not use per pixel
     transparency, then the rescale can be used. You may want to make your
     graphics drawing code easier, or portable between PC and CE/TV worlds,
     by working in the full RGB range. This may also be useful when implementing
     a standard web browser (e.g. Webkit) for use in a TV application or for
     JPEG photo slideshows. The rescale will then compress the RGB so the
     shadows and highlights are not clipped by the final output or display
     device.
     
     But in a mixed graphics and video environment, you should author your
     graphics specifically for use on CE displays and not enable the rescale.

