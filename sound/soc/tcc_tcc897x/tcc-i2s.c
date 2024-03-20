/*
 * linux/sound/soc/tcc/tcc-i2s.c
 *
 * Based on:    linux/sound/soc/pxa/pxa2xx-i2s.h
 * Author: Liam Girdwood<liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com>
 * Rewritten by:    <linux@telechips.com>
 * Created:     12th Aug 2005   Initial version.
 * Modified:    Nov 25, 2008
 * Description: ALSA PCM interface for the Intel PXA2xx chip
 *
 * Copyright 2005 Wolfson Microelectronics PLC.
 * Copyright (C) 2008-2009 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

//#include <linux/clk-private.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h> /* __clk_is_enabled */
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <asm/io.h>
#include <linux/pinctrl/consumer.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include <sound/initval.h>
#include <sound/soc.h>


#include "tcc-pcm.h"
#include "tcc-i2s.h"

#undef alsa_dbg
#if 0
#define alsa_dai_dbg(devid, f, a...)  printk("== alsa-debug I2S-%d == " f, devid, ##a)
#define alsa_dbg(f, a...)  printk("== alsa-debug I2S == " f, ##a)
#else
#define alsa_dai_dbg(devid, f, a...)
#define alsa_dbg(f, a...)
#endif

static int tcc_i2s_tx_enable(struct snd_pcm_substream *substream, int En);
static int tcc_i2s_rx_enable(struct snd_pcm_substream *substream, int En);

static unsigned int tcc_i2s_get_mclk_fs_for_af(unsigned int freq, struct clk	*dai_pclk)
{
	unsigned int fs=MAX_TCC_MCLK_FS, minfs=0, mdiff=freq;
	unsigned int temp=0;

#if defined(FIX_TCC_MCLK_FS)
	minfs = (unsigned int)FIX_TCC_MCLK_FS;
	temp = minfs*freq*M_TCC_MCLK;
	if(temp > MAX_TCC_AF_PCLK) {
		alsa_dbg("Your setting Freq.[%d]Hz, MCLK_FS[%d] can't do well. We doesn't guarantee over MCLK [%d]MHz.", freq, minfs, MAX_TCC_AF_PCLK/M_TCC_MCLK/(1000*1000));
	}
#else
	temp = MIN_TCC_MCLK_FS*freq*M_TCC_MCLK;
	if(temp > MAX_TCC_AF_PCLK) {
		alsa_dbg("Your setting Freq.[%d]Hz, min_MCLK_FS[%d] can't do well. We doesn't guarantee over MCLK [%d]MHz.", freq, MIN_TCC_MCLK_FS, (MAX_TCC_AF_PCLK/M_TCC_MCLK)/(1000*1000));
		return MIN_TCC_MCLK_FS;
	}
	while((fs>=MIN_TCC_MCLK_FS)&&(fs<=MAX_TCC_MCLK_FS)){
		temp = fs*freq*M_TCC_MCLK;
		if(temp > MAX_TCC_AF_PCLK) {
			goto next;
		}

		clk_set_rate(dai_pclk, freq*fs);
		temp = clk_get_rate(dai_pclk);
		temp = abs(freq*fs-temp);
		if(temp < mdiff){
			mdiff=temp;
			minfs=fs;
		}
next:
		fs=fs/2;
	}
#endif
	return minfs;
}

static unsigned int tcc_i2s_get_mclk_fs(unsigned int freq, struct clk	*dai_pclk)
{
	unsigned int fs=1024, minfs=0, mdiff=freq;
	unsigned int temp=0;

#if defined(FIX_TCC_MCLK_FS)
	minfs = (unsigned int)FIX_TCC_MCLK_FS;
	temp = minfs*freq;
	if(temp > MAX_TCC_MCLK) {
		alsa_dbg("Your setting Freq.[%d]Hz, MCLK_FS[%d] can't do well. We doesn't guarantee over MCLK [%d]MHz.", freq, minfs, (MAX_TCC_MCLK/(1000*1000)));
	}
#else
#if defined(CONFIG_SND_SOC_AK4601)
	//Codec AK4601 can't lock PLL in low Freq.
	if(freq <= 24000) {
		return MAX_TCC_MCLK_FS;
	}
#endif
	temp = MIN_TCC_MCLK_FS*freq;
	if(temp > MAX_TCC_MCLK) {
		alsa_dbg("Your setting Freq.[%d]Hz, min_MCLK_FS[%d] can't do well. We doesn't guarantee over MCLK [%d]MHz.", freq, MIN_TCC_MCLK_FS, (MAX_TCC_MCLK/(1000*1000)));
		return MIN_TCC_MCLK_FS;
	}
	while((fs>=MIN_TCC_MCLK_FS)&&(fs<=MAX_TCC_MCLK_FS)){
		temp = fs*freq;
		if(temp > MAX_TCC_MCLK) {
			goto next;
		}

		clk_set_rate(dai_pclk, freq*fs);
		temp = clk_get_rate(dai_pclk);
		temp = abs(freq*fs-temp);
		if(temp < mdiff){
			mdiff=temp;
			minfs=fs;
		}
next:
		fs=fs/2;
	}
#endif
	return minfs;
}

