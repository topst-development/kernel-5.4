// SPDX-License-Identifier: GPL-2.0-or-later OR MIT
/*
* Copyright (C) Telechips Inc.
*/

#include <linux/device.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/stat.h>

#include "Dptx_v14.h"
#include "Dptx_reg.h"
#include "Dptx_dbg.h"
#include "Dptx_drm_dp_addition.h"


#define DPTX_DEBUGFS_BUF_SIZE		1024
#define DATA_DUMP_BUF_SIZE			DPTX_EDID_BUFLEN

int dptx_proc_open(struct inode *pinode, struct file *filp);
int dptx_proc_close(struct inode *pinode, struct file *filp);

ssize_t dptx_ext_proc_read_hpd_state(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set);
ssize_t dptx_ext_proc_read_port_composition(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set);
ssize_t dptx_ext_proc_read_edid_data(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set);
ssize_t dptx_ext_proc_read_link_training_status(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set);
ssize_t dptx_ext_proc_read_str_status(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set);
ssize_t dptx_ext_proc_read_video_timing(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set);
ssize_t dptx_ext_proc_write_video_timing(struct file *filp, const char __user *buffer, size_t cnt, loff_t *off_set);
ssize_t dptx_ext_proc_read_audio_configuration(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set);
ssize_t dptx_ext_proc_write_audio_configuration(struct file *filp, const char __user *buffer, size_t cnt, loff_t *off_set);


static const struct file_operations proc_fops_hpd_state = {
	.owner   = THIS_MODULE,
	.open    = dptx_proc_open,
	.release = dptx_proc_close,
	.read    = dptx_ext_proc_read_hpd_state,
};

static const struct file_operations proc_fops_topology_state = {
	.owner   = THIS_MODULE,
	.open    = dptx_proc_open,
	.release = dptx_proc_close,
	.read    = dptx_ext_proc_read_port_composition,
};

static const struct file_operations proc_fops_edid_data = {
	.owner   = THIS_MODULE,
	.open    = dptx_proc_open,
	.release = dptx_proc_close,
	.read    = dptx_ext_proc_read_edid_data,
};

static const struct file_operations proc_fops_linkT_data = {
	.owner   = THIS_MODULE,
	.open    = dptx_proc_open,
	.release = dptx_proc_close,
	.read    = dptx_ext_proc_read_link_training_status,
};

static const struct file_operations proc_fops_str_data = {
	.owner   = THIS_MODULE,
	.open    = dptx_proc_open,
	.release = dptx_proc_close,
	.read    = dptx_ext_proc_read_str_status,
};

static const struct file_operations proc_fops_video_data = {
	.owner   = THIS_MODULE,
	.open    = dptx_proc_open,
	.release = dptx_proc_close,
	.read    = dptx_ext_proc_read_video_timing,
	.write	 = dptx_ext_proc_write_video_timing,
};


static const struct file_operations proc_fops_audio_data = {
	.owner   = THIS_MODULE,
	.open    = dptx_proc_open,
	.release = dptx_proc_close,
	.read    = dptx_ext_proc_read_audio_configuration,
	.write	 = dptx_ext_proc_write_audio_configuration,
};


static void dptx_ext_Print_U8_Buf(const u8 *pucBuf, u32 uiLength)
{
	uint32_t uiOffset;

	if (uiLength < (uint32_t)DATA_DUMP_BUF_SIZE) {
		for (uiOffset = 0; uiOffset < uiLength; uiOffset++) {
			if ((uiOffset % 16U) == 0U) {
				/*For Coverity*/
				dptx_dump("\n0x%02x:", uiOffset);
			} else {
				/*For Coverity*/
				dptx_dump(" 0x%02x", pucBuf[uiOffset]);
			}
		}
	} else {
		/*For Coverity*/
		dptx_err("Invalid dump length as %d", uiLength);
	}
}

int dptx_proc_open(struct inode *pinode, struct file *filp)
{
	int32_t iRet = DPTX_RETURN_NO_ERROR;

	pinode++;
	filp++;

	if( try_module_get(THIS_MODULE) == (bool)false) {
		/*For Coverity*/
		iRet = -ENODEV;
	}

	return iRet;
}

int dptx_proc_close(struct inode *pinode, struct file *filp)
{
	pinode++;
	filp++;

	module_put(THIS_MODULE);

	return 0;
}

