// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include "dxb_ctrl_gpio.h"

void dxb_ctrl_gpio_out_init(int32_t pin)
{
	int32_t rc;

	if (!gpio_is_valid(pin)) {
		if (pin == (-ENOENT)) {
			/* skip, no msg */
		} else {
			(void)
			    pr_err("[ERROR][TCC_DXB_CTRL] %s pin[%d] invalid\n",
				   __func__, pin);
		}
	} else {
		rc = gpio_request((uint32_t) pin, NULL);
		if (rc != 0) {
			(void)
			    pr_err
			    ("[ERROR][TCC_DXB_CTRL] %s pin[%d] error(%d)\n",
			     __func__, pin, rc);
		} else {
			(void)pr_info("[INFO][TCC_DXB_CTRL] %s pin[%d]\n",
				      __func__, pin);
			(void)gpio_direction_output((uint32_t) pin, 0);
		}
	}
}

void dxb_ctrl_gpio_in_init(int32_t pin)
{
	int32_t rc;

	if (!gpio_is_valid(pin)) {
		if (pin == (-ENOENT)) {
			/* skip, no msg */
		} else {
			(void)
			    pr_err("[ERROR][TCC_DXB_CTRL] %s pin[%d] invalid\n",
				   __func__, pin);
		}
	} else {
		rc = gpio_request((uint32_t) pin, NULL);
		if (rc != 0) {
			(void)
			    pr_err
			    ("[ERROR][TCC_DXB_CTRL] %s pin[%d] error(%d)\n",
			     __func__, pin, rc);
		} else {
			(void)pr_info("[INFO][TCC_DXB_CTRL] %s pin[%d]\n",
				      __func__, pin);
			(void)gpio_direction_input((uint32_t) pin);
		}
	}
}

void dxb_ctrl_gpio_set_value(int32_t pin, int32_t value)
{
	if (!gpio_is_valid(pin)) {
		if (pin == (-ENOENT)) {
			/* skip, no msg */
		} else {
			(void)
			    pr_err("[ERROR][TCC_DXB_CTRL] %s pin[%d] invalid\n",
				   __func__, pin);
		}
	} else {
		(void)pr_info("[INFO][TCC_DXB_CTRL] %s pin[%d] value[%d]\n",
			      __func__, pin, value);

		if (gpio_cansleep((uint32_t) pin) != 0) {
			gpio_set_value_cansleep((uint32_t) pin, value);
		} else {
			gpio_set_value((uint32_t) pin, value);
		}
	}
}

int32_t dxb_ctrl_gpio_get_value(int32_t pin)
{
	int32_t ret;

	if (!gpio_is_valid(pin)) {
		(void)pr_err("[ERROR][TCC_DXB_CTRL] %s pin[%d] invalid\n",
			     __func__, pin);
		ret = (-1);
	} else {
		(void)pr_info("[INFO][TCC_DXB_CTRL] %s pin[%d]\n",
			      __func__, pin);

		if (gpio_cansleep((uint32_t) pin) != 0) {
			ret = gpio_get_value_cansleep((uint32_t) pin);
		} else {
			ret = gpio_get_value((uint32_t) pin);
		}
	}
	return ret;
}

void dxb_ctrl_gpio_free(int32_t pin)
{
	if (gpio_is_valid(pin)) {
		(void)pr_info("[INFO][TCC_DXB_CTRL] %s pin[%d]\n",
			      __func__, pin);
		gpio_free((uint32_t) pin);
	}
}