static void tcc_i2s_set_clock(struct snd_soc_dai *dai, unsigned int req_lrck, bool stream)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	struct tcc_i2s_data *tcc_i2s = prtd->private_data;
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;
	unsigned int pre_mclk=0, tDAMR=0, temp=0;

	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);
	tDAMR = i2s_readl(pdai_reg+I2S_DAMR);

	pre_mclk = clk_get_rate(prtd->ptcc_clk->dai_pclk);

	if((tcc_i2s->dai_clk_rate[0] != req_lrck)
			||(tcc_i2s->dai_clk_rate[1] != pre_mclk)){

		if(tDAMR & Hw15){
			temp = tDAMR & (~Hw15);	//DAI Disable
			i2s_writel(temp, pdai_reg+I2S_DAMR);
		}

		if(prtd->ptcc_clk->dai_pclk) {
			if(__clk_is_enabled(prtd->ptcc_clk->dai_pclk))
				clk_disable_unprepare(prtd->ptcc_clk->dai_pclk);

			if(prtd->ptcc_clk->af_pclk) {
				temp = tcc_i2s_get_mclk_fs_for_af(req_lrck, prtd->ptcc_clk->dai_pclk);
			} else {
#if !defined(CONFIG_SND_SOC_AK4601)
				//Codec AK4601 can't lock PLL in low Freq.
				if(req_lrck <= 24000) {
					temp = MAX_TCC_MCLK_FS;
				} else {
					temp = tcc_i2s_get_mclk_fs(req_lrck, prtd->ptcc_clk->dai_pclk);
				}
#else
					temp = tcc_i2s_get_mclk_fs(req_lrck, prtd->ptcc_clk->dai_pclk);
#endif
			}

			clk_set_rate(prtd->ptcc_clk->dai_pclk, req_lrck*temp);

			alsa_dai_dbg(prtd->id, "CKC_CHECK: set_rate[%d], get_rate[%lu], fs[%d]\n", (req_lrck*temp), clk_get_rate(prtd->ptcc_clk->dai_pclk), temp);

			tDAMR &= ~(Hw8-Hw4);
			switch(temp) {
				case 256:
					tDAMR |= (Hw5);
					break;
				case 512:
					tDAMR |= (Hw7|Hw5);
					break;
				case 1024:
					tDAMR |= (Hw7|Hw6|Hw5);
					break;
				default:
					break;
			}

			clk_prepare_enable(prtd->ptcc_clk->dai_pclk);

			tcc_i2s->dai_clk_rate[0] = req_lrck;
			tcc_i2s->dai_clk_rate[1] = clk_get_rate(prtd->ptcc_clk->dai_pclk);
		}
	}
	i2s_writel(tDAMR, pdai_reg+I2S_DAMR);

}

static int tcc_i2s_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	struct tcc_i2s_data *tcc_i2s = prtd->private_data;
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;
	unsigned int reg_value=0;

	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);

	if (!dai->active) {

		if(tcc_i2s->dai_en_ctrl) {
			alsa_dai_dbg(prtd->id, "[%s] Enable DAI\n", __func__);
			//DAI Enable Bit countrol For output clock control
			reg_value = i2s_readl(pdai_reg+I2S_DAMR);
			//Hw15: DAI Enable
			if(!(reg_value & Hw15)) {
				reg_value |= Hw15;
				i2s_writel(reg_value, pdai_reg+I2S_DAMR);
			}
		}
	}

	return 0;
}