ssize_t dptx_ext_proc_read_hpd_state(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
	char *pcHpd_Buf = NULL;
	uint8_t ucHPD_State;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	int32_t iFmt_Size;
	ssize_t stSize = 0;
	struct Dptx_Params *pstDptx;

	pstDptx = (struct Dptx_Params *)PDE_DATA(file_inode(filp));

	pcHpd_Buf = (char *)devm_kzalloc(pstDptx->pstParentDev, DPTX_DEBUGFS_BUF_SIZE, GFP_KERNEL);
	if (pcHpd_Buf == NULL) {
		dptx_err("Could not allocate HPD state buffer");
		iRetVal = DPTX_RETURN_ENOMEM;
	} else {
		iRetVal = Dptx_Intr_Get_HotPlug_Status(pstDptx, &ucHPD_State);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			dptx_err("Error from Dptx_Intr_Get_HotPlug_Status()");
			ucHPD_State = (uint8_t)HPD_STATUS_UNPLUGGED;
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		iFmt_Size = scnprintf(pcHpd_Buf, (size_t)DPTX_DEBUGFS_BUF_SIZE, "%s\n",
								(ucHPD_State == (uint8_t)HPD_STATUS_PLUGGED) ? "Hot plugged" : "Hot unplugged");

		if (iFmt_Size > 0) {
			/*For Coverity*/
			stSize = simple_read_from_buffer(usr_buf, cnt, off_set, (void *)pcHpd_Buf, (size_t)iFmt_Size);
		}
	}

	if (pcHpd_Buf != NULL) {
		/*For Coverity*/
		devm_kfree(pstDptx->pstParentDev, pcHpd_Buf);
	}

	return stSize;
}

