/*
 * linux/sound/soc/tcc/tcc-cdif.c
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
//#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <asm/io.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include <sound/initval.h>
#include <sound/soc.h>

#include "tcc-pcm.h"
#include "tcc-cdif.h"

#define cdif_writel	__raw_writel
#define cdif_readl	__raw_readl

#undef alsa_dbg
#if 0
#define alsa_dai_dbg(devid, f, a...)  printk("== alsa-debug CDIF-%d == " f, devid, ##a)
#define alsa_dbg(f, a...)  printk("== alsa-debug CDIF == " f, ##a)
#else
#define alsa_dai_dbg(devid, f, a...)
#define alsa_dbg(f, a...)
#endif

static int tcc_cdif_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;   // dai base addr
	unsigned int reg_value=0, ret=0;

	alsa_dai_dbg(prtd->id, "%s() \n", __func__);
	if (!dai->active) {
		//Set BCLK 64fs
		//Hw3, Hw2: [0x00]64fs, [0x01]32fs, [0x10]48fs
		reg_value = cdif_readl(pdai_reg + CDIF_CICR);
		reg_value &= ~(Hw3|Hw2);
		cdif_writel(reg_value, pdai_reg + CDIF_CICR);
	}

	return ret;
}

static int tcc_cdif_set_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(cpu_dai->dev);
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;   // dai base addr
	unsigned int reg_value=0;
	int ret=0;
	alsa_dai_dbg(prtd->id, "%s() \n", __func__);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
		case SND_SOC_DAIFMT_CBS_CFS:
			break;
		case SND_SOC_DAIFMT_CBM_CFS:
			break;
		default:
			break;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
		case SND_SOC_DAIFMT_I2S:
			alsa_dbg("[%s] set I2S format. \n", __func__);
			reg_value = cdif_readl(pdai_reg + CDIF_CICR);
			reg_value &= ~Hw1;	//0: I2S Format
			cdif_writel(reg_value, pdai_reg + CDIF_CICR);

			break;
		case SND_SOC_DAIFMT_LEFT_J:
			alsa_dbg("[%s] set LSB Justified format. \n", __func__);
			reg_value = cdif_readl(pdai_reg + CDIF_CICR);
			reg_value |= Hw1;	//1: LSB Justified Format
			cdif_writel(reg_value, pdai_reg + CDIF_CICR);

			break;
		default:
			ret = -EINVAL;
	}

	/* polarity format */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		//Hw0: CDIF Bit Clock Polarity. 0: positive, 1: negative
		case SND_SOC_DAIFMT_NB_NF:
		case SND_SOC_DAIFMT_NB_IF:
		case SND_SOC_DAIFMT_IB_IF:
			reg_value = cdif_readl(pdai_reg + CDIF_CICR);
			reg_value &= ~Hw0;
			cdif_writel(reg_value, pdai_reg + CDIF_CICR);

			break;

		case SND_SOC_DAIFMT_IB_NF:	//Negative BCLK
			reg_value = cdif_readl(pdai_reg + CDIF_CICR);
			reg_value |= Hw0;
			cdif_writel(reg_value, pdai_reg + CDIF_CICR);

			break;
		default:
			ret = -EINVAL;
	}

	return ret;
}

static int tcc_cdif_set_sysclk(struct snd_soc_dai *cpu_dai,
		int clk_id, unsigned int freq, int dir)
{
	/*
	   struct tcc_runtime_data *prtd = dev_get_drvdata(cpu_dai->dev);
	   alsa_dai_dbg(prtd->id, "%s() \n", __func__);
	   if (clk_id != TCC_CDIF_SYSCLK)
	   return -ENODEV;
	   */
	return 0;
}

static int tcc_cdif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);

	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;   // dai base addr
#if !defined(TCC_FIX_FS)
	snd_pcm_format_t format = params_format(params);
