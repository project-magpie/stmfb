/***********************************************************************
 *
 * File: soc/sti7108/sti7108auxoutput.cpp
 * Copyright (c) 2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <stmdisplay.h>

#include <Generic/IOS.h>
#include <Generic/IDebug.h>


#include <STMCommon/stmsdvtg.h>
#include <STMCommon/stmfsynth.h>

#include <HDTVOutFormatter/stmhdfawg.h>
#include <HDTVOutFormatter/stmhdfoutput.h>
#include <HDTVOutFormatter/stmtvoutreg.h>
#include <HDTVOutFormatter/stmtvoutdenc.h>

#include "sti7108reg.h"
#include "sti7108device.h"
#include "sti7108auxoutput.h"
#include "sti7108mixer.h"
#include "sti7108clkdivider.h"

CSTi7108AuxOutput::CSTi7108AuxOutput(
    CSTi7108Device        *pDev,
    CSTmSDVTG             *pVTG,
    CSTmTVOutDENC         *pDENC,
    CSTi7108AuxMixer      *pMixer,
    CSTmFSynth            *pFSynth,
    CSTmHDFormatterAWG    *pAWG,
    CSTmHDFormatterOutput *pHDFOutput,
    CSTi7108ClkDivider    *pClkDivider): CSTmAuxTVOutput(pDev,
                                                         STi7108_MAIN_TVOUT_BASE,
                                                         STi7108_AUX_TVOUT_BASE,
                                                         STi7108_HD_FORMATTER_BASE,
                                                         pVTG,
                                                         pDENC,
                                                         pMixer,
                                                         pFSynth,
                                                         pAWG,
                                                         pHDFOutput)
{
  DENTRY();

  m_pClkDivider = pClkDivider;

  CSTi7108AuxOutput::DisableDACs();

  DEXIT();
}


void CSTi7108AuxOutput::StartClocks(const stm_mode_line_t *mode)
{
  DENTRY();

  m_pFSynth->SetAdjustment(0);
  m_pFSynth->Start(mode->TimingParams.ulPixelClock);

  m_pClkDivider->Enable(STM_CLK_PIX_SD, STM_CLK_SRC_1, STM_CLK_DIV_4);
  m_pClkDivider->Enable(STM_CLK_DISP_SD, STM_CLK_SRC_1, STM_CLK_DIV_8);

  DEXIT();
}


void CSTi7108AuxOutput::SetAuxClockToHDFormatter(void)
{
  DENTRY();
  /*
   * Switch the HD DAC clock to the SD clock with no divide
   */
  m_pClkDivider->Enable(STM_CLK_PIX_HD, STM_CLK_SRC_1, STM_CLK_DIV_1);

  DEXIT();
}


bool CSTi7108AuxOutput::ShowPlane(stm_plane_id_t planeID)
{
  DEBUGF2(2,("%s: planeID = %d\n",__PRETTY_FUNCTION__, (int)planeID));

  if(planeID == OUTPUT_GDP1)
  {
    DEBUGF2(2,("%s: GDP1 clock set to SD disp clock\n",__PRETTY_FUNCTION__));
    m_pClkDivider->Enable(STM_CLK_GDP1, STM_CLK_SRC_1, STM_CLK_DIV_8);
  }
  else if(planeID == OUTPUT_GDP2)
  {
    DEBUGF2(2,("%s: GDP2 clock set to SD disp clock\n",__PRETTY_FUNCTION__));
    m_pClkDivider->Enable(STM_CLK_GDP2, STM_CLK_SRC_1, STM_CLK_DIV_8);
  }
  else if (planeID == OUTPUT_VID1)
  {
    ULONG val = ReadSysCfgReg(SYS_CFG2) & ~SYS_CFG2_PIP_MODE;
    WriteSysCfgReg(SYS_CFG2,val);

    DEBUGF2(2,("%s: VID1 clock set to SD disp clock\n",__PRETTY_FUNCTION__));
    m_pClkDivider->Enable(STM_CLK_PIP, STM_CLK_SRC_1, STM_CLK_DIV_8);
  }

  return CSTmMasterOutput::m_pMixer->EnablePlane(planeID);
}


void CSTi7108AuxOutput::EnableDACs(void)
{
  ULONG val;

  DENTRY();

  val = ReadSysCfgReg(SYS_CFG3);

  if(m_ulOutputFormat & (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_RGB))
  {
    /*
     * We can blindly enable the HD DACs if we need them, it doesn't
     * matter if they were already in use by the HD pipeline.
     */
    DEBUGF2(2,("%s: Enabling HD DACs\n",__PRETTY_FUNCTION__));
    val &= ~(SYS_CFG3_DAC_HD_HZU_DISABLE |
             SYS_CFG3_DAC_HD_HZV_DISABLE |
             SYS_CFG3_DAC_HD_HZW_DISABLE);

    ULONG hddacs = ReadMainTVOutReg(TVOUT_PADS_CTL) & ~TVOUT_MAIN_PADS_DAC_POFF;
    WriteMainTVOutReg(TVOUT_PADS_CTL, hddacs);
  }

  if(m_ulOutputFormat & STM_VIDEO_OUT_CVBS)
  {
    DEBUGF2(2,("%s: Enabling CVBS DAC\n",__PRETTY_FUNCTION__));
    val &= ~SYS_CFG3_DAC_SD_HZU_DISABLE;
  }
  else
  {
    DEBUGF2(2,("%s: Tri-Stating CVBS DAC\n",__PRETTY_FUNCTION__));
    val |=  SYS_CFG3_DAC_SD_HZU_DISABLE;
  }

  if(m_ulOutputFormat & STM_VIDEO_OUT_YC)
  {
    DEBUGF2(2,("%s: Enabling Y/C DACs\n",__PRETTY_FUNCTION__));
    val &= ~(SYS_CFG3_DAC_SD_HZV_DISABLE |
             SYS_CFG3_DAC_SD_HZW_DISABLE);
  }
  else
  {
    DEBUGF2(2,("%s: Tri-Stating Y/C DACs\n",__PRETTY_FUNCTION__));
    val |=  (SYS_CFG3_DAC_SD_HZV_DISABLE |
             SYS_CFG3_DAC_SD_HZW_DISABLE);
  }

  WriteSysCfgReg(SYS_CFG3,val);

  ULONG sddacs = ReadAuxTVOutReg(TVOUT_PADS_CTL) & ~TVOUT_AUX_PADS_DAC_POFF;
  WriteAuxTVOutReg(TVOUT_PADS_CTL, sddacs);

  DEXIT();
}


void CSTi7108AuxOutput::DisableDACs(void)
{
  DENTRY();
  ULONG val;

  if(m_ulOutputFormat & (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_RGB))
  {
    /*
     * Disable HD DACs only if this pipeline using them
     */
    stm_clk_divider_output_source_t hdpixsrc;
    m_pClkDivider->getSource(STM_CLK_PIX_HD,&hdpixsrc);

    if(hdpixsrc == STM_CLK_SRC_1)
    {
      DEBUGF2(2,("%s: Disabling HD DACs\n",__PRETTY_FUNCTION__));
      val = ReadMainTVOutReg(TVOUT_PADS_CTL) | TVOUT_MAIN_PADS_DAC_POFF;
      WriteMainTVOutReg(TVOUT_PADS_CTL,val);
    }
  }

  val = ReadAuxTVOutReg(TVOUT_PADS_CTL) | TVOUT_AUX_PADS_DAC_POFF;
  WriteAuxTVOutReg(TVOUT_PADS_CTL, val);

  DEXIT();
}