static int tcc_i2s_set_dai_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(cpu_dai->dev);
	struct tcc_i2s_data *tcc_i2s = prtd->private_data;
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;
	unsigned int reg_value=0;
	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);

	tcc_i2s->dai_en_ctrl = 0;
	/* MASTER/SLAVE mode */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBM_CFS:	//If codec is clk master, frm slave. (MASTER MODE)
			//This case of...
			//1. This is for MCLK, BCLK, LRCK(Frame-clk) output from TCC.
			//2. Audio Codec Chip core clock doesn't connect from TCC.
			//3. MCLK, BCLK, LRCK should be output only in operate.

			tcc_i2s->dai_en_ctrl = 1;	//DAI Enable Bit countrol For output clock control

			reg_value = i2s_readl(pdai_reg+I2S_DAMR);

			//Hw31: BCLK direct at master mode.
			//Hw30: LRCLK direct at master mode	.
			reg_value |= (Hw31 | Hw30);
			//Hw11: DAI System Clock master select
			//Hw10: DAI Bit Clock master select
			//Hw9: DAI Frame clock(LRCLK) master selcet
			reg_value |= (Hw11 | Hw10 | Hw9 );

			i2s_writel(reg_value, pdai_reg+I2S_DAMR);

			break;

		case SND_SOC_DAIFMT_CBS_CFS:	//If codec is clk, frm slave. (MASTER MODE)
			//This case of...
			//1. This is for MCLK, BCLK, LRCK(Frame-clk) output from TCC.
			//2. Audio Codec Chip core clock from TCC.
			//3. Always MCLK, BCLK, LRCK output.

			reg_value = i2s_readl(pdai_reg+I2S_DAMR);

			//Hw31: BCLK direct at master mode.
			//Hw30: LRCLK direct at master mode	.
			reg_value |= (Hw31 | Hw30);
			//Hw11: DAI System Clock master select
			//Hw10: DAI Bit Clock master select
			//Hw9: DAI Frame clock(LRCLK) master selcet
			reg_value |= (Hw11 | Hw10 | Hw9 );
			//Hw15: DAI Enable
			if(tcc_i2s->dai_en_ctrl == 0)
				reg_value |= (Hw15);

			i2s_writel(reg_value, pdai_reg+I2S_DAMR);

			break;

		case SND_SOC_DAIFMT_CBM_CFM:	//If codec is clk, frm master. (SLAVE MODE)
			//This case of...
			//1. This is for MCLK, BCLK, LRCK(Frame-clk) input from Audio Codec or DSP Chip.
			//2. TCC Audio core clock from TCC Audio Peri-clock (ref. DAMR[11]).
			//3. BCLK, LRCK should be input in always.


			reg_value = i2s_readl(pdai_reg+I2S_DAMR);

			//if(prtd->ptcc_clk->af_pclk != NULL)reg_value |= (Hw27);	//Audio Filter Enable.

			//Hw31: BCLK direct at master mode.
			//Hw30: LRCLK direct at master mode	.
			reg_value &= ~(Hw31|Hw30);
			//Hw11: DAI System Clock master select external pin
			//Hw10: DAI Bit Clock master select external pin
			//Hw9: DAI Frame clock(LRCLK) master selcet external pin
			reg_value &= ~(Hw12-Hw9);
			reg_value |= (Hw11);	//DAI System Clock From TCC Audio Peri-clock
			//Hw15: DAI Enable
			reg_value |= (Hw15);

			i2s_writel(reg_value, pdai_reg+I2S_DAMR);

			break;
		default:
			break;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			reg_value = i2s_readl(pdai_reg+I2S_DAMR);
			//Hw25: DSP mode [0] I2S mode
			//Hw12: DAI Sync mode [0] I2S mode
			reg_value &= ~(Hw25 | Hw12);
			i2s_writel(reg_value, pdai_reg+I2S_DAMR);
			break;
		case SND_SOC_DAIFMT_RIGHT_J:
			//RIGHT_J RX doesn't work well when BCLK_RATIO is 32fs.
			//but, the waveform is the same as LEFT_J when BCLK_RATIO is 32fs
			reg_value = i2s_readl(pdai_reg+I2S_DAMR);
			//Hw25: DSP mode [0] I2S mode
			//Hw24: RX justified mode [1] Right-justified mode
			//Hw23: TX justified mode [1] Right-justified mode
			//Hw12: DAI Sync mode [1] Left/Right-justified mode
			reg_value &= ~(Hw12 | Hw23 | Hw24 | Hw25);
			reg_value |= (Hw12 | Hw23 | Hw24);
			//reg_value |= (Hw12 | Hw23);	//This line is for BCLK_RATIO 32fs
			i2s_writel(reg_value, pdai_reg+I2S_DAMR);
			break;
		case SND_SOC_DAIFMT_LEFT_J:
			reg_value = i2s_readl(pdai_reg+I2S_DAMR);
			//Hw25: DSP mode [0] I2S mode
			//Hw24: RX justified mode [0] Left-justified mode
			//Hw23: TX justified mode [0] Left-justified mode
			//Hw12: DAI Sync mode [1] Left/Right-justified mode
			reg_value &= ~(Hw12 | Hw23 | Hw24 | Hw25);
			reg_value |= (Hw12);
			i2s_writel(reg_value, pdai_reg+I2S_DAMR);
			break;
		case SND_SOC_DAIFMT_DSP_A:
			printk("Error!! TCC897x support only DSP Early mode!!!\n");
			break;

		case SND_SOC_DAIFMT_DSP_B:
#if 0	//This is reference code.
			reg_value = i2s_readl(pdai_reg+I2S_DAMR);
			//Hw26: DSP mode bit width [1] 16bit
			//Hw25: DSP mode [1] DSP mode
			//Hw24: RX justified mode [0] Left-justified mode
			//Hw23: TX justified mode [0] Left-justified mode
			//Hw12: DAI Sync mode [1] Left/Right-justified mode
			reg_value &= ~(Hw12 | Hw23 | Hw24 | Hw25 | Hw26);
			reg_value |= (Hw26 | Hw25);
			i2s_writel(reg_value, pdai_reg+I2S_DAMR);

			reg_value = i2s_readl(pdai_reg+I2S_MCCR0);
			//Hw25: Frame Begin Position (DSP)
			//Hw24-Hw16: Frame End Position (DSP)
			//Hw15-Hw14: TDM mode
			//Hw11-Hw10: Frame Clock Divider
			//Hw9: Frame Invert (DSP)
			//Hw0-Hw8: Frame Size (DSP)
			reg_value &= ~((Hw9-Hw0) | Hw9 | Hw10 | Hw11 | Hw14 | Hw15 | (Hw25-Hw16) | Hw25);
			reg_value |= Hw14 | Hw11 | Hw10 | (Hw6-Hw0);
			i2s_writel(reg_value, pdai_reg+I2S_MCCR0);
#endif
			break;
	}
	return 0;
}

static int tcc_i2s_set_dai_sysclk(struct snd_soc_dai *cpu_dai,
		int clk_id, unsigned int freq, int dir)
{
	/*
	   struct tcc_runtime_data *prtd = dev_get_drvdata(cpu_dai->dev);
	   alsa_dai_dbg(prtd->id, "[%s] \n", __func__);

	   if (clk_id != TCC_I2S_SYSCLK)
	   return -ENODEV;
	   */
	return 0;
}

static int tcc_i2s_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	//struct tcc_pcm_dma_params *dma_data;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	struct tcc_i2s_data *tcc_i2s = prtd->private_data;
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;
	snd_pcm_format_t format=params_format(params);
	unsigned int chs = params_channels(params);
	unsigned int temp=0, tDAMR=0;

	alsa_dai_dbg(prtd->id, "[%s] check device(%d)\n", __func__,substream->pcm->device);

	// Planet 20160113 Add CDIF Bypass Start
	if((prtd->id == 0)&&(params->reserved[0] & 0x01)) {
		alsa_dbg("~~~!!! tcc_pcm_hw_params() CDIF Bypass Enable !!!~~~\n");
		tcc_i2s->cdif_bypass_en = 1;
	} else {
		alsa_dbg("~~~!!! tcc_pcm_hw_params() CDIF Bypass Disable !!!~~~\n");
		tcc_i2s->cdif_bypass_en = 0;
	}
	// Planet 20160113 Add CDIF Bypass End

	tDAMR = i2s_readl(pdai_reg+I2S_DAMR);

	/* For HDMI IN */
