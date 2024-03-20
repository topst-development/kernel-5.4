// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-buf.h>

#include <uapi/misc/tccmisc_drv.h>

int dbg;
#define kdbg(fmt, args...) \
	do { \
		if (dbg) \
			pr_err("\e[33m[%s:%d] \e[0m" fmt, \
				__func__, __LINE__, ## args); \
	} while (0)

struct device_attribute dev_attr_pmap;
struct device_attribute dev_attr_debug;

/* Driver private data */
struct tccmisc_data_t {
	struct miscdevice *misc;
	struct tccmisc_user_t info;
	struct tccmisc_phys_t phys;
};

/* for ioctl testing */
//int tccmisc_drv_test(void)
//{
//	struct file *fp;
//	struct tccmisc_user_t info;
//	int ret;
//
//	fp = filp_open("/dev/tccmisc", O_RDWR, 0666);
//	if (IS_ERR(fp)) {
//		pr_err("%s: tccmisc file open error\n", __func__);
//		ret = -ENODEV;
//		goto exit;
//	}
//
//	strcpy(info.name, "fb_video");
//	info.base = 0;
//	info.size = 0;
//
//	ret = fp->f_op->unlocked_ioctl(fp, IOCTL_TCCMISC_PMAP_KERNEL,
//		(unsigned long)&info);
//	if (ret < 0) {
//		pr_err("%s: tccmisc IOCTL_TCCMISC_PMAP_KERNEL error\n",
//			__func__);
//		goto exit_fp;
//	}
//
//	pr_info("%s: %s,0x%llx,0x%llx\n", __func__,
//		info.name, info.base, info.size);
//
//exit_fp:
//	filp_close(fp, NULL);
//exit:
//	pr_info("%s: ret(%d)\n", __func__, ret);
//	return ret;
//}

int tccmisc_pmap(struct tccmisc_user_t *pmap)
{
	struct device_node *np;
	struct reserved_mem *rmem;
	//const char *name;
	char buf[32];
	int ret = 0;

	sprintf(buf, "pmap,%s", pmap->name);
	kdbg("%s = %s\n", pmap->name, buf);

	np = of_find_compatible_node(NULL, NULL, buf);
	if (np == NULL) {
		kdbg("can't find %s\n", buf);
		ret = -1;
		goto exit;
	}

	rmem = of_reserved_mem_lookup(np);
	if (rmem == NULL) {
		kdbg("pmap,%s allocation is failed\n", pmap->name);
		ret = -2;
		goto exit;
	}

	//if (of_property_read_string(np, "pmap-name", &name)) {
	//	kdbg("can't get pmap-name\n");
	//} else {
	//	strcpy(pmap->name, name);
	//}

	pmap->base = rmem->base;
	pmap->size = rmem->size;

	kdbg("name(%s) base(0x%llx) size(0x%llx)\n",
		pmap->name, pmap->base, pmap->size);
exit:
	kdbg("ret(%d)\n", ret);
	return ret;
}
EXPORT_SYMBOL(tccmisc_pmap);

int tccmisc_phys(struct device *dev, struct tccmisc_phys_t *phys)
{
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	int ret = -1;

	pr_debug("%s: dmabuf_fd %d\n", __func__, phys->dmabuf_fd);
	dmabuf = dma_buf_get(phys->dmabuf_fd);
	if (dmabuf == NULL) {
		pr_err("%s failed to get dma_buf from fd(%d).\n",
			__func__, phys->dmabuf_fd);
		ret = -1;
		goto exit;
	}

	attach = dma_buf_attach(dmabuf, dev);
	if (attach == NULL) {
		pr_err("%s failed to get dma_buf_attach.\n", __func__);
		ret = -1;
		goto exit;
	}

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (sgt == NULL) {
		pr_err("%s failed to get dma_buf_map_attachment.\n",
			__func__);
		ret = -1;
		goto exit;
	}

	phys->addr = sg_dma_address(sgt->sgl);
	phys->len = dmabuf->size;
	ret = 0;

exit:
    return ret;
}
EXPORT_SYMBOL(tccmisc_phys);

static long tccmisc_drv_ioctl(
	struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct tccmisc_data_t *pdata = dev_get_drvdata(misc->parent);
	int ret = 0;

	kdbg("ioctl(%d)\n", cmd);

	switch (cmd) {
	case IOCTL_TCCMISC_PMAP:
	case IOCTL_TCCMISC_PMAP_KERNEL:
		if (cmd == IOCTL_TCCMISC_PMAP_KERNEL) {
			memcpy(&pdata->info,
				(struct tccmisc_user_t *)arg,
				sizeof(struct tccmisc_user_t));
		} else {
			if (copy_from_user(&pdata->info,
				(struct tccmisc_user_t *)arg,
				sizeof(struct tccmisc_user_t))) {
				pr_err("%s: error IOCTL_TCCMISC_PMAP copy_from_user\n",
					__func__);
				ret = -EFAULT;
				goto exit;
			}
		}

		ret = tccmisc_pmap(&pdata->info);

		if (ret == 0) {
			if (cmd == IOCTL_TCCMISC_PMAP_KERNEL) {
				memcpy((struct tccmisc_user_t *)arg,
					&pdata->info,
					sizeof(struct tccmisc_user_t));
			} else {
				if (copy_to_user((struct tccmisc_user_t *)arg,
					&pdata->info,
					sizeof(struct tccmisc_user_t))) {
					pr_err("%s: error IOCTL_TCCMISC_PMAP copy_to_user\n",
						__func__);
					ret = -EFAULT;
					goto exit;
				}
			}

			kdbg("pmap %s (base:0x%llx, size:0x%llx)\n",
				pdata->info.name,
				pdata->info.base,
				pdata->info.size);
		} else {
			pr_err("%s: pmap %s doen't exist.\n",
				__func__,
				pdata->info.name);
		}
		break;
	case IOCTL_TCCMISC_PHYS:
	case IOCTL_TCCMISC_PHYS_KERNEL:
		if (cmd == IOCTL_TCCMISC_PHYS_KERNEL) {
			memcpy(&pdata->phys,
				(struct tccmisc_phys_t *)arg,
				sizeof(struct tccmisc_phys_t));
		} else {
			if (copy_from_user(&pdata->phys,
				(struct tccmisc_phys_t *)arg,
				sizeof(struct tccmisc_phys_t))) {
				pr_err("%s: error IOCTL_TCCMISC_PHYS copy_from_user\n",
					__func__);
				ret = -EFAULT;
				goto exit;
			}
		}

		ret = tccmisc_phys(pdata->misc->this_device, &pdata->phys);

		if (ret == 0) {
			if (cmd == IOCTL_TCCMISC_PHYS_KERNEL) {
				memcpy((struct tccmisc_phys_t *)arg,
					&pdata->phys,
					sizeof(struct tccmisc_phys_t));
			} else {
				if (copy_to_user((struct tccmisc_phys_t *)arg,
					&pdata->phys,
					sizeof(struct tccmisc_phys_t))) {
					pr_err("%s: error IOCTL_TCCMISC_PHYS copy_to_user\n",
						__func__);
					ret = -EFAULT;
					goto exit;
				}
			}
			kdbg("%s: dmabuf_fd:%d dma_addr:0x%llx size:%llx\n",
				__func__,
				pdata->phys.dmabuf_fd,
				(__u64)pdata->phys.addr,
				pdata->phys.len);

		} else {
			pr_err("%s: faild to get physical address from fd(%d)\n",
				__func__, pdata->phys.dmabuf_fd);
		}
		break;
	default:
		pr_err("%s: error ioctl cmd(%d)\n", __func__, cmd);
	}

exit:
	return ret;
}

static ssize_t pmap_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct tccmisc_data_t *pdata;
	int ret;

	pdata = (struct tccmisc_data_t *)dev->platform_data;

	ret = tccmisc_pmap(&pdata->info);
	if (ret < 0) {
		kdbg("pmap %s doen't exist.\n", pdata->info.name);
		return sprintf(buf, "pmap,%s doen't exist.\n",
			pdata->info.name);
	} else {
		return sprintf(buf, "%s,0x%llx,0x%llx\n",
			pdata->info.name,
			pdata->info.base,
			pdata->info.size);
	}
}

