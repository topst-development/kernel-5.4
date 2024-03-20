// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#define pr_fmt(fmt) "tcc-pmi: " fmt

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched/signal.h>
#include <linux/spinlock.h>

#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox/tcc_multi_mbox.h>

#define MSGLEN ((size_t)MBOX_CMD_FIFO_SIZE << 2U)

struct power_msg {
	u8 cmd[MSGLEN];
	struct list_head list;
};

struct power_mbox {
	struct list_head msgs;
	spinlock_t lock;
	struct wait_queue_head wq;
};

struct pmi_data {
	struct platform_device *pdev;
	struct device *dev;
	struct cdev chrdev;
	struct mbox_chan *chan;
	struct power_mbox mbox;
};

static void power_mbox_print_msg(const char *prefix, const u8 *cmd)
{
	char buf[MSGLEN * 3U];
	const char *shorten;
	size_t cmdlen;
	const size_t bufsz = sizeof(buf);

	for (cmdlen = MSGLEN; cmdlen > 0U; cmdlen--) {
		if (cmd[cmdlen - 1U] != 0U) {
			break;
		}
	}

	shorten = (cmdlen < (MSGLEN - 3U)) ? " 00 .. 00" : "";

	(void)hex_dump_to_buffer(cmd, cmdlen, 32, 1, buf, bufsz, (bool)false);
	(void)pr_info("%s: %s%s\n", prefix, buf, shorten);
}

static struct power_msg *power_mbox_get_msg(struct power_mbox *mbox)
{
	struct power_msg *msg;
	ulong flags;

	spin_lock_irqsave(&mbox->lock, flags);

	msg = list_first_entry_or_null(&mbox->msgs, struct power_msg, list);
	if (msg != NULL) {
		/* Delete the message from list */
		list_del(&msg->list);
	}

	spin_unlock_irqrestore(&mbox->lock, flags);

	return msg;
}

static struct power_msg *power_mbox_poll_msg(struct power_mbox *mbox)
{
	struct power_msg *msg;
	s32 sigpend;
	bool retry = (bool)true;

	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&mbox->wq, &wait);

	/* Poll message until either catching a message or signal. */
	while (retry) {
		__set_current_state(TASK_INTERRUPTIBLE);

		msg = power_mbox_get_msg(mbox);
		sigpend = signal_pending(current);
		retry = (bool)false;

		if ((msg == NULL) && (sigpend == 0)) {
			schedule();
			retry = (bool)true;
		}
	}

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&mbox->wq, &wait);

	return msg;
}

static void power_mbox_put_msg(struct power_mbox *mbox, struct power_msg *msg)
{
	ulong flags;
	s32 empty;

	spin_lock_irqsave(&mbox->lock, flags);

	empty = list_empty(&mbox->msgs);
	list_add_tail(&msg->list, &mbox->msgs);

	if (empty != 0) {
		/* Wake the one waiting for any messages coming */
		wake_up_interruptible(&mbox->wq);
	}

	spin_unlock_irqrestore(&mbox->lock, flags);
}

static inline void power_mbox_rm_msg(struct power_msg *msg, struct device *dev)
{
	list_del(&msg->list);
	devm_kfree(dev, msg);
}

static void power_mbox_purge_msg(struct power_mbox *mbox, struct device *dev)
{
	struct power_msg *msg;
	struct power_msg *next;
	ulong flags;

	spin_lock_irqsave(&mbox->lock, flags);

	list_for_each_entry_safe(msg, next, &mbox->msgs, list) {
		power_mbox_rm_msg(msg, dev);
	}

	spin_unlock_irqrestore(&mbox->lock, flags);
}

static void power_mbox_rx_callback(struct mbox_client *cl, void *m)
{
	struct pmi_data *data = dev_get_drvdata(cl->dev);
	struct power_msg *msg;

	msg = devm_kmalloc(data->dev, MSGLEN, GFP_KERNEL);
	if (msg != NULL) {
		(void)memcpy(msg->cmd, m, MSGLEN);
		power_mbox_put_msg(&data->mbox, msg);
		power_mbox_print_msg("receive", msg->cmd);
	}
}