#if defined(CONFIG_VIDEO_HDMI_IN_SENSOR)
	if(params_format(params) == SNDRV_PCM_FORMAT_U16) {

		printk("~~~!!! HDMI In Setting Enable !!!~~~\n");
		tDAMR &= ~(Hw11 | Hw10 | Hw9);

	} else {

		printk("~~~!!! HDMI In Setting Disable !!!~~~\n");
		tDAMR |= (Hw11 | Hw10 | Hw9);

	}
#endif
	//Set bit width, FMT: 16bit mode S16LE / 24bit mode S24LE
	if (format == SNDRV_PCM_FORMAT_S24_LE) { //24bit mode S24LE
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			tDAMR &= ~(Hw19|Hw18);
			tDAMR |= Hw19;
		} else {
			tDAMR &= ~(Hw21|Hw20);
			tDAMR |= Hw21;
		}
		alsa_dai_dbg(prtd->id, "%s SNDRV_PCM_FORMAT_S24_LE reg 0x%x\n", __func__, tDAMR);
	} else {	//16bit mode S16LE
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			tDAMR |= (Hw19|Hw18);
		} else {
			tDAMR |= (Hw21|Hw20);
		}

		alsa_dai_dbg(prtd->id, "%s SNDRV_PCM_FORMAT_S16_LE reg 0x%x\n", __func__, tDAMR);
	}

	i2s_writel(tDAMR, pdai_reg+I2S_DAMR);

	tcc_i2s_set_clock(dai, params_rate(params), substream->stream);

	tDAMR = i2s_readl(pdai_reg+I2S_DAMR);

	/* Multi-channel or Stereo Channel setting */
	//Hw29: DAI Buffer Threshold Enable
	//Hw28: DAI Multi-port Enable
	switch(chs) {
		case 8:
			/*	//TCC doesn't support 5, 6, 7 channels
				case 7:
				case 6:
				case 5:
				case 4:
				case 3:
				*/
			tDAMR |= (Hw29|Hw28);
			break;
		case 2:
		case 1:
			tDAMR &= ~Hw28;
			tDAMR |= Hw29;
			break;
		default:
			printk("ERROR!! %d channel is not supported!\n", chs);
	}
	i2s_writel(tDAMR, pdai_reg+I2S_DAMR);

	temp = pcm_readl(pdai_reg+I2S_MCCR0);
	temp &= ~(Hw31 | Hw30 | Hw29 | Hw28); // DAO_0, 1, 2, 3 Mask Disable
	pcm_writel(temp, pdai_reg+I2S_MCCR0);

	alsa_dai_dbg(prtd->id, "===================\n");
	alsa_dai_dbg(prtd->id, "rate        : %d\n", params_rate(params));
	alsa_dai_dbg(prtd->id, "channels    : %d\n", params_channels(params));
	alsa_dai_dbg(prtd->id, "period_size : %d\n", params_period_size(params));

	return 0;
}


static int tcc_i2s_tx_enable(struct snd_pcm_substream *substream, int En)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;   // dai base addr
	unsigned int reg_value=0, ret=0;

	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);
	if(En) {

		reg_value = pcm_readl(pdai_reg+I2S_DAMR);
		reg_value |= Hw14;  //DAI TX Enable
		pcm_writel(reg_value, pdai_reg+I2S_DAMR);

	} else {

		reg_value = pcm_readl(pdai_reg+I2S_DAMR);
		reg_value &= ~Hw14;  //DAI TX Disable
		pcm_writel(reg_value, pdai_reg+I2S_DAMR);

	}

	return ret;

}

static int tcc_i2s_rx_enable(struct snd_pcm_substream *substream, int En)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;   // dai base addr
	unsigned int reg_value=0, ret=0;

	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);
	if(En) {

		reg_value = pcm_readl(pdai_reg+I2S_DAMR);
		//reg_value |= Hw17;    //This is loop-back setting for Debug. Direction: TX->RX
		reg_value |= Hw13;  //DAI RX Enable
		pcm_writel(reg_value, pdai_reg+I2S_DAMR);

	} else {

		reg_value = pcm_readl(pdai_reg+I2S_DAMR);
		//reg_value &= ~Hw17;    //This is loop-back setting for Debug. Direction: TX->RX
		reg_value &= ~Hw13;  //DAI RX Disable
		pcm_writel(reg_value, pdai_reg+I2S_DAMR);

	}

	return ret;
}

static void tcc_dai_mute(struct device *dev, bool enable)
{
	struct pinctrl *pin;
	if(enable) {
		pin = pinctrl_get_select(dev, "dai_mute");
	} else {
		pin = pinctrl_get_select(dev, "default");
	}
	if(pin == NULL) {
		printk("%s() has some wrong.\n", __func__);
	}
}