static ssize_t pmap_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct tccmisc_data_t *pdata;
	int n;

	pdata = (struct tccmisc_data_t *)dev->platform_data;

	//strcpy(pdata->info.name, buf);
	n = sprintf(pdata->info.name, "%s", buf);
	if (pdata->info.name[n - 1] == '\n')
		pdata->info.name[n - 1] = '\0';

	kdbg("%s, %s\n", buf, pdata->info.name);

	return count;
}
DEVICE_ATTR_RW(pmap);

static ssize_t debug_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct tccmisc_data_t *pdata;

	/*tccmisc_drv_test();*/

	pdata = (struct tccmisc_data_t *)dev->platform_data;
	return sprintf(buf, "%d\n", dbg);
}

static ssize_t debug_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct tccmisc_data_t *pdata;

	pdata = (struct tccmisc_data_t *)dev->platform_data;
	if (kstrtoul(buf, 10, (unsigned long *)&dbg))
		pr_err("%s\n", __func__);
	return count;
}
DEVICE_ATTR_RW(debug);

static int tccmisc_drv_release(struct inode *inode, struct file *filp)
{
	//struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	//struct tccmisc_data_t *pdata = dev_get_drvdata(misc->parent);
	int ret = 0;

	kdbg("\n");
	return ret;
}

