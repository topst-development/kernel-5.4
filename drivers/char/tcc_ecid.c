// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/arm-smccc.h>
#include <soc/tcc/tcc-sip.h>
#include <soc/tcc/chipinfo.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#define MODE    ((unsigned int)1<<31)
#define CS         ((unsigned int)1<<30)
#define FSET     ((unsigned int)1<<29)
#define MCK      ((unsigned int)1<<28)
#define PRCHG   ((unsigned int)1<<27)
#define PROG     ((unsigned int)1<<26)
#define SCK       ((unsigned int)1<<25)
#define SDI       ((unsigned int)1<<24)
#define E_SIGDV  ((unsigned int)1<<23)
#define A2        ((unsigned int)1<<19)
#define A1        ((unsigned int)1<<18)
#define A0        ((unsigned int)1<<17)
//#define SEC     Hw16
//#define USER    (~Hw16)


#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) ||\
	defined(CONFIG_ARCH_TCC901X) || defined(CONFIG_ARCH_TCC805X)
#define SEL	  14
#else
#define SEL	  15
#endif

/* ECID Code
 * -------- 31 ------------- 15 ----------- 0 --------
 * [0]     |****************|****************|    : '*' is valid
 * [1]     |0000000000000000|****************|    :
 */


struct ecid_platform_data {
	void __iomem *ecid0;
	void __iomem *ecid2;
	void __iomem *ecid3;
	void __iomem *PMU;
	unsigned int gPKG;
	unsigned int gECID[2];
};

static void IO_UTIL_ReadECID(struct ecid_platform_data *pdata, unsigned int iA)
{
	unsigned int ecid_num, ecid_addr;
	unsigned int ecid_data_parallel[4][2];
	unsigned int temp;

	for (ecid_num = 0U; ecid_num < 4U; ecid_num++) {
		// 0: USER0, 1: SEC, 2:USR1, 3:SEC
		writel(MODE | (ecid_num<<SEL), pdata->ecid0);
		writel(MODE | (ecid_num<<SEL) | CS, pdata->ecid0);

		for (ecid_addr = 0U; ecid_addr < 8U; ecid_addr++) {
			writel(MODE | CS | (ecid_addr<<17) |
			       (ecid_num<<SEL), pdata->ecid0);

			writel(MODE | CS | (ecid_addr<<17) |
			       (ecid_num<<SEL) | E_SIGDV, pdata->ecid0);

			writel(MODE | CS | (ecid_addr<<17) |
			       (ecid_num<<SEL) | E_SIGDV | PRCHG, pdata->ecid0);

			writel(MODE | CS | (ecid_addr<<17) |
			       (ecid_num<<SEL) | E_SIGDV | PRCHG | FSET,
			       pdata->ecid0);

			writel(MODE | CS | (ecid_addr<<17) |
			       (ecid_num<<SEL) | PRCHG  | FSET, pdata->ecid0);

			writel(MODE | CS | (ecid_addr<<17) |
			       (ecid_num<<SEL) | FSET, pdata->ecid0);

			writel(MODE | CS | (ecid_addr<<17) |
			       (ecid_num<<SEL), pdata->ecid0);
		}

		// High 16 Bit
		ecid_data_parallel[ecid_num][1] = readl_relaxed(pdata->ecid3);
		// Low  32 Bit
		ecid_data_parallel[ecid_num][0] = readl_relaxed(pdata->ecid2);
		// A2,A1,A0 are LOW
		temp = readl_relaxed(pdata->ecid0);
		temp = (temp & ~(0xE0000U));
		writel(temp, pdata->ecid0);
		temp = readl_relaxed(pdata->ecid0);
		temp = (temp & ~PRCHG);
		writel(temp, pdata->ecid0);

		if (ecid_num == iA) {
			pdata->gECID[0] = ecid_data_parallel[ecid_num][0];
			pdata->gECID[1] = ecid_data_parallel[ecid_num][1];
		}
	}
}

static ssize_t cpu_id_read(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct ecid_platform_data *pdata = dev_get_drvdata(dev);

	IO_UTIL_ReadECID(pdata, 1U);
	// chip id
	return scnprintf(buf, (4U*sizeof(unsigned int)) + 1U,
		"%08X%08X", pdata->gECID[1], pdata->gECID[0]);
}

static ssize_t cpu_id_write(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	return 1;
}