#endif
	unsigned int reg_value=0;
	//For Debug
	alsa_dai_dbg(prtd->id, "%s() \n", __func__);


	//Set BCLK 64fs
	//Hw3, Hw2: [0x00]64fs, [0x01]32fs, [0x10]48fs
	reg_value = cdif_readl(pdai_reg + CDIF_CICR);
	reg_value &= ~(Hw3|Hw2);	//64fs
#if defined(TCC_FIX_FS)
	if(TCC_FIX_FS == 32)
		reg_value |= Hw2;
	else if(TCC_FIX_FS == 48)
		reg_value |= Hw3;
#else
	if((format == SNDRV_PCM_FMTBIT_S24_LE)||(format == SNDRV_PCM_FMTBIT_U24_LE))
		reg_value |= Hw2;	//32fs
	else if((format == SNDRV_PCM_FMTBIT_S32_LE)||(format == SNDRV_PCM_FMTBIT_U32_LE))
		reg_value |= Hw3;	//48fs
#endif
	cdif_writel(reg_value, pdai_reg + CDIF_CICR);

	return 0;
}


static int tcc_cdif_trigger(struct snd_pcm_substream *substream, int cmd, struct snd_soc_dai *dai)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;
	unsigned int reg_value=0;
	int ret=0;

	alsa_dai_dbg(prtd->id, "%s() \n", __func__);

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			alsa_dbg("%s() CDIF recording start\n", __func__);
			//SPDIF RX Enable
			reg_value = cdif_readl(pdai_reg - CDIF_OFFSET + SPDIF_RXCONFIG_OFFSET);
			reg_value |= Hw0; // 0: disable, 1: enable
			cdif_writel(reg_value, pdai_reg - CDIF_OFFSET + SPDIF_RXCONFIG_OFFSET);
			//CDIF enable
			reg_value = cdif_readl(pdai_reg + CDIF_CICR);
			reg_value |= Hw7; // 0: disable, 1: enable
			cdif_writel(reg_value, pdai_reg + CDIF_CICR);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			alsa_dbg("%s() CDIF recording end\n", __func__);
			break;
		default:
			ret = -EINVAL;
	}
	return ret;
}

static void tcc_cdif_shutdown(struct snd_pcm_substream *substream, struct snd_soc_dai *dai)
{
	//struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);

	alsa_dai_dbg(prtd->id, "%s() \n", __func__);
}

static int tcc_cdif_suspend(struct device *dev)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dev);
	/*
	   static int tcc_cdif_suspend(struct snd_soc_dai *dai)
	   {
	   struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	   */
	struct tcc_cdif_data *tcc_cdif = prtd->private_data;
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;

	alsa_dai_dbg(prtd->id, "%s() \n", __func__);

	// Backup's all about dai control.
	tcc_cdif->backup_rCICR = cdif_readl(pdai_reg+CDIF_CICR);
	// Disable all about dai clk
	if(__clk_is_enabled(prtd->ptcc_clk->dai_hclk))
		clk_disable_unprepare(prtd->ptcc_clk->dai_hclk);

	return 0;
}

static int tcc_cdif_resume(struct device *dev)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dev);
	/*
	   static int tcc_cdif_resume(struct snd_soc_dai *dai)
	   {
	   struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);
	   */
	struct tcc_cdif_data *tcc_cdif = prtd->private_data;
	void __iomem *pdai_reg = prtd->ptcc_reg->dai_reg;

	alsa_dai_dbg(prtd->id, "%s() \n", __func__);


	// Enable all about dai clk
	if(prtd->ptcc_clk->dai_hclk)
		clk_disable_unprepare(prtd->ptcc_clk->dai_hclk);

	// Set dai reg from backup's.
	cdif_writel(tcc_cdif->backup_rCICR, pdai_reg+CDIF_CICR);

	return 0;
}

