/*
 * Author:  <linux@telechips.com>
 * Created: Nov 30, 2007
 * Description: SoC audio for TCCxx
 *
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>

#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

static int tcc_startup(struct snd_pcm_substream *substream)
{
	/*
	   struct snd_soc_pcm_runtime *rtd = substream->private_data;
	   struct snd_soc_codec *codec = rtd->codec;
	   struct snd_soc_dapm_context *dapm = &codec->dapm;
	   *///ehk23 to avoid warning
	return 0;
}

static void tcc_shutdown(struct snd_pcm_substream *substream)
{
}

static int tcc_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	return 0;
}

static struct snd_soc_ops tcc_ops = {
	.startup = tcc_startup,
	.hw_params = tcc_hw_params,
	.shutdown = tcc_shutdown,
};

static int tcc_snd_card_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

/* tcc digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link tcc_dai[] = {
	{
		.name = "AK4601",
		.stream_name = "AK4601 Stream",
		.ops = &tcc_ops,
		.dai_fmt = (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS),
		//master mode: SND_SOC_DAIFMT_CBS_CFS
		//slave mode: SND_SOC_DAIFMT_CBM_CFM
	},
	{
		.name = "TCC-SPDIF-CH0",
		.stream_name = "IEC958",
		.ops = &tcc_ops,
	},
	{
		.name = "TCC-I2S-CH1",
		.stream_name = "I2S-CH1",
		.ops = &tcc_ops,
		.dai_fmt = (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS),
	},
	{
		.name = "TCC-SPDIF-CH1",
		.stream_name = "IEC958",
		.ops = &tcc_ops,
	},
	//{
	//	.name = "TCC-CDIF-CH1",
	//	.stream_name = "CDIF-CH1",
	//	.ops = &tcc_ops,
	//},
	//{
	//	.name = "TCC-I2S-CH2",
	//	.stream_name = "I2S-CH2",
	//	.ops = &tcc_ops,
	//	.dai_fmt = (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM),
	//},
};

/* tcc audio machine driver */
static struct snd_soc_card tcc_soc_card = {
	.driver_name 	= "TCC Audio",
	.long_name		= "TCC8971 LCN 3.0 Audio",
	.dai_link		= tcc_dai,
	.num_links		= ARRAY_SIZE(tcc_dai),
};

static int tcc_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &tcc_soc_card;
	char dummy_str[]="dummy0", *tmp_str;
	ssize_t alloc_size=0;
	int ret=0, i;

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);

	ret = snd_soc_of_parse_card_name(card, "telechips,model");
	if (ret) {
		return ret;
	}

	alloc_size =
		(ssize_t) sizeof(struct snd_soc_dai_link_component);

	for (i=0 ; i<card->num_links ; i++) {
		tcc_dai[i].cpus =
			kzalloc((size_t) alloc_size, GFP_KERNEL);
		tcc_dai[i].platforms =
			kzalloc((size_t) alloc_size, GFP_KERNEL);
		tcc_dai[i].codecs =
			kzalloc((size_t) alloc_size, GFP_KERNEL);

		if ((tcc_dai[i].cpus == NULL)
				|| (tcc_dai[i].platforms == NULL)
				|| (tcc_dai[i].codecs == NULL)) {
			dev_err(&pdev->dev, "kzalloc failed\n");
			ret = -ENOMEM;
			break;
		}

		tcc_dai[i].cpus->of_node = of_parse_phandle(pdev->dev.of_node,
				"telechips,dai-controller", i);

		if (tcc_dai[i].cpus->of_node != NULL) {
			tcc_dai[i].platforms->of_node =
				of_parse_phandle(tcc_dai[i].cpus->of_node, "adma", 0);
		} else {
			kfree(tcc_dai[i].cpus);
			kfree(tcc_dai[i].platforms);
			kfree(tcc_dai[i].codecs);
			if (i > 0)
				i--;
			break;
		}

		tcc_dai[i].codecs->of_node = of_parse_phandle(pdev->dev.of_node,
				"telechips,audio-codec", i);

		if ((!tcc_dai[i].cpus->of_node)
				|| (!tcc_dai[i].platforms->of_node)) {
			kfree(tcc_dai[i].cpus);
			kfree(tcc_dai[i].platforms);
			kfree(tcc_dai[i].codecs);
			dev_err(&pdev->dev, "platforms of node get fail\n");
			if (i > 0)
				i--;
			break;
		} else {
			if(tcc_dai[i].codecs->of_node == NULL) {
				tmp_str = (char *)kmalloc(strlen("snd-soc-dummy"), GFP_KERNEL);
				sprintf(tmp_str, "snd-soc-dummy");
				tcc_dai[i].codecs->name = tmp_str;
				tmp_str = (char *)kmalloc(strlen("snd-soc-dummy-dai"), GFP_KERNEL);
				sprintf(tmp_str, "snd-soc-dummy-dai");
				tcc_dai[i].codecs->dai_name = tmp_str;
			} else {
#ifdef CONFIG_SND_SOC_AK4601
				char ak4601_str[]="ak4601";
				if (!strcmp(tcc_dai[i].codecs->of_node->name, ak4601_str)) {

					tmp_str = (char *)kmalloc(strlen("ak4601-aif1"), GFP_KERNEL);
					sprintf(tmp_str, "ak4601-aif1");
					tcc_dai[i].codecs->dai_name = tmp_str;

				} else
#endif
					if (!strcmp(tcc_dai[i].codecs->of_node->name, dummy_str)) {
						tcc_dai[i].codecs->of_node = NULL;
						tmp_str = (char *)kmalloc(strlen("snd-soc-dummy"), GFP_KERNEL);
						sprintf(tmp_str, "snd-soc-dummy");
						tcc_dai[i].codecs->name = tmp_str;
						tmp_str = (char *)kmalloc(strlen("snd-soc-dummy-dai"), GFP_KERNEL);
						sprintf(tmp_str, "snd-soc-dummy-dai");
						tcc_dai[i].codecs->dai_name = tmp_str;
					} else {
						dev_err(&pdev->dev, "codecs of node get fail\n");
						kfree(tcc_dai[i].cpus);
						kfree(tcc_dai[i].platforms);
						kfree(tcc_dai[i].codecs);
						if (i > 0)
							i--;
						break;

					}
			}

			tcc_dai[i].num_cpus = 1;
			tcc_dai[i].num_platforms = 1;
			tcc_dai[i].num_codecs = 1;
			tcc_dai[i].init = tcc_snd_card_dai_init;
		}

	}
	card->num_links = i;

	if(ret == 0)
		ret = snd_soc_register_card(card);

	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
				ret);
	}

	return ret;
}

static int tcc_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	snd_soc_unregister_card(card);
	return 0;
}

static const struct of_device_id tcc_audio_of_match[] = {
	{ .compatible = "telechips,snd-lcn3", },
	{},
};

static struct platform_driver tcc_audio_driver = {
	.driver = {
		.name = "tcc-audio-lcn3",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tcc_audio_of_match,
	},
	.probe = tcc_audio_probe,
	.remove = tcc_audio_remove,
};
module_platform_driver(tcc_audio_driver);

MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:tcc-audio-lcn3");
MODULE_DEVICE_TABLE(of, tcc_audio_of_match);