static int tcc_i2s_trigger(struct snd_pcm_substream *substream, int cmd, struct snd_soc_dai *dai)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	struct tcc_i2s_data *tcc_i2s = prtd->private_data;
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;
	unsigned int reg_value=0;
	int ret=1;

	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {

				alsa_dbg("%s() +playback start\n", __func__);
				// Planet 20160113 Add CDIF Bypass Start
				if((prtd->id == 0)&&(tcc_i2s->cdif_bypass_en == 1)) {

					reg_value = pcm_readl(pdai_reg+CDIF_BYPASS_CICR);
					reg_value |= Hw7;           // CDIF Enable
					i2s_writel(reg_value, pdai_reg+CDIF_BYPASS_CICR);

					reg_value = pcm_readl(pdai_reg+I2S_DAMR);
					reg_value |= (Hw2 | Hw8);           // CDIF Monitor Mode & CDIF Clock Select
					i2s_writel(reg_value, pdai_reg+I2S_DAMR);

				}
				// Planet 20160113 Add CDIF Bypass End
				i2s_writel(0x10, pdai_reg+I2S_DAVC); /* Mute ON */
				ret = tcc_i2s_tx_enable(substream, 1);
				i2s_writel(0x0, pdai_reg+I2S_DAVC); /* Mute OFF */

				alsa_dbg("%s() -playback start ret=%d\n", __func__, ret);

			} else {	//for SNDRV_PCM_STREAM_CAPTURE

				alsa_dbg("%s() +capture start\n", __func__);
				ret = tcc_i2s_rx_enable(substream, 1);

				alsa_dbg("%s() -capture start ret=%d\n", __func__, ret);

			}
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				alsa_dbg("%s() +playback stop\n", __func__);
				pcm_writel(0x10, pdai_reg+I2S_DAVC); /* Mute ON */
				// Planet 20160113 Add CDIF Bypass Start
				if((prtd->id == 0)&&(tcc_i2s->cdif_bypass_en == 1)) {
					reg_value = pcm_readl(pdai_reg+CDIF_BYPASS_CICR);
					reg_value &= ~Hw7;          // CDIF Disable
					pcm_writel(reg_value, pdai_reg+CDIF_BYPASS_CICR);

					reg_value = pcm_readl(pdai_reg+I2S_DAMR);
					reg_value &= ~(Hw2 | Hw8);          // CDIF Monitor Mode & CDIF Clock Select
					pcm_writel(reg_value, pdai_reg+I2S_DAMR);
					tcc_i2s->cdif_bypass_en = 0;
				}

			} else {	//for SNDRV_PCM_STREAM_CAPTURE
				alsa_dbg("%s() +capture stop\n", __func__);
			}
			break;
		default:
			ret = -EINVAL;
	}
	return ret;
}

static void tcc_i2s_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	//struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	//TCC897x, TCC570x can't support this step.
	//There are Many different thing in each chip's DAI stop seq.
	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);
}

static int tcc_i2s_suspend(struct device *dev)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dev);
	struct tcc_i2s_data *tcc_i2s = prtd->private_data;
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;
	unsigned int tMCCR1=0;

	tcc_i2s->backup_dai = (struct tcc_i2s_backup_reg *)kmalloc(sizeof(struct tcc_i2s_backup_reg), GFP_KERNEL);
	if(!tcc_i2s->backup_dai) {
		alsa_dai_dbg(prtd->id, "[%s] no memory for backup_dai\n", __func__);
		return -1;
	}
	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);

	// Backup's all about dai control.
	tcc_i2s->backup_dai->reg_damr = i2s_readl(pdai_reg+I2S_DAMR);
	tcc_i2s->backup_dai->reg_davc = i2s_readl(pdai_reg+I2S_DAVC);
	tcc_i2s->backup_dai->reg_mccr0 = i2s_readl(pdai_reg+I2S_MCCR0);
	tMCCR1 = i2s_readl(pdai_reg+I2S_MCCR1);
	if(tMCCR1) {
		tcc_i2s->backup_dai->reg_mccr1 = tMCCR1;
	} else {
		tcc_i2s->backup_dai->reg_mccr1 = 0;
	}

	// Disable all about dai clk
	if(__clk_is_enabled(prtd->ptcc_clk->af_pclk)) {
		clk_disable_unprepare(prtd->ptcc_clk->af_pclk);
	}
	if(__clk_is_enabled(prtd->ptcc_clk->dai_pclk)) {
		clk_disable_unprepare(prtd->ptcc_clk->dai_pclk);
	}
	if(__clk_is_enabled(prtd->ptcc_clk->dai_hclk)) {
		clk_disable_unprepare(prtd->ptcc_clk->dai_hclk);
	}

	return 0;
}

static int tcc_i2s_resume(struct device *dev)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dev);
	struct tcc_i2s_data *tcc_i2s = prtd->private_data;
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;

	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);

	// Enable all about dai clk
	if(prtd->ptcc_clk->dai_hclk)
		clk_prepare_enable(prtd->ptcc_clk->dai_hclk);
	if(prtd->ptcc_clk->dai_pclk)
		clk_prepare_enable(prtd->ptcc_clk->dai_pclk);
	if(prtd->ptcc_clk->af_pclk)
		clk_prepare_enable(prtd->ptcc_clk->af_pclk);

	// Set dai reg from backup's.
	i2s_writel(tcc_i2s->backup_dai->reg_damr, pdai_reg+I2S_DAMR);
	i2s_writel(tcc_i2s->backup_dai->reg_davc, pdai_reg+I2S_DAVC);
	i2s_writel(tcc_i2s->backup_dai->reg_mccr0, pdai_reg+I2S_MCCR0);
	i2s_writel(tcc_i2s->backup_dai->reg_mccr1, pdai_reg+I2S_MCCR1);

	kfree(tcc_i2s->backup_dai);
	return 0;
}

static int tcc_i2s_probe(struct snd_soc_dai *dai)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	struct tcc_i2s_data *tcc_i2s = prtd->private_data;
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;
	unsigned int reg_value=0;
	int i;