ssize_t dptx_ext_proc_read_port_composition(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
	char *pcTopology_Buf = NULL;
	uint8_t ucHPD_State;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	int32_t iFmt_Size;
	ssize_t stSize = 0;
	struct Dptx_Params *pstDptx;

	pstDptx = (struct Dptx_Params *)PDE_DATA(file_inode(filp));

	pcTopology_Buf = (char *)devm_kzalloc(pstDptx->pstParentDev, DPTX_DEBUGFS_BUF_SIZE, GFP_KERNEL);
	if (pcTopology_Buf == NULL) {
		dptx_err("Could not allocate HPD state buffer ");
		iRetVal = DPTX_RETURN_ENOMEM;
	} else {
		iRetVal = Dptx_Intr_Get_HotPlug_Status(pstDptx, &ucHPD_State);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			dptx_err("Error from Dptx_Intr_Get_HotPlug_Status -> Hot unplugged");
			ucHPD_State = (uint8_t)HPD_STATUS_UNPLUGGED;
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		if (ucHPD_State == (uint8_t)HPD_STATUS_PLUGGED) {
			iRetVal = Dptx_Intr_Get_Port_Composition(pstDptx, 0);
			if (iRetVal != DPTX_RETURN_NO_ERROR) {
				/*For Coverity*/
				dptx_err("Error from Dptx_Intr_Get_Port_Composition()");
			}
		} else {
			dptx_err("Hot unplugged..");
			iRetVal = DPTX_RETURN_ENODEV;
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		iFmt_Size = scnprintf(pcTopology_Buf, (size_t)DPTX_DEBUGFS_BUF_SIZE, "%s : %d %s %s connected\n",
								(pstDptx->bMultStreamTransport == (bool)true) ? "MST mode":"SST mode",
								pstDptx->ucNumOfPorts,
								(pstDptx->bSideBand_MSG_Supported == (bool)true) ? "Ext. monitor":"SerDes",
								(pstDptx->ucNumOfPorts == 1U) ? "port is":"ports are");

		if (iFmt_Size > 0) {
			/*For Coverity*/
			stSize = simple_read_from_buffer(usr_buf, cnt, off_set, (void *)pcTopology_Buf, (size_t)iFmt_Size);
		}
	}

	if (pcTopology_Buf != NULL) {
		/*For Coverity*/
		devm_kfree(pstDptx->pstParentDev, pcTopology_Buf);
	}

	return stSize;
}

ssize_t dptx_ext_proc_read_edid_data(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
	bool bSink_MST_Supported = (bool)false;
	bool bSink_Has_EDID = (bool)false;
	char *pcEdid_Buf = NULL;
	uint8_t ucHPD_State, ucNumOfPluggedPorts = 0, ucStream_Index;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	int32_t iFmt_Size;
	ssize_t stSize = 0;
	struct Dptx_Params *pstDptx;

	pstDptx = (struct Dptx_Params *)PDE_DATA(file_inode(filp));

	pcEdid_Buf = (char *)devm_kzalloc(pstDptx->pstParentDev, DPTX_DEBUGFS_BUF_SIZE, GFP_KERNEL);
	if (pcEdid_Buf == NULL) {
		dptx_err("Could not allocate HPD state buffer ");
		iRetVal = DPTX_RETURN_ENOMEM;
	} else {
		iRetVal = Dptx_Intr_Get_HotPlug_Status(pstDptx, &ucHPD_State);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			dptx_err("Error from Dptx_Intr_Get_HotPlug_Status -> Hot unplugged");
			ucHPD_State = (uint8_t)HPD_STATUS_UNPLUGGED;
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		if (ucHPD_State == (uint8_t)HPD_STATUS_PLUGGED) {
			iRetVal = Dptx_Ext_Get_Stream_Mode(pstDptx, &bSink_MST_Supported, &ucNumOfPluggedPorts);
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				if ((ucNumOfPluggedPorts == 0U) || (ucNumOfPluggedPorts > (uint8_t)PHY_INPUT_STREAM_MAX)) {
					dptx_err("Invalid the Num. of Ports %d-> Get Port Composition first\n", ucNumOfPluggedPorts);
					iRetVal = DPTX_RETURN_ENODEV;
				}
			}
		} else {
			dptx_err("Hot unplugged..");
			iRetVal = DPTX_RETURN_ENODEV;
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {

		bSink_Has_EDID = (bool)true;

		if (bSink_MST_Supported) {
			for (ucStream_Index = 0; ucStream_Index < ucNumOfPluggedPorts; ucStream_Index++) {
				iRetVal = Dptx_Edid_Read_EDID_Over_Sideband_Msg(pstDptx, ucStream_Index);
				if (iRetVal == DPTX_RETURN_NO_ERROR) {
					/*For Coverity*/
					dptx_ext_Print_U8_Buf(pstDptx->pucEdidBuf,    (DPTX_ONE_EDID_BLK_LEN + DPTX_ONE_EDID_BLK_LEN));
				} else {
					if (ucStream_Index == 0U) {
						dptx_info("Sink doesn't support EDID");
						bSink_Has_EDID = (bool)false;
					}
					break;
				}
			}
		} else {
			iRetVal = Dptx_Edid_Read_EDID_I2C_Over_Aux(pstDptx);
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				/*For Coverity*/
				dptx_ext_Print_U8_Buf(pstDptx->pucEdidBuf, (DPTX_ONE_EDID_BLK_LEN + DPTX_ONE_EDID_BLK_LEN));
			} else {
				dptx_info("Sink doesn't support EDID");
				bSink_Has_EDID = (bool)false;
			}
		}
	}

	iFmt_Size = scnprintf(pcEdid_Buf, (size_t)DPTX_DEBUGFS_BUF_SIZE, "%s : %s %d %s connected\n",
							(bSink_MST_Supported == (bool)true) ? "MST mode":"SST mode",
							(bSink_Has_EDID == (bool)true) ? "Sink has EDID from":"Sink doesn't have EDID from",
							ucNumOfPluggedPorts,
							(ucNumOfPluggedPorts > 1U) ? "port is":"ports are");

	if (iFmt_Size > 0) {
		/*For Coverity*/
		stSize = simple_read_from_buffer(usr_buf, cnt, off_set, (void *)pcEdid_Buf, (size_t)iFmt_Size);
	}

	if (pcEdid_Buf != NULL) {
		/*For Coverity*/
		devm_kfree(pstDptx->pstParentDev, pcEdid_Buf);
	}

	return stSize;
}

ssize_t dptx_ext_proc_read_link_training_status(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
	bool bSink_MST_Supported = (bool)false;
	bool bPrevTrainingState = (bool)false;
	char *pcEdid_Buf = NULL;
	uint8_t ucHPD_State = (uint8_t)HPD_STATUS_PLUGGED;
	uint8_t ucNumOfPluggedPorts = 0;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	int32_t iFmt_Size;
	ssize_t stSize = 0;
	struct Dptx_Params *pstDptx;
	
	pstDptx = (struct Dptx_Params *)PDE_DATA(file_inode(filp));

	pcEdid_Buf = (char *)devm_kzalloc(pstDptx->pstParentDev, DPTX_DEBUGFS_BUF_SIZE, GFP_KERNEL);
	if (pcEdid_Buf == NULL) {
		dptx_err("Could not allocate HPD state buffer ");
		iRetVal = DPTX_RETURN_ENOMEM;
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		iRetVal = Dptx_Intr_Get_HotPlug_Status(pstDptx, &ucHPD_State);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			dptx_err("Error from Dptx_Intr_Get_HotPlug_Status -> Hot unplugged");
			ucHPD_State = (uint8_t)HPD_STATUS_UNPLUGGED;
		}
	}

	if (ucHPD_State == (uint8_t)HPD_STATUS_PLUGGED) {
		iRetVal = Dptx_Ext_Get_Stream_Mode(pstDptx, &bSink_MST_Supported, &ucNumOfPluggedPorts);
		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			if ((ucNumOfPluggedPorts == 0U) || (ucNumOfPluggedPorts > (uint8_t)PHY_INPUT_STREAM_MAX)) {
				dptx_err("Invalid the Num. of Ports %d-> Get Port Composition first\n", ucNumOfPluggedPorts);
				iRetVal = DPTX_RETURN_ENODEV;
			}
		}
	} else {
		dptx_err("Hot unplugged..");
		iRetVal = DPTX_RETURN_ENODEV;
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		iRetVal = Dptx_Link_Get_LinkTraining_Status(pstDptx, &bPrevTrainingState);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR) {
			dptx_err("Error from Dptx_Link_Get_LinkTraining_Status()");
			bPrevTrainingState = (bool)false;
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		if (bPrevTrainingState == (bool)false) {
			iRetVal = Dptx_Link_Perform_BringUp(pstDptx, bSink_MST_Supported);
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				iRetVal = Dptx_Link_Perform_Training(pstDptx, pstDptx->ucMax_Rate, pstDptx->ucMax_Lanes);
				if (iRetVal != DPTX_RETURN_NO_ERROR) {
					/*For Coverity*/
					dptx_err("Error from Dptx_Link_Perform_Training()");
				}
			} else {
				/*For Coverity*/
				dptx_err("Error from Dptx_Link_Perform_BringUp()");
			}
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		if (bSink_MST_Supported == (bool)true) {
			iRetVal = Dptx_Ext_Set_Topology_Configuration(pstDptx, pstDptx->ucNumOfPorts, pstDptx->bSideBand_MSG_Supported);
			if (iRetVal !=  DPTX_RETURN_NO_ERROR) {
				/*For Coverity*/
				dptx_err("Error from Dptx_Ext_Set_Topology_Configuration()");
			}
		}
	}

	if (iRetVal ==  DPTX_RETURN_NO_ERROR) {
		iFmt_Size = scnprintf(pcEdid_Buf, (size_t)DPTX_DEBUGFS_BUF_SIZE, "%s : link training %s with %s on %d lanes\n",
					(bSink_MST_Supported == (bool)true) ? "MST mode" : "SST mode",
					(bPrevTrainingState == (bool)true) ? "already successed" : "newly successed",
					(pstDptx->stDptxLink.ucLinkRate == (uint8_t)DPTX_PHYIF_CTRL_RATE_RBR) ? "RBR" :
					(pstDptx->stDptxLink.ucLinkRate == (uint8_t)DPTX_PHYIF_CTRL_RATE_HBR) ? "HBR" :
					(pstDptx->stDptxLink.ucLinkRate == (uint8_t)DPTX_PHYIF_CTRL_RATE_HBR2) ? "HB2" : "HBR3",
					pstDptx->stDptxLink.ucNumOfLanes);

		if (iFmt_Size > 0) {
			/*For Coverity*/
			stSize = simple_read_from_buffer(usr_buf, cnt, off_set, (void *)pcEdid_Buf, (size_t)iFmt_Size);
		}
	}

	if (pcEdid_Buf != NULL) {
		/*For Coverity*/
		devm_kfree(pstDptx->pstParentDev, pcEdid_Buf);
	}

	return  stSize;
}