static ssize_t power_mbox_read(struct file *filp, char __user *buf,
			       size_t len, loff_t *ppos)
{
	struct pmi_data *data = filp->private_data;
	const struct power_msg *msg;
	ssize_t cnt;
	const char *fail = NULL;

	if (len < MSGLEN) {
		fail = "Buffer too short";
		msg = NULL;
		cnt = -EINVAL;
	} else if ((filp->f_flags & (u32)O_NONBLOCK) != 0U) {
		msg = power_mbox_get_msg(&data->mbox);
		cnt = -EAGAIN;
	} else {
		msg = power_mbox_poll_msg(&data->mbox);
		cnt = -ERESTARTSYS;
	}

	if (msg != NULL) {
		cnt = simple_read_from_buffer(buf, MSGLEN, ppos, msg->cmd, len);
		if (cnt <= 0) {
			fail = "Failed to copy message";
		} else {
			power_mbox_print_msg("read", msg->cmd);
		}

		*ppos = 0;
		devm_kfree(data->dev, msg);
	}

	if (fail != NULL) {
		(void)pr_err("read: %s (err: %zd)\n", fail, cnt);
	}

	return cnt;
}

static ssize_t power_mbox_write(struct file *filp, const char __user *buf,
				size_t len, loff_t *ppos)
{
	const struct pmi_data *data = filp->private_data;
	struct tcc_mbox_data mdata;
	ssize_t cnt;
	const char *fail = NULL;

	if (len > MSGLEN) {
		fail = "Buffer too long";
		cnt = -EINVAL;
	} else {
		(void)memset(mdata.cmd, 0, sizeof(mdata.cmd));
		mdata.data_len = 0;

		cnt = simple_write_to_buffer(mdata.cmd, MSGLEN, ppos, buf, len);
		if (cnt <= 0) {
			fail = "Failed to copy message";
		} else {
			s32 ret = mbox_send_message(data->chan, &mdata);
			if (ret < 0) {
				fail = "Failed to send message";
				cnt = (ssize_t)ret;
			} else {
				power_mbox_print_msg("write", (u8 *)mdata.cmd);
			}
		}

		*ppos = 0;
	}

	if (fail != NULL) {
		(void)pr_err("write: %s (err: %zd)\n", fail, cnt);
	}

	return cnt;
}

static int power_mbox_open(struct inode *in, struct file *filp)
{
	filp->private_data = NULL;

	if (in->i_cdev != NULL) {
		filp->private_data =
			container_of(in->i_cdev, struct pmi_data, chrdev);
	}

	return (filp->private_data == NULL) ? -ENODEV : 0;
}

static const struct file_operations power_mbox_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = power_mbox_read,
	.write = power_mbox_write,
	.open = power_mbox_open,
};

static int power_mbox_chan_init(struct mbox_chan **chan_p, struct device *dev)
{
	struct mbox_client *cl;
	struct mbox_chan *chan;
	s32 ret = -ENOMEM;

	cl = devm_kzalloc(dev, sizeof(struct mbox_client), GFP_KERNEL);
	if (cl != NULL) {
		cl->dev = dev;
		cl->tx_block = (bool)true;
		cl->tx_tout = CLIENT_MBOX_TX_TIMEOUT;
		cl->knows_txdone = (bool)false;
		cl->rx_callback = &power_mbox_rx_callback;

		chan = mbox_request_channel(cl, 0);
		ret = PTR_ERR_OR_ZERO(chan);
		if (ret < 0) {
			devm_kfree(dev, cl);
			chan = NULL;
		}

		*chan_p = chan;
	}

	return ret;
}

static void power_mbox_chan_free(struct mbox_chan *chan, struct device *dev)
{
	if ((dev != NULL) && (chan != NULL)) {
		const struct mbox_client *cl = chan->cl;

		mbox_free_channel(chan);
		devm_kfree(dev, cl);
	}
}

static int power_mbox_cdev_init(struct cdev *chrdev)
{
	dev_t devt;
	s32 ret;

	ret = alloc_chrdev_region(&devt, 0, 1, "tcc-pmi");
	if (ret == 0) {
		cdev_init(chrdev, &power_mbox_fops);
		chrdev->owner = THIS_MODULE;

		ret = cdev_add(chrdev, devt, 1);
		if (ret != 0) {
			unregister_chrdev_region(devt, 1);
		}
	}

	return ret;
}