/*	// When default setting is 24bit mode.
	unsigned int bitmode_24=0;
*/
	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);

	/* clock enable */
	if (prtd->ptcc_clk->dai_hclk)
		clk_prepare_enable(prtd->ptcc_clk->dai_hclk);

	//* init dai register *//

	i2s_writel(0, pdai_reg+I2S_DAVC);

	/* DAMR init */
	reg_value = (Hw29	//dai buffer threshold enable
			| Hw21| Hw20 	//dai rx shift bit-pack lsb and 16bit mode
			| Hw19| Hw18 	//dai tx shift bit-pack lsb and 16bit mode
			);

	if (prtd->ptcc_clk->af_pclk) {
		reg_value |= (Hw27);	//dai Audio Filter enable
	}

	/*
	//Hw15: DAI Enable
	//Hw14: DAI TX Enable
	//Hw13: DAI RX Enable
	reg_value &= ~(Hw15 | Hw14 | Hw13);	//dai disable
	*/
	/*
	if(bitmode_24){	//If default Bit mode is 24bits.
	   reg_value |= Hw21	//dai rx shift bit-pack lsb and 24bit mode
	   |Hw19;	//dai tx shift bit-pack lsb and 24bit mode
	}
	//printk("%s === default set reg 0x%x\n",__func__,reg_value);
	*/
	i2s_writel(reg_value, pdai_reg+I2S_DAMR);

	/* MCCR0 init */
	reg_value = (Hw31 | Hw30 | Hw29 | Hw28); // DAO_0, 1, 2, 3 mask enable.
	i2s_writel(reg_value, pdai_reg+I2S_MCCR0);

//This is for FIFO clear in first time.
	tcc_dai_mute(dai->dev, 1);	//For DAI RX FIFO clear
	reg_value = pcm_readl(pdai_reg+I2S_DRMR);
	reg_value |= (Hw15|Hw14|Hw13);  //DAI enable, DAI RX/TX enable
	pcm_writel(reg_value, pdai_reg+I2S_DRMR);
	for(i=0; i<64; i++) {
		i2s_writel(0, pdai_reg+I2S_DADOR0);
		i2s_writel(0, pdai_reg+I2S_DADOR1);
		i2s_writel(0, pdai_reg+I2S_DADOR2);
		i2s_writel(0, pdai_reg+I2S_DADOR3);
		i2s_writel(0, pdai_reg+I2S_DADOR4);
		i2s_writel(0, pdai_reg+I2S_DADOR5);
		i2s_writel(0, pdai_reg+I2S_DADOR6);
		i2s_writel(0, pdai_reg+I2S_DADOR7);
		reg_value = i2s_readl(pdai_reg+I2S_DADIR0);
		reg_value = i2s_readl(pdai_reg+I2S_DADIR1);
		reg_value = i2s_readl(pdai_reg+I2S_DADIR2);
		reg_value = i2s_readl(pdai_reg+I2S_DADIR3);
		reg_value = i2s_readl(pdai_reg+I2S_DADIR4);
		reg_value = i2s_readl(pdai_reg+I2S_DADIR5);
		reg_value = i2s_readl(pdai_reg+I2S_DADIR6);
		reg_value = i2s_readl(pdai_reg+I2S_DADIR7);
	}
	mdelay(1);
	reg_value = pcm_readl(pdai_reg+I2S_DRMR);
	reg_value &= ~(Hw15|Hw14|Hw13);  //DAI disable, DAI RX/TX disable
	pcm_writel(reg_value, pdai_reg+I2S_DRMR);
	tcc_dai_mute(dai->dev, 0);	//For DAI RX FIFO clear

	/* set All about dai clk */

	if (prtd->ptcc_clk->af_pclk) {
		if(__clk_is_enabled(prtd->ptcc_clk->af_pclk))
			clk_disable_unprepare(prtd->ptcc_clk->af_pclk);

		clk_set_rate(prtd->ptcc_clk->af_pclk, MAX_TCC_AF_PCLK);
		clk_prepare_enable(prtd->ptcc_clk->af_pclk);
		//alsa_dai_dbg(prtd->id, "[%s] Audio Filter Clock Set [%d]MHz \n", __func__, (MAX_TCC_AF_PCLK/(1000*1000)));
	}

	if (prtd->ptcc_clk->dai_pclk) {
		if(__clk_is_enabled(prtd->ptcc_clk->dai_pclk))
			clk_disable_unprepare(prtd->ptcc_clk->dai_pclk);

		tcc_i2s_set_clock(dai, tcc_i2s->dai_clk_rate[0], SNDRV_PCM_STREAM_PLAYBACK);

		//alsa_dai_dbg(prtd->id, "[%s]default sample rate[%d]Hz \n", __func__,tcc_i2s->dai_clk_rate[0]);
	}

	return 0;
}

static struct snd_soc_dai_ops tcc_i2s_ops = {
	.set_sysclk = tcc_i2s_set_dai_sysclk,
	.set_fmt    = tcc_i2s_set_dai_fmt,
	.startup    = tcc_i2s_startup,
	.shutdown   = tcc_i2s_shutdown,
	.hw_params  = tcc_i2s_hw_params,
	.trigger    = tcc_i2s_trigger,
};

static struct snd_soc_dai_driver tcc_i2s_dai = {
	.name = "tcc-dai-i2s",
	.probe = tcc_i2s_probe,
	//.suspend = tcc_i2s_suspend,
	//.resume = tcc_i2s_resume,
	.playback = {
		.channels_min = 1,
		.channels_max = 8,
		.rates = TCC_I2S_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 8,
		.rates = TCC_I2S_RATES,
#if defined(CONFIG_VIDEO_HDMI_IN_SENSOR)
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S24_LE,
#else
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
#endif
	},
	.ops   = &tcc_i2s_ops,
	.symmetric_rates = 1,
	.symmetric_channels = 1,
	//.symmetric_samplebits = 1,
};