ssize_t dptx_ext_proc_read_video_timing(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
	bool bSink_MST_Supported;
	char	*pcVideoTiming_Buf = NULL;
	uint8_t ucStream_Index;
	uint8_t ucHPD_State, ucNumOfPluggedPorts = 0;
	uint16_t usH_Active[PHY_INPUT_STREAM_MAX] = { 0, };
	uint16_t usV_Active[PHY_INPUT_STREAM_MAX] = { 0, };
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	int32_t iFmt_Size;
	ssize_t stSize = 0;
	struct Dptx_Dtd_Params stDtd_Params;
	struct Dptx_Params *pstDptx;

	pstDptx = (struct Dptx_Params *)PDE_DATA(file_inode(filp));

	pcVideoTiming_Buf = (char *)devm_kzalloc(pstDptx->pstParentDev, DPTX_DEBUGFS_BUF_SIZE, GFP_KERNEL);
	if (pcVideoTiming_Buf == NULL) {
		dptx_err("Could not allocate Video state buffer ");
		iRetVal = DPTX_RETURN_ENOMEM;
	} else {
		iRetVal = Dptx_Intr_Get_HotPlug_Status(pstDptx, &ucHPD_State);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			dptx_err("Error from Dptx_Intr_Get_HotPlug_Status() -> Hot unplugged");
			ucHPD_State = (uint8_t)HPD_STATUS_UNPLUGGED;
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		if (ucHPD_State == (uint8_t)HPD_STATUS_PLUGGED) {
			iRetVal = Dptx_Ext_Get_Stream_Mode(pstDptx, &bSink_MST_Supported, &ucNumOfPluggedPorts);
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				if ((ucNumOfPluggedPorts == 0U) || (ucNumOfPluggedPorts > (uint8_t)PHY_INPUT_STREAM_MAX)) {
					dptx_err("Invalid the Num. of Ports %d-> Get Port Composition first\n", ucNumOfPluggedPorts);
					iRetVal = DPTX_RETURN_ENODEV;
				}
			}
		} else {
			dptx_err("Hot unplugged..");
			iRetVal = DPTX_RETURN_ENODEV;
		}
	}

	if (iRetVal ==  DPTX_RETURN_NO_ERROR) {
		for (ucStream_Index = 0; ucStream_Index < ucNumOfPluggedPorts; ucStream_Index++) {
			iRetVal = Dptx_VidIn_Get_Configured_Timing(pstDptx, ucStream_Index, &stDtd_Params);
			if (iRetVal != DPTX_RETURN_NO_ERROR) {
				dptx_err("Error from Dptx_VidIn_Get_Configured_Timing()");
				break;
			}

			usH_Active[ucStream_Index] = stDtd_Params.h_active;
			usV_Active[ucStream_Index] = stDtd_Params.v_active;
		}
	}

	if (iRetVal ==  DPTX_RETURN_NO_ERROR) {
		iFmt_Size = scnprintf(pcVideoTiming_Buf, (size_t)DPTX_DEBUGFS_BUF_SIZE, "%s : 1st %d x %d, 2nd : %d x %d, 3rd : %d x %d, 4th : %d x %d\n",
						(bSink_MST_Supported == (bool)true) ? "MST mode":"SST mode",
						usH_Active[PHY_INPUT_STREAM_0], usV_Active[PHY_INPUT_STREAM_0],
						usH_Active[PHY_INPUT_STREAM_1], usV_Active[PHY_INPUT_STREAM_1],
						usH_Active[PHY_INPUT_STREAM_2], usV_Active[PHY_INPUT_STREAM_2],
						usH_Active[PHY_INPUT_STREAM_3], usV_Active[PHY_INPUT_STREAM_3]);

		if (iFmt_Size > 0) {
			/*For Coverity*/
			stSize = simple_read_from_buffer(usr_buf, cnt, off_set, (void *)pcVideoTiming_Buf, (size_t)iFmt_Size);
		}
	}

	if (pcVideoTiming_Buf != NULL) {
		/*For Coverity*/
		devm_kfree(pstDptx->pstParentDev, pcVideoTiming_Buf);
	}

	return stSize;
}