static void power_mbox_cdev_free(struct cdev *chrdev)
{
	cdev_del(chrdev);
	unregister_chrdev_region(chrdev->dev, 1);
}

static int power_mbox_dev_init(struct device **dp, const dev_t devt)
{
	struct class *cls;
	struct device *dev;
	s32 ret;

	cls = class_create(THIS_MODULE, "mbox");
	ret = PTR_ERR_OR_ZERO(cls);
	if (ret == 0) {
		dev = device_create(cls, NULL, devt, NULL, "mbox_power");
		ret = PTR_ERR_OR_ZERO(dev);
		if (ret < 0) {
			class_destroy(cls);
		} else {
			*dp = dev;
		}
	}

	return ret;
}

static void power_mbox_dev_free(const struct device *dev, const dev_t devt)
{
	struct class *cls = dev->class;

	device_destroy(cls, devt);
	class_destroy(cls);
}

static void power_mbox_struct_init(struct power_mbox *mbox)
{
	INIT_LIST_HEAD(&mbox->msgs);
	init_waitqueue_head(&mbox->wq);
	spin_lock_init(&mbox->lock);
}

static int tcc_pmi_data_init(struct device *dev, struct pmi_data *data)
{
	s32 ret;
	const char *fail = NULL;

	/* Initialize power mailbox struct */
	power_mbox_struct_init(&data->mbox);

	/* Initialize power mailbox channel */
	ret = power_mbox_chan_init(&data->chan, dev);
	if (ret != 0) {
		fail = "Failed to init power mbox channel";
	} else {
		/* Initialize character device for power mailbox */
		ret = power_mbox_cdev_init(&data->chrdev);
		if (ret != 0) {
			fail = "Failed to init character device";
			power_mbox_chan_free(data->chan, dev);
		} else {
			/* Initialize device for power mailbox */
			ret = power_mbox_dev_init(&data->dev, data->chrdev.dev);
			if (ret != 0) {
				fail = "Failed to init device";
				power_mbox_chan_free(data->chan, dev);
				power_mbox_cdev_free(&data->chrdev);
			}
		}
	}

	if (fail != NULL) {
		(void)pr_err("%s (err: %d)\n", fail, ret);
	}

	return ret;
}

static int tcc_pmi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pmi_data *data;
	s32 ret;

	/* Allocate memory for driver data */
	data = devm_kzalloc(dev, sizeof(struct pmi_data), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		(void)pr_err("Failed to allocate driver data (err: %d)\n", ret);
	} else {
		/* Initialize PMI driver data */
		ret = tcc_pmi_data_init(dev, data);
		if (ret != 0) {
			devm_kfree(dev, data);
		} else {
			platform_set_drvdata(pdev, data);
			data->pdev = pdev;
		}
	}

	return ret;
}

static int tcc_pmi_remove(struct platform_device *pdev)
{
	struct pmi_data *data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	power_mbox_dev_free(data->dev, data->chrdev.dev);
	power_mbox_cdev_free(&data->chrdev);
	power_mbox_chan_free(data->chan, dev);
	devm_kfree(dev, data);

	return 0;
}

static int tcc_pmi_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct pmi_data *data = platform_get_drvdata(pdev);

	power_mbox_chan_free(data->chan, &pdev->dev);
	power_mbox_purge_msg(&data->mbox, data->dev);

	return 0;
}

static int tcc_pmi_resume(struct platform_device *pdev)
{
	struct pmi_data *data = platform_get_drvdata(pdev);
	s32 ret = power_mbox_chan_init(&data->chan, &pdev->dev);

	return ret;
}

static const struct of_device_id tcc_pmi_match[2] = {
	{ .compatible = "telechips,pmi" },
	{ .compatible = "" }
};

MODULE_DEVICE_TABLE(of, tcc_pmi_match);

static struct platform_driver tcc_pmi_driver = {
	.probe = tcc_pmi_probe,
	.remove = tcc_pmi_remove,
	.suspend = tcc_pmi_suspend,
	.resume = tcc_pmi_resume,
	.driver = {
		.name = "tcc-pmi",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tcc_pmi_match),
	},
};

module_platform_driver(tcc_pmi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips power management interface driver");