static ssize_t cpu_pkg_read(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct arm_smccc_res res;
	unsigned int temp;

	arm_smccc_smc(SIP_CHIP_NAME, 0, 0, 0, 0, 0, 0, 0, &res);

	if (res.a0 < 0xFFFFFFFFU) {
		temp = (uint32_t)res.a0;
	} else {
		temp = 0xFFFFFFFFU;
	}
	return scnprintf(buf, (4U*sizeof(int32_t)) + 1U, "%08X", temp); // chip id
}

static ssize_t cpu_pkg_write(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	return 1;
}

static ssize_t cpu_rev_read(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	uint32_t revision;

	revision = get_chip_rev();
	return scnprintf(buf, (4U*sizeof(uint32_t)) + 1U,
				"%08X", revision);// chip revision
}

static ssize_t cpu_rev_write(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	return 1;
}



static DEVICE_ATTR(chip_id, 0644, cpu_id_read, cpu_id_write);
static DEVICE_ATTR(chip_pkg, 0644, cpu_pkg_read, cpu_pkg_write);
static DEVICE_ATTR(chip_rev, 0644, cpu_rev_read, cpu_rev_write);

static struct attribute *cpu_rev_sysfs_entries[] = {
	&dev_attr_chip_rev.attr,
	NULL,
};

static struct attribute_group cpu_rev_attr_group = {
	.name	= NULL,
	.attrs	= cpu_rev_sysfs_entries,
};

static struct attribute *cpu_pkg_sysfs_entries[] = {
	&dev_attr_chip_pkg.attr,
	NULL,
};

static struct attribute_group cpu_pkg_attr_group = {
	.name	= NULL,
	.attrs	= cpu_pkg_sysfs_entries,
};

static struct attribute *cpu_id_sysfs_entries[] = {
	&dev_attr_chip_id.attr,
	NULL,
};

static struct attribute_group cpu_id_attr_group = {
	.name	= NULL,
	.attrs	= cpu_id_sysfs_entries,
};

static int cpu_id_probe(struct platform_device *pdev)
{
	struct ecid_platform_data *pdata = NULL;
	struct device_node *np = pdev->dev.of_node;
	int ret;

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct ecid_platform_data),
			     GFP_KERNEL);

	if (np != NULL) {
		pdata->ecid0 = of_iomap(np, 0);
	}

	if (np != NULL) {
		pdata->ecid2 = of_iomap(np, 1);
	}

	if (np != NULL) {
		pdata->ecid3 = of_iomap(np, 2);
	}

	if (np != NULL) {
		pdata->PMU = of_iomap(np, 3);
	}

	pdata->gECID[0] = 0;
	pdata->gECID[1] = 0;

	platform_set_drvdata(pdev, pdata);

	ret = sysfs_create_group(&pdev->dev.kobj, &cpu_id_attr_group);
	ret = sysfs_create_group(&pdev->dev.kobj, &cpu_pkg_attr_group);
	ret = sysfs_create_group(&pdev->dev.kobj, &cpu_rev_attr_group);

	if (ret != 0) {
		(void)pr_err("[CPU_ID] error creating sysfs entries\n");
	}

	(void)pr_info("[CPU_ID] probe is done\n");

	return 0;
}

static int cpu_id_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &cpu_id_attr_group);
	sysfs_remove_group(&pdev->dev.kobj, &cpu_pkg_attr_group);
	sysfs_remove_group(&pdev->dev.kobj, &cpu_rev_attr_group);
	return 0;
}

#ifdef CONFIG_PM
static int cpu_id_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int cpu_id_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define cpu_id_suspend	NULL
#define cpu_id_resume	NULL
#endif

static const struct of_device_id tcc_ecid_id_table[2] = {
	{ .compatible = "telechips,tcc-cpu-id", },
	{}
};
MODULE_DEVICE_TABLE(of, tcc_ecid_id_table);

static struct platform_driver cpu_id_driver = {
	.probe		= cpu_id_probe,
	.remove		= cpu_id_remove,
	.suspend	= cpu_id_suspend,
	.resume		= cpu_id_resume,
	.driver		= {
		.name	= "tcc-cpu-id",
		.of_match_table = of_match_ptr(tcc_ecid_id_table),
	},
};


module_platform_driver(cpu_id_driver);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jay Kim <jay.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips ECID Driver");