ssize_t dptx_ext_proc_write_video_timing(struct file *filp, const char __user *buffer, size_t cnt, loff_t *off_set)
{
	char *pcVideoTiming_Buf = NULL;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR, iScanRet = 0;
	uint32_t uiVideoCode, uiStream_Index;
	ssize_t stSize = 0;
	struct Dptx_Dtd_Params stDtd_Params;
	struct Dptx_Params *pstDptx;

	pstDptx = (struct Dptx_Params *)PDE_DATA(file_inode(filp));

	pcVideoTiming_Buf = (char *)devm_kzalloc(pstDptx->pstParentDev, DPTX_DEBUGFS_BUF_SIZE, GFP_KERNEL);
	if (pcVideoTiming_Buf == NULL) {
		dptx_err("Could not allocate Video buffer");
		iRetVal = DPTX_RETURN_ENOMEM;
	} else {
		stSize = simple_write_to_buffer(pcVideoTiming_Buf, cnt, off_set, buffer, cnt);
		if (stSize >= 0) {
			if ((size_t)stSize != cnt) {
				dptx_err("Can't get input data : %ld <-> %ld ", stSize, cnt);
				iRetVal =  DPTX_RETURN_ENOENT;
			}
		} else {
			dptx_err("Can't get input data : %ld <-> %ld ", stSize, cnt);
			iRetVal =  DPTX_RETURN_ENOENT;
		}
	}

	if (iRetVal ==  DPTX_RETURN_NO_ERROR) {
		pcVideoTiming_Buf[cnt] = '\0';

		iScanRet = sscanf(pcVideoTiming_Buf, "%u %u", &uiStream_Index, &uiVideoCode);
		if (iScanRet < 0) {
			dptx_err("Can't scan input data");
			iRetVal = DPTX_RETURN_ENOENT;
		} else {
			/*For Coverity*/
			dptx_info("Stream index : %d, Vidoe code : %d", (u32)uiStream_Index, (u32)uiVideoCode);
		}
	}

	if (iRetVal ==  DPTX_RETURN_NO_ERROR) {
		iRetVal = Dptx_VidIn_Fill_Dtd(&stDtd_Params, uiVideoCode, 60000, (u8)VIDEO_FORMAT_CEA_861);
		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			iRetVal = Dptx_VidIn_Set_Detailed_Timing(pstDptx, (uint8_t)(uiStream_Index & 0xFFU), &stDtd_Params);
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				iRetVal = Dptx_VidIn_Set_Stream_Enable(pstDptx, (bool)true, (uint8_t)(uiStream_Index & 0xFFU));
				if (iRetVal != DPTX_RETURN_NO_ERROR) {
					/*For Coverity*/
					dptx_err("Error from Dptx_VidIn_Set_Stream_Enable()");
				}
			} else {
				/*For Coverity*/
				dptx_err("Error from Dptx_VidIn_Set_Detailed_Timing()");
			}
		} else {
			/*For Coverity*/
			dptx_err("Can't find VIC %d from dtd", (u32)uiVideoCode);
		}
	}

	if (pcVideoTiming_Buf != NULL) {
		/*For Coverity*/
		devm_kfree(pstDptx->pstParentDev, pcVideoTiming_Buf);
	}

	return stSize;
}