static int tcc_cdif_probe(struct snd_soc_dai *dai)
{
	struct tcc_runtime_data *prtd = dev_get_drvdata(dai->dev);

	alsa_dai_dbg(prtd->id, "%s() \n", __func__);

	/* clock enable */
	if(prtd->ptcc_clk->dai_hclk)
		clk_prepare_enable(prtd->ptcc_clk->dai_hclk);

	return 0;
}

struct snd_soc_dai_ops tcc_cdif_ops = {
	.set_sysclk = tcc_cdif_set_sysclk,
	.set_fmt    = tcc_cdif_set_fmt,
	.startup    = tcc_cdif_startup,
	.shutdown   = tcc_cdif_shutdown,
	.hw_params  = tcc_cdif_hw_params,
	.trigger    = tcc_cdif_trigger,
};

struct snd_soc_dai_driver tcc_snd_cdif = {
	.name = "tcc-dai-cdif",
	.probe = tcc_cdif_probe,
	/*
	   .suspend = tcc_cdif_suspend,
	   .resume  = tcc_cdif_resume,
	   */
	.capture = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = TCC_CDIF_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE},
	.ops   = &tcc_cdif_ops,
	//.symmetric_rates = 1,
	//.symmetric_channels = 1,
	//.symmetric_samplebits = 1,
};

static const struct snd_soc_component_driver tcc_cdif_component = {
	.name		= "tcc-cdif",
};
static int soc_tcc_cdif_probe(struct platform_device *pdev)
{
	struct device_node *of_node_adma;
	struct platform_device *pdev_adma;
	struct tcc_adma_drv_data *pdev_adma_data;
	struct tcc_runtime_data *prtd;
	struct tcc_cdif_data *tcc_cdif;
	struct tcc_audio_reg *tcc_reg;
	struct tcc_audio_clk *tcc_clk;
	struct tcc_audio_intr *tcc_intr;
	u32 clk_rate;
	int ret = 0;


	/* Allocation for keeping Device Tree Info. START */
	prtd = devm_kzalloc(&pdev->dev, sizeof(struct tcc_runtime_data), GFP_KERNEL);
	if (!prtd) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err;
	}
	dev_set_drvdata(&pdev->dev, prtd);

	memset(prtd, 0, sizeof(struct tcc_runtime_data));

	tcc_cdif = devm_kzalloc(&pdev->dev, sizeof(struct tcc_cdif_data), GFP_KERNEL);
	if (!tcc_cdif) {
		dev_err(&pdev->dev, "no memory for tcc_i2s_data\n");
		ret = -ENOMEM;
		goto err;
	}
	prtd->private_data = tcc_cdif;
	memset(tcc_cdif, 0, sizeof(struct tcc_cdif_data));

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

	/* Flag set CDIF for getting I/F name in PCM driver */
	prtd->id = -1;
	prtd->id = of_alias_get_id(pdev->dev.of_node, "cdif");
	if(prtd->id < 0) {
		dev_err(&pdev->dev, "cdif id[%d] is wrong.\n", prtd->id);
		ret = -EINVAL;
		goto err;
	}
	prtd->flag = TCC_CDIF_FLAG;

	/* get dai info. */
	prtd->ptcc_reg->dai_reg = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR((void *)prtd->ptcc_reg->dai_reg))
		prtd->ptcc_reg->dai_reg = NULL;

	/* get dai clk info. */
	prtd->ptcc_clk->dai_hclk = of_clk_get(pdev->dev.of_node, 1);
	if (IS_ERR(prtd->ptcc_clk->dai_hclk))
		prtd->ptcc_clk->dai_hclk = NULL;
	//CDIF doesn't need to set peri & audio filter clock.
	prtd->ptcc_clk->dai_pclk = NULL;
	prtd->ptcc_clk->af_pclk = NULL;

	of_property_read_u32(pdev->dev.of_node, "clock-frequency", &clk_rate);
	if(clk_rate > 192000)clk_rate=48000;
	tcc_cdif->bclk_fs = (unsigned long)clk_rate;

	/* get adma info */
	of_node_adma = of_parse_phandle(pdev->dev.of_node, "adma", 0);
	if (of_node_adma) {

		pdev_adma = of_find_device_by_node(of_node_adma);
		prtd->ptcc_reg->adma_reg = of_iomap(of_node_adma, 0);
		if (IS_ERR((void *)prtd->ptcc_reg->adma_reg))
			prtd->ptcc_reg->adma_reg = NULL;
		else
			alsa_dai_dbg(prtd->id, "adma_reg=%p\n", prtd->ptcc_reg->adma_reg);

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
		return -1;
	}

	ret = snd_soc_register_component(&pdev->dev, &tcc_cdif_component,
			&tcc_snd_cdif, 1);
	if (ret)
		goto err_reg_component;

	alsa_dai_dbg(prtd->id, "%s()\n", __func__);


	return 0;