/* This is for Stereo-I2S in TCC897x, TCC570x */
static struct snd_soc_dai_driver tcc_i2s_dai_stereo = {
	.name = "tcc-dai-i2s",
	.probe = tcc_i2s_probe,
	//.suspend = tcc_i2s_suspend,
	//.resume = tcc_i2s_resume,
	.playback = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = TCC_I2S_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	},
	.capture = {
		.channels_min = 1,
		.channels_max = 2,
		.rates = TCC_I2S_RATES,
#if defined(CONFIG_VIDEO_HDMI_IN_SENSOR)
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_S24_LE,
#else
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
#endif
	},
	.ops   = &tcc_i2s_ops,
	.symmetric_rates = 1,
	.symmetric_channels = 1,
	//.symmetric_samplebits = 1,
};

static const struct snd_soc_component_driver tcc_i2s_component = {
	.name		= "tcc-i2s",
};

static int soc_tcc_i2s_probe(struct platform_device *pdev)
{
	struct device_node *of_node_adma;
	struct platform_device *pdev_adma;
	struct tcc_adma_drv_data *pdev_adma_data;
	struct tcc_runtime_data *prtd;
	struct tcc_i2s_data *tcc_i2s;
	struct tcc_audio_reg *tcc_reg;
	struct tcc_audio_clk *tcc_clk;
	struct tcc_audio_intr *tcc_intr;
	u32 clk_rate;
	int ret = 0;

	/* Allocation for keeping Device Tree Info. START */
	prtd = devm_kzalloc(&pdev->dev, sizeof(struct tcc_runtime_data), GFP_KERNEL);
	if (!prtd) {
		dev_err(&pdev->dev, "no memory for tcc_runtime_data\n");
		goto err;
	}
	dev_set_drvdata(&pdev->dev, prtd);

	memset(prtd, 0, sizeof(struct tcc_runtime_data));

	tcc_i2s = devm_kzalloc(&pdev->dev, sizeof(struct tcc_i2s_data), GFP_KERNEL);
	if (!tcc_i2s) {
		dev_err(&pdev->dev, "no memory for tcc_i2s_data\n");
		goto err;
	}
	prtd->private_data = tcc_i2s;
	memset(tcc_i2s, 0, sizeof(struct tcc_i2s_data));

	tcc_reg = devm_kzalloc(&pdev->dev, sizeof(struct tcc_audio_reg), GFP_KERNEL);
	if (!tcc_reg) {
		dev_err(&pdev->dev, "no memory for tcc_audio_reg\n");
		ret = -ENOMEM;
		goto err;
	}
	prtd->ptcc_reg = tcc_reg;
	memset(tcc_reg, 0, sizeof(struct tcc_audio_reg));

	tcc_clk = devm_kzalloc(&pdev->dev, sizeof(struct tcc_audio_clk), GFP_KERNEL);
	if (!tcc_clk) {
		dev_err(&pdev->dev, "no memory for tcc_audio_clk\n");
		ret = -ENOMEM;
		goto err;
	}
	prtd->ptcc_clk = tcc_clk;
	memset(tcc_clk, 0, sizeof(struct tcc_audio_clk));

	tcc_intr = devm_kzalloc(&pdev->dev, sizeof(struct tcc_audio_intr), GFP_KERNEL);
	if (!tcc_intr) {
		dev_err(&pdev->dev, "no memory for tcc_audio_intr\n");
		ret = -ENOMEM;
		goto err;
	}
	prtd->ptcc_intr = tcc_intr;
	memset(tcc_intr, 0, sizeof(struct tcc_audio_intr));
	/* Allocation for keeping Device Tree Info. END */

	/* Flag set I2S for getting I/F name in PCM driver */
	prtd->id = -1;
	prtd->id = of_alias_get_id(pdev->dev.of_node, "i2s");
	if(prtd->id < 0) {
		dev_err(&pdev->dev, "i2s id[%d] is wrong.\n", prtd->id);
		ret = -EINVAL;
		goto err;
	}
	prtd->flag = TCC_I2S_FLAG;

	alsa_dai_dbg(prtd->id, "[%s] \n", __func__);
	/* get dai info. */
	prtd->ptcc_reg->dai_reg = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR((void *)prtd->ptcc_reg->dai_reg))
		prtd->ptcc_reg->dai_reg = NULL;

	/* get dai clk info. */
	prtd->ptcc_clk->dai_pclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(prtd->ptcc_clk->dai_pclk))
		prtd->ptcc_clk->dai_pclk = NULL;
	prtd->ptcc_clk->dai_hclk = of_clk_get(pdev->dev.of_node, 1);
	if (IS_ERR(prtd->ptcc_clk->dai_hclk))
		prtd->ptcc_clk->dai_hclk = NULL;
	prtd->ptcc_clk->af_pclk = of_clk_get(pdev->dev.of_node, 2);
	if (IS_ERR(prtd->ptcc_clk->af_pclk))
		prtd->ptcc_clk->af_pclk = NULL;

	of_property_read_u32(pdev->dev.of_node, "clock-frequency", &clk_rate);
	if(clk_rate > 192000)clk_rate=48000;	//First Default Sample Rate
	tcc_i2s->dai_clk_rate[0] = (unsigned long)clk_rate;
	tcc_i2s->dai_clk_rate[1] = 1; //Initialize value for set clock.
	//'0' can't through if condition in tcc_spdif_set_clock()

	/* get adma info */
	of_node_adma = of_parse_phandle(pdev->dev.of_node, "adma", 0);
	if (of_node_adma) {

		pdev_adma = of_find_device_by_node(of_node_adma);
		prtd->ptcc_reg->adma_reg = of_iomap(of_node_adma, 0);
		if (IS_ERR((void *)prtd->ptcc_reg->adma_reg))
			prtd->ptcc_reg->adma_reg = NULL;
		else
			alsa_dbg("adma_reg=0x%08x\n", (unsigned int)prtd->ptcc_reg->adma_reg);

		prtd->adma_irq = platform_get_irq(pdev_adma, 0);

		if(platform_get_drvdata(pdev_adma) == NULL) {
			alsa_dai_dbg(prtd->id, "adma dev data is NULL\n");
			ret = -EINVAL;
			goto err_reg_component;
		} else {
			alsa_dai_dbg(prtd->id, "adma dev data isn't NULL\n");
			pdev_adma_data = platform_get_drvdata(pdev_adma);
		}

		if(pdev_adma_data->slock == NULL) {
			alsa_dai_dbg(prtd->id, "pdev_adma_data->slock has some error.\n");
			ret = -EINVAL;
			goto err_reg_component;
		} else {
			prtd->adma_slock = pdev_adma_data->slock;
		}

	} else {
		alsa_dai_dbg(prtd->id, "of_node_adma is NULL\n");
		ret = -EINVAL;
		goto err;
	}

	//TCC897x, TCC570x Audio3-I2S doesn't support multi-channel.
	if(prtd->id == 3){
		ret = devm_snd_soc_register_component(&pdev->dev, &tcc_i2s_component,
				&tcc_i2s_dai_stereo, 1);
	} else {
		ret = devm_snd_soc_register_component(&pdev->dev, &tcc_i2s_component,
				&tcc_i2s_dai, 1);
	}
	if (ret)
		goto err_reg_component;

	return 0;