ssize_t dptx_ext_proc_read_audio_configuration(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
	bool bSink_MST_Supported;
	char *pcAudioConf_Buf = NULL;
	uint8_t ucInfType,  ucNumOfCh, ucDataWidth;
	uint8_t ucHBRMode, ucSamplingFreq, ucOrgSamplingFreq;
	uint8_t ucNumOfPluggedPorts = 0, ucStream_Index;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	int32_t iFmt_Size;
	ssize_t stSize = 0;
	struct Dptx_Params *pstDptx;

	pstDptx = (struct Dptx_Params *)PDE_DATA(file_inode(filp));

	pcAudioConf_Buf = (char *)devm_kzalloc(pstDptx->pstParentDev, DPTX_DEBUGFS_BUF_SIZE, GFP_KERNEL);
	if (pcAudioConf_Buf == NULL) {
		dptx_err("Could not allocate HPD buffer");
		iRetVal = DPTX_RETURN_ENOMEM;
	} else {
		iRetVal = Dptx_Ext_Get_Stream_Mode(pstDptx, &bSink_MST_Supported, &ucNumOfPluggedPorts);
		if (iRetVal ==  DPTX_RETURN_NO_ERROR) {
			if ((ucNumOfPluggedPorts == 0U) || (ucNumOfPluggedPorts > (uint8_t)PHY_INPUT_STREAM_MAX)) {
				dptx_err("Invalid the Num. of Ports %d -> Get Port Composition first\n", ucNumOfPluggedPorts);
				iRetVal = DPTX_RETURN_ENODEV;
			}
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		for (ucStream_Index = 0; ucStream_Index < ucNumOfPluggedPorts; ucStream_Index++) {
			Dptx_AudIn_Get_Input_InterfaceType(pstDptx, ucStream_Index, &ucInfType);
			Dptx_AudIn_Get_DataWidth(pstDptx, ucStream_Index, &ucDataWidth);
			Dptx_AudIn_Get_MaxNumOfChannels(pstDptx, ucStream_Index, &ucNumOfCh);
			Dptx_AudIn_Get_HBR_En(pstDptx, ucStream_Index, &ucHBRMode);
			Dptx_AudIn_Get_SamplingFreq(pstDptx, &ucSamplingFreq, &ucOrgSamplingFreq);

			iFmt_Size = scnprintf(pcAudioConf_Buf, (size_t)DPTX_DEBUGFS_BUF_SIZE, "DP %d : IT(%d), DW(%d), NumOfCh(%d), HBR(%d), SFq(%d), OrgSF(%d)\n",
								ucStream_Index,
								ucInfType,
								ucDataWidth,
								ucNumOfCh,
								ucHBRMode,
								ucSamplingFreq,
								ucOrgSamplingFreq);

			if (iFmt_Size > 0) {
				/*For Coverity*/
				stSize = simple_read_from_buffer(usr_buf, cnt, off_set, (void *)pcAudioConf_Buf, (size_t)iFmt_Size);
			}
		}
	}

	if (pcAudioConf_Buf != NULL ) {
		/*For Coverity*/
		devm_kfree(pstDptx->pstParentDev, pcAudioConf_Buf);
	}

	return stSize;
}

ssize_t dptx_ext_proc_write_audio_configuration(struct file *filp, const char __user *buffer, size_t cnt, loff_t *off_set)
{
	char *pcAudioConf_Buf = NULL;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiStream_Index, uiInfType;
	uint32_t uiDataWidth, uiNumOfCh;
	uint32_t uiHBRMode, uiSamplingFreq;
	uint32_t uiOrgSamplingFreq;
	ssize_t  stSize = 0;
	struct Dptx_Audio_Params stAudioParams;
	struct Dptx_Params *pstDptx;

	pstDptx = (struct Dptx_Params *)PDE_DATA(file_inode(filp));

	pcAudioConf_Buf = (char *)devm_kzalloc(pstDptx->pstParentDev, DPTX_DEBUGFS_BUF_SIZE, GFP_KERNEL);
	if (pcAudioConf_Buf == NULL) {
		dptx_err("Could not allocate HPD buf");
		iRetVal = DPTX_RETURN_ENOMEM;
	} else {
		stSize = simple_write_to_buffer(pcAudioConf_Buf, cnt, off_set, buffer, cnt);
		if (stSize >= 0) {
			if ((size_t)stSize != cnt) {
				dptx_err("Can't get input data : %ld <-> %ld ", stSize, cnt);
				iRetVal =  DPTX_RETURN_ENOENT;
			}
		} else {
			dptx_err("Can't get input data : %ld <-> %ld ", stSize, cnt);
			iRetVal =  DPTX_RETURN_ENOENT;
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		pcAudioConf_Buf[cnt] = '\0';

		iRetVal = sscanf(pcAudioConf_Buf, "%u %u %u %u %u %u %u",
							&uiStream_Index,
							&uiInfType,
							&uiDataWidth,
							&uiNumOfCh,
							&uiHBRMode,
							&uiSamplingFreq,
							&uiOrgSamplingFreq);
		if (iRetVal < 0) {
			dptx_err("Can't scan input data");
			iRetVal = DPTX_RETURN_ENOENT;
		}
	}

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		stAudioParams.ucInput_InterfaceType     = (uint8_t)(uiInfType & 0xFFU);
		stAudioParams.ucInput_DataWidth         = (uint8_t)(uiDataWidth & 0xFFU);
		stAudioParams.ucInput_Max_NumOfchannels = (uint8_t)(uiNumOfCh & 0xFFU);
		stAudioParams.ucInput_HBR_Mode          = (uint8_t)(uiHBRMode & 0xFFU);
		stAudioParams.ucIEC_OriginSamplingFreq  = (uint8_t)(uiOrgSamplingFreq & 0xFFU);
		stAudioParams.ucIEC_Sampling_Freq       = (uint8_t)(uiSamplingFreq & 0xFFU);
		stAudioParams.ucInput_TimestampVersion	= 0x12;

		Dptx_AudIn_Configure(pstDptx, (uint8_t)(uiStream_Index & 0xFFU), &stAudioParams);

		dptx_info("\n[Write audio Conf]DP %d: Input type(%d), Data Width(%d), NumOfCh(%d), HBR(%d), SFreq(%d), OrgSFreq(%d)\n",
							uiStream_Index,
							uiInfType,
							uiDataWidth,
							uiNumOfCh,
							uiHBRMode,
							uiSamplingFreq,
							uiOrgSamplingFreq);
	}

	if (pcAudioConf_Buf != NULL ) {
		/*For Coverity*/
		devm_kfree(pstDptx->pstParentDev, pcAudioConf_Buf);
	}

	return stSize;
}

ssize_t dptx_ext_proc_read_str_status(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	struct Dptx_Params *pstDptx;

	pstDptx = (struct Dptx_Params *)PDE_DATA(file_inode(filp));

	iRetVal = Dpv14_Tx_Suspend_T(pstDptx);

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		mdelay(5000);

		iRetVal = Dpv14_Tx_Resume_T(pstDptx);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			/*For Coverity*/
			dptx_err("Error from Dpv14_Tx_Resume_T()");
		}
	}

	dptx_info("cnt %lu, off_set: 0x%p, off_set: 0x%p", cnt, usr_buf, off_set);

	return iRetVal;
}

