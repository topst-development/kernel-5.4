// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - present Telechips.co and/or its affiliates.
 */

#ifndef PANEL_TCC_DPV14_H
#define PANEL_TCC_DPV14_H

#if defined(CONFIG_DRM_PANEL_MAX968XX)
int panel_max968xx_reset(void);
int panel_max968xx_get_topology(uint8_t *num_of_ports);
#endif
#endif
