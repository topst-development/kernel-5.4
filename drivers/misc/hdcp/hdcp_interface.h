/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) Telechips Inc.
 *
 */

#ifndef __HDCP_INTERFACE_H
#define __HDCP_INTERFACE_H

enum {
	HDCP_STATUS_IDLE = 0,
	HDCP_STATUS_1_x_AUTENTICATED = 0x10,
	HDCP_STATUS_1_x_AUTHENTICATION_FAILED,
	HDCP_STATUS_1_x_ENCRYPTED,
	HDCP_STATUS_1_x_ENCRYPT_DISABLED,
	HDCP_STATUS_2_2_AUTHENTICATED = 0x20,
	HDCP_STATUS_2_2_AUTHENTICATION_FAILED,
	HDCP_STATUS_2_2_ENCRYPTED,
	HDCP_STATUS_2_2_ENCRYPT_DISABLED,
};

struct hdcp2_driver {
	int (*init)(void);
	int (*deinit)(void);
	int (*enable)(void);
	int (*disable)(void);
	int (*send_srm_list)(uint8_t *data, uint32_t size);
	int (*status)(void);
};

int hdcp2_set_driver(struct hdcp2_driver *driver);
int hdcp2_release_driver(struct hdcp2_driver *driver);

#endif /* __HDCP_INTERFACE_H */