int32_t Dptx_Ext_Proc_Interface_Init(struct Dptx_Params *pstDptx)
{
	pstDptx->pstDP_Proc_Dir = proc_mkdir("dptx_v14", NULL);
	if (pstDptx->pstDP_Proc_Dir == NULL) {
		/*For Coverity*/
		dptx_err("Could not create file system @ /proc/dptx_v14");
	}

	pstDptx->pstDP_HPD_Dir = proc_create_data("hpd", ((umode_t)S_IFREG | (umode_t)0444), pstDptx->pstDP_Proc_Dir, &proc_fops_hpd_state, pstDptx);
	if (pstDptx->pstDP_HPD_Dir == NULL) {
		/*For Coverity*/
		dptx_err("Could not create file system data @ /proc/dptx_v14/hpd");
	}

	pstDptx->pstDP_Topology_Dir = proc_create_data("topology", ((umode_t)S_IFREG | (umode_t)0444), pstDptx->pstDP_Proc_Dir, &proc_fops_topology_state, pstDptx);
	if (pstDptx->pstDP_Topology_Dir == NULL) {
		/*For Coverity*/
		dptx_err("Could not create file system data @ /proc/dptx_v14/topology");
	}

	pstDptx->pstDP_EDID_Dir = proc_create_data("edid", ((umode_t)S_IFREG | (umode_t)0444), pstDptx->pstDP_Proc_Dir, &proc_fops_edid_data, pstDptx);
	if (pstDptx->pstDP_EDID_Dir == NULL) {
		/*For Coverity*/
		dptx_err("Could not create file system data @ /proc/dptx_v14/edid");
	}

	pstDptx->pstDP_LinkT_Dir = proc_create_data("link", ((umode_t)S_IFREG | (umode_t)0444), pstDptx->pstDP_Proc_Dir, &proc_fops_linkT_data, pstDptx);
	if (pstDptx->pstDP_LinkT_Dir == NULL) {
		/*For Coverity*/
		dptx_err("Could not create file system data @ /proc/dptx_v14/link");
	}

	pstDptx->pstDP_LinkT_Dir = proc_create_data("str", ((umode_t)S_IFREG | (umode_t)0444), pstDptx->pstDP_Proc_Dir, &proc_fops_str_data, pstDptx);
	if (pstDptx->pstDP_LinkT_Dir == NULL) {
		/*For Coverity*/
		dptx_err("Could not create file system data @ /proc/dptx_v14/str");
	}

	pstDptx->pstDP_Video_Dir = proc_create_data("video", ((umode_t)S_IFREG | (umode_t)0666), pstDptx->pstDP_Proc_Dir, &proc_fops_video_data, pstDptx);
	if (pstDptx->pstDP_Video_Dir == NULL) {
		/*For Coverity*/
		dptx_err("Could not create file system data @ /proc/dptx_v14/video");
	}


	pstDptx->pstDP_Auio_Dir = proc_create_data("audio", ((umode_t)S_IFREG | (umode_t)0444), pstDptx->pstDP_Proc_Dir, &proc_fops_audio_data, pstDptx);
	if (pstDptx->pstDP_Auio_Dir == NULL) {
		/*For Coverity*/
		dptx_err("Could not create file system data @ /proc/dptx_v14/audio");
	}

	return DPTX_RETURN_NO_ERROR;
}


