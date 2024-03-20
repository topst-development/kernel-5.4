// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef AMFMTUNER_H_
#define AMFMTUNER_H_

#include "dxb_ctrl_defs.h"
extern long_t amfmtuner_ioctl(const struct tcc_dxb_ctrl_t *ctrl,
				uint32_t cmd, ulong arg);

#endif // AMFMTUNER_H_