err_reg_component:
	if (prtd->ptcc_clk->dai_hclk) {
		clk_put(prtd->ptcc_clk->dai_hclk);
		prtd->ptcc_clk->dai_hclk = NULL;
	}
	if (prtd->ptcc_clk->dai_pclk) {
		clk_put(prtd->ptcc_clk->dai_pclk);
		prtd->ptcc_clk->dai_pclk = NULL;
	}
	if (prtd->ptcc_clk->af_pclk) {
		clk_put(prtd->ptcc_clk->af_pclk);
		prtd->ptcc_clk->af_pclk = NULL;
	}

	devm_kfree(&pdev->dev, tcc_intr);
	devm_kfree(&pdev->dev, tcc_clk);
	devm_kfree(&pdev->dev, tcc_reg);
	devm_kfree(&pdev->dev, tcc_i2s);
	devm_kfree(&pdev->dev, prtd);
err:
	return ret;
}

static int  soc_tcc_i2s_remove(struct platform_device *pdev)
{
	struct tcc_runtime_data *prtd = platform_get_drvdata(pdev);
	struct tcc_i2s_data *tcc_i2s = prtd->private_data;
	struct tcc_audio_reg *tcc_reg = prtd->ptcc_reg;
	struct tcc_audio_clk *tcc_clk = prtd->ptcc_clk;
	struct tcc_audio_intr *tcc_intr = prtd->ptcc_intr;

	tcc_pcm_platform_unregister(&pdev->dev);

	snd_soc_unregister_component(&pdev->dev);
	if (prtd->ptcc_clk->dai_hclk) {
		clk_put(prtd->ptcc_clk->dai_hclk);
		prtd->ptcc_clk->dai_hclk = NULL;
	}
	if (prtd->ptcc_clk->dai_pclk) {
		clk_put(prtd->ptcc_clk->dai_pclk);
		prtd->ptcc_clk->dai_pclk = NULL;
	}
	if (prtd->ptcc_clk->af_pclk) {
		clk_put(prtd->ptcc_clk->af_pclk);
		prtd->ptcc_clk->af_pclk = NULL;
	}

	devm_kfree(&pdev->dev, tcc_intr);
	devm_kfree(&pdev->dev, tcc_clk);
	devm_kfree(&pdev->dev, tcc_reg);
	devm_kfree(&pdev->dev, tcc_i2s);
	devm_kfree(&pdev->dev, prtd);

	return 0;
}

static struct of_device_id tcc_i2s_of_match[] = {
	{ .compatible = "telechips,i2s" },
	{ }
};

static SIMPLE_DEV_PM_OPS(tcc_i2s_pm_ops, tcc_i2s_suspend,
		tcc_i2s_resume);

static struct platform_driver tcc_i2s_driver = {
	.driver = {
		.name = "tcc-dai",
		.owner = THIS_MODULE,
		.pm = &tcc_i2s_pm_ops,
		.of_match_table	= of_match_ptr(tcc_i2s_of_match),
	},
	.probe = soc_tcc_i2s_probe,
	.remove = soc_tcc_i2s_remove,
};

static int __init snd_tcc_i2s_init(void)
{
	return platform_driver_register(&tcc_i2s_driver);
}
module_init(snd_tcc_i2s_init);

static void __exit snd_tcc_i2s_exit(void)
{
	return platform_driver_unregister(&tcc_i2s_driver);
}
module_exit(snd_tcc_i2s_exit);

/* Module information */
MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC I2S SoC Interface");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, tcc_i2s_of_match);