static int tccmisc_drv_open(struct inode *inode, struct file *filp)
{
	//struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	//struct tccmisc_data_t *pdata = dev_get_drvdata(misc->parent);
	int ret = 0;

	kdbg("\n");
	return ret;
}

static const struct file_operations tccmisc_drv_fops = {
	.owner = THIS_MODULE,
	.open = tccmisc_drv_open,
	.release = tccmisc_drv_release,
	.unlocked_ioctl = tccmisc_drv_ioctl,
};

static int tccmisc_drv_probe(struct platform_device *pdev)
{
	struct tccmisc_data_t *pdata;
	struct device_node *np;
	int ret = -ENOMEM;

	pdata = kzalloc(sizeof(struct tccmisc_data_t), GFP_KERNEL);
	if (!pdata)
		goto exit;

	pdata->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (!pdata->misc)
		goto err;

	pdata->misc->minor = MISC_DYNAMIC_MINOR;
	pdata->misc->fops = &tccmisc_drv_fops;
	pdata->misc->name = pdev->name;
	pdata->misc->parent = &pdev->dev;
	ret = misc_register(pdata->misc);
	if (ret)
		goto err_misc;

	platform_set_drvdata(pdev, pdata); // @platform_device
	pdev->dev.platform_data = pdata;   // @device

	np = of_find_compatible_node(NULL, NULL, "telechips,tccmisc");
	if (np == NULL) {
		pr_err("%s: tccmisc dt is not exist\n", __func__);
		ret = -ENODEV;
		goto err_dt;
	}

	device_create_file(&pdev->dev, &dev_attr_pmap);
	device_create_file(&pdev->dev, &dev_attr_debug);

	pr_info("%s\n", __func__);
	return 0;

err_dt:
	misc_deregister(pdata->misc);
err_misc:
	kfree(pdata->misc);
err:
	kfree(pdata);
exit:
	pr_err("%s: err(%d)\n", __func__, ret);
	return ret;
}

static int tccmisc_drv_remove(struct platform_device *pdev)
{
	struct tccmisc_data_t *pdata;

	pdata = (struct tccmisc_data_t *)platform_get_drvdata(pdev);

	device_remove_file(&pdev->dev, &dev_attr_pmap);
	device_remove_file(&pdev->dev, &dev_attr_debug);

	misc_deregister(pdata->misc);
	kfree(pdata->misc);
	kfree(pdata);

	return 0;
}

static const struct of_device_id tccmisc_of_match[] = {
	{ .compatible = "telechips,tccmisc" },
	{}
};
MODULE_DEVICE_TABLE(of, tccmisc_of_match);

static struct platform_driver tccmisc_driver = {
	.probe = tccmisc_drv_probe,
	.remove = tccmisc_drv_remove,
	.driver = {
		.name = "tccmisc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tccmisc_of_match),
	},
};

static int __init tccmisc_drv_init(void)
{
	return platform_driver_register(&tccmisc_driver);
}

static void __exit tccmisc_drv_exit(void)
{
	platform_driver_unregister(&tccmisc_driver);
}

module_init(tccmisc_drv_init);
module_exit(tccmisc_drv_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("tccmisc driver");
MODULE_LICENSE("GPL");