err_reg_component:
	if (prtd->ptcc_clk->dai_hclk) {
		clk_put(prtd->ptcc_clk->dai_hclk);
		prtd->ptcc_clk->dai_hclk = NULL;
	}

	devm_kfree(&pdev->dev, tcc_intr);
	devm_kfree(&pdev->dev, tcc_clk);
	devm_kfree(&pdev->dev, tcc_reg);
	devm_kfree(&pdev->dev, tcc_cdif);
	devm_kfree(&pdev->dev, prtd);

err:
	return ret;
}

static int  soc_tcc_cdif_remove(struct platform_device *pdev)
{
	struct tcc_runtime_data *prtd = platform_get_drvdata(pdev);
	struct tcc_cdif_data *tcc_cdif = prtd->private_data;
	struct tcc_audio_reg *tcc_reg = prtd->ptcc_reg;
	struct tcc_audio_clk *tcc_clk = prtd->ptcc_clk;
	struct tcc_audio_intr *tcc_intr = prtd->ptcc_intr;

#if 0
	if (tcc_spdif->id == 0)
		tcc_iec958_pcm_platform_unregister(&pdev->dev);
	else
		tcc_iec958_pcm_sub3_platform_unregister(&pdev->dev);
#else
	tcc_pcm_platform_unregister(&pdev->dev);
#endif
	snd_soc_unregister_component(&pdev->dev);
	if (prtd->ptcc_clk->dai_hclk) {
		clk_put(prtd->ptcc_clk->dai_hclk);
		prtd->ptcc_clk->dai_hclk = NULL;
	}

	devm_kfree(&pdev->dev, tcc_intr);
	devm_kfree(&pdev->dev, tcc_clk);
	devm_kfree(&pdev->dev, tcc_reg);
	devm_kfree(&pdev->dev, tcc_cdif);
	devm_kfree(&pdev->dev, prtd);
	return 0;
}

static struct of_device_id tcc_cdif_of_match[] = {
	{ .compatible = "telechips,cdif" },
	{ }
};

static SIMPLE_DEV_PM_OPS(tcc_cdif_pm_ops, tcc_cdif_suspend,
		tcc_cdif_resume);

static struct platform_driver tcc_cdif_driver = {
	.driver = {
		.name = "tcc-cdif",
		.owner = THIS_MODULE,
		.pm = &tcc_cdif_pm_ops,
		.of_match_table	= of_match_ptr(tcc_cdif_of_match),
	},
	.probe = soc_tcc_cdif_probe,
	.remove = soc_tcc_cdif_remove,
};

static int __init snd_tcc_cdif_init(void)
{
	return platform_driver_register(&tcc_cdif_driver);
}
module_init(snd_tcc_cdif_init);

static void __exit snd_tcc_cdif_exit(void)
{
	return platform_driver_unregister(&tcc_cdif_driver);
}
module_exit(snd_tcc_cdif_exit);

/* Module information */
MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC CDIF SoC Interface");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(of, tcc_cdif_of_match);
