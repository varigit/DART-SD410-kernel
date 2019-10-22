/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Jijie Wang <jijie_wang@ilitek.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 *
 */

#include "ilitek_ts.h"
#include "ilitek_common.h"
#include "ilitek_protocol.h"

#include <linux/firmware.h>
#include <linux/vmalloc.h>

#ifdef ILITEK_UPDATE_FW
#if !ILITEK_UPGRADE_WITH_BIN
#include "ilitek_fw.h"
#endif
#endif

static uint32_t UpdateCRC(uint32_t crc, uint8_t newbyte)
{
	char i;			// loop counter
#define CRC_POLY 0x8408		// CRC16-CCITT FCS (X^16+X^12+X^5+1)

	crc = crc ^ newbyte;

	for (i = 0; i < 8; i++) {
		if (crc & 0x01) {
			crc = crc >> 1;
			crc ^= CRC_POLY;
		} else {
			crc = crc >> 1;
		}
	}
	return crc;
}

static int32_t check_busy(int32_t delay)
{
	int32_t i;
	uint8_t inbuf[2], outbuf[2];
	for (i = 0; i < 1000; i++) {
		inbuf[0] = ILITEK_TP_CMD_GET_SYSTEM_BUSY;
		if (ilitek_i2c_write_and_read(inbuf, 1, delay, outbuf, 1) < 0) {
			return ILITEK_I2C_TRANSFER_ERR;
		}
		if (outbuf[0] == ILITEK_TP_SYSTEM_READY) {
			//tp_log_info("check_busy i = %d\n", i);
			return 0;
		}
	}
	tp_log_info("check_busy error\n");
	return -1;
}

static int32_t ilitek_changetoblmode(bool mode)
{
	int32_t i = 0;
	uint8_t outbuf[64];
	if(api_protocol_set_cmd(ILITEK_TP_CMD_READ_MODE, NULL, outbuf) < ILITEK_SUCCESS)
		return ILITEK_I2C_TRANSFER_ERR;
	msleep(30);
	tp_log_info("ilitek ic. mode =%d , it's %s\n", outbuf[0],
		    ((outbuf[0] == 0x5A) ? "AP MODE" : ((outbuf[0] == ILITEK_TP_MODE_BOOTLOADER) ? "BL MODE" : "UNKNOW MODE")));
	if ((outbuf[0] == ILITEK_TP_MODE_APPLICATION && !mode) || (outbuf[0] == ILITEK_TP_MODE_BOOTLOADER && mode)) {
		if (mode) {
			tp_log_info("ilitek change to BL mode ok already BL mode\n");
		} else {
			tp_log_info("ilitek change to AP mode ok already AP mode\n");
		}
	} else {
		for (i = 0; i < 5; i++) {
			if(api_protocol_set_cmd(ILITEK_TP_CMD_WRITE_ENABLE, NULL, outbuf) < ILITEK_SUCCESS) {
				return ILITEK_I2C_TRANSFER_ERR;
			}
			msleep(20);
			if (mode) {
				if(api_protocol_set_cmd(ILITEK_TP_CMD_SET_BLMODE, NULL, outbuf) < ILITEK_SUCCESS) {
					return ILITEK_I2C_TRANSFER_ERR;
				}
			} else {
				if(api_protocol_set_cmd(ILITEK_TP_CMD_SET_APMODE, NULL, outbuf) < ILITEK_SUCCESS) {
					return ILITEK_I2C_TRANSFER_ERR;
				}
			}
			msleep(500);
			if(api_protocol_set_cmd(ILITEK_TP_CMD_READ_MODE, NULL, outbuf) < ILITEK_SUCCESS)
				return ILITEK_I2C_TRANSFER_ERR;
			msleep(30);
			tp_log_info("ilitek ic. mode =%d , it's  %s\n", outbuf[0],
				    ((outbuf[0] == ILITEK_TP_MODE_APPLICATION) ? "AP MODE" : ((outbuf[0] == ILITEK_TP_MODE_BOOTLOADER) ? "BL MODE" : "UNKNOW MODE")));
			if ((outbuf[0] == ILITEK_TP_MODE_APPLICATION && !mode) || (outbuf[0] == ILITEK_TP_MODE_BOOTLOADER && mode)) {
				if (mode) {
					tp_log_info("ilitek change to BL mode ok\n");
				} else {
					tp_log_info("ilitek change to AP mode ok\n");
				}
				break;
			}
		}
	}
	if (i >= 5) {
		if (mode) {
			tp_log_err("change to bl mode err, 0x%X\n", outbuf[0]);
			return ILITEK_TP_CHANGETOBL_ERR;
		} else {
			tp_log_err("change to ap mode err, 0x%X\n", outbuf[0]);
			return ILITEK_TP_CHANGETOAP_ERR;
		}
	} else {
		return 0;
	}
}

static int32_t ilitek_upgrade_ptl_1_6(uint32_t df_startaddr, uint32_t df_endaddr, uint32_t df_checksum,
			     uint32_t ap_startaddr, uint32_t ap_endaddr, uint32_t ap_checksum, uint8_t *CTPM_FW)
{
	int32_t ret = 0, upgrade_status = 0, i = 0, j = 0, k = 0, tmp_ap_end_addr = 0;
	uint8_t buf[64] = { 0 };
	if (ilitek_data->bl_ver[0] >= 1 && ilitek_data->bl_ver[1] >= 4) {
		buf[0] = (uint8_t)ILITEK_TP_CMD_WRITE_ENABLE;	//0xc4
		buf[1] = 0x5A;
		buf[2] = 0xA5;
		buf[3] = 0x81;
		if (!ilitek_data->has_df) {
			tp_log_info("ilitek no df data set df_endaddr 0x1ffff\n");
			df_endaddr = 0x1ffff;
			df_checksum = 0x1000 * 0xff;
			buf[4] = df_endaddr >> 16;
			buf[5] = (df_endaddr >> 8) & 0xFF;
			buf[6] = (df_endaddr) & 0xFF;
			buf[7] = df_checksum >> 16;
			buf[8] = (df_checksum >> 8) & 0xFF;
			buf[9] = df_checksum & 0xFF;
		} else {
			buf[4] = df_endaddr >> 16;
			buf[5] = (df_endaddr >> 8) & 0xFF;
			buf[6] = (df_endaddr) & 0xFF;
			buf[7] = df_checksum >> 16;
			buf[8] = (df_checksum >> 8) & 0xFF;
			buf[9] = df_checksum & 0xFF;
		}
		tp_log_info("ilitek df_startaddr=0x%X, df_endaddr=0x%X, df_checksum=0x%X\n", df_startaddr, df_endaddr, df_checksum);
		ret = ilitek_i2c_write(buf, 10);
		if (ret < 0) {
			tp_log_err("ilitek_i2c_write\n");
		}
		msleep(20);
		j = 0;
		for (i = df_startaddr; i < df_endaddr; i += 32) {
			buf[0] = ILITEK_TP_CMD_WRITE_DATA;
			for (k = 0; k < 32; k++) {
				if (ilitek_data->has_df) {
					if ((i + k) >= df_endaddr) {
						buf[1 + k] = 0x00;
					} else {
						buf[1 + k] = CTPM_FW[i + 32 + k];
					}
				} else {
					buf[1 + k] = 0xff;
				}
			}

			ret = ilitek_i2c_write(buf, 33);
			if (ret < 0) {
				tp_log_err("ilitek_i2c_write_and_read\n");
				return ILITEK_I2C_TRANSFER_ERR;
			}
			j += 1;

#ifdef ILI_UPDATE_BY_CHECK_INT
			for (k = 0; k < 40; k++) {
				if (!(ilitek_poll_int())) {
					break;
				} else {
					mdelay(1);
				}
			}
			if (k >= 40) {
				tp_log_err("upgrade check int fail retry = %d\n", k);
			}
#else
			if ((j % (ilitek_data->page_number)) == 0) {
				//msleep(30);
				mdelay(20);
			}
			mdelay(10);
#endif
			upgrade_status = ((i - df_startaddr) * 100) / (df_endaddr - df_startaddr);
			if (upgrade_status % 10 == 0) {
				tp_log_info("%c ilitek ILITEK: Firmware Upgrade(Data flash), %02d%c.\n", 0x0D, upgrade_status, '%');
			}
		}
		buf[0] = (uint8_t)ILITEK_TP_CMD_WRITE_ENABLE;	//0xc4
		buf[1] = 0x5A;
		buf[2] = 0xA5;
		buf[3] = 0x80;
		if (((ap_endaddr + 1) % 32) != 0) {
			tp_log_info("ap_endaddr = 0x%X\n", (int32_t)(ap_endaddr + 32 + 32 - ((ap_endaddr + 1) % 32)));
			buf[4] = (ap_endaddr + 32 + 32 - ((ap_endaddr + 1) % 32)) >> 16;
			buf[5] = ((ap_endaddr + 32 + 32 - ((ap_endaddr + 1) % 32)) >> 8) & 0xFF;
			buf[6] = ((ap_endaddr + 32 + 32 - ((ap_endaddr + 1) % 32))) & 0xFF;
			tp_log_info("ap_checksum = 0x%X\n", (int32_t)(ap_checksum + (32 + 32 - ((ap_endaddr + 1) % 32)) * 0xff));
			buf[7] = (ap_checksum + (32 + 32 - ((ap_endaddr + 1) % 32)) * 0xff) >> 16;
			buf[8] = ((ap_checksum + (32 + 32 - ((ap_endaddr + 1) % 32)) * 0xff) >> 8) & 0xFF;
			buf[9] = (ap_checksum + (32 + 32 - ((ap_endaddr + 1) % 32)) * 0xff) & 0xFF;
		} else {
			tp_log_info("32 0 ap_endaddr  32 = 0x%X\n", (int32_t)(ap_endaddr + 32));
			tp_log_info("ap_endaddr = 0x%X\n", (int32_t)(ap_endaddr + 32));
			buf[4] = (ap_endaddr + 32) >> 16;
			buf[5] = ((ap_endaddr + 32) >> 8) & 0xFF;
			buf[6] = ((ap_endaddr + 32)) & 0xFF;
			tp_log_info("ap_checksum = 0x%X\n", (int32_t)(ap_checksum + 32 * 0xff));
			buf[7] = (ap_checksum + 32 * 0xff) >> 16;
			buf[8] = ((ap_checksum + 32 * 0xff) >> 8) & 0xFF;
			buf[9] = (ap_checksum + 32 * 0xff) & 0xFF;
		}
		ret = ilitek_i2c_write(buf, 10);
		msleep(20);
		tmp_ap_end_addr = ap_endaddr;
		ap_endaddr += 32;
		j = 0;
		for (i = ap_startaddr; i < ap_endaddr; i += 32) {
			buf[0] = ILITEK_TP_CMD_WRITE_DATA;
			for (k = 0; k < 32; k++) {
				if ((i + k) > tmp_ap_end_addr) {
					buf[1 + k] = 0xff;
				} else {
					buf[1 + k] = CTPM_FW[i + k + 32];
				}
			}

			buf[0] = 0xc3;
			ret = ilitek_i2c_write(buf, 33);
			if (ret < 0) {
				tp_log_err("%s, ilitek_i2c_write_and_read\n", __func__);
				return ILITEK_I2C_TRANSFER_ERR;
			}
			j += 1;
#ifdef ILI_UPDATE_BY_CHECK_INT
			for (k = 0; k < 40; k++) {
				if (!(ilitek_poll_int())) {
					break;
				} else {
					mdelay(1);
				}
			}
			if (k >= 40) {
				tp_log_err("upgrade check int fail retry = %d\n", k);
			}
#else
			if ((j % (ilitek_data->page_number)) == 0) {
				mdelay(20);
			}
			mdelay(10);
#endif
			upgrade_status = (i * 100) / ap_endaddr;
			if (upgrade_status % 10 == 0) {
				tp_log_info("%c ilitek ILITEK: Firmware Upgrade(AP), %02d%c.\n", 0x0D, upgrade_status, '%');
			}
		}
	} else {
		tp_log_info("not support this bl version upgrade flow\n");
	}
	return 0;
}

static int32_t ilitek_upgrade_ptl_1_7(uint32_t df_startaddr, uint32_t df_endaddr, uint32_t ap_startaddr, uint32_t ap_endaddr, uint8_t *CTPM_FW)
{
	int32_t ret = 0, upgrade_status = 0, i = 0, k = 0, CRC_DF = 0, CRC_AP = 0;
	uint8_t buf[64] = { 0 };
	for (i = df_startaddr + 2; i < df_endaddr; i++) {
		CRC_DF = UpdateCRC(CRC_DF, CTPM_FW[i + 32]);
	}
	tp_log_info("CRC_DF = 0x%X\n", CRC_DF);
	for (i = ap_startaddr; i < ap_endaddr - 1; i++) {
		CRC_AP = UpdateCRC(CRC_AP, CTPM_FW[i + 32]);
	}
	tp_log_info("CRC_AP = 0x%x\n", CRC_AP);

	buf[0] = (uint8_t)ILITEK_TP_CMD_WRITE_ENABLE;	//0xc4
	buf[1] = 0x5A;
	buf[2] = 0xA5;
	buf[3] = 0x01;
	buf[4] = df_endaddr >> 16;
	buf[5] = (df_endaddr >> 8) & 0xFF;
	buf[6] = (df_endaddr) & 0xFF;
	buf[7] = CRC_DF >> 16;
	buf[8] = (CRC_DF >> 8) & 0xFF;
	buf[9] = CRC_DF & 0xFF;
	ret = ilitek_i2c_write(buf, 10);
	if (ret < 0) {
		tp_log_err("ilitek_i2c_write\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}
	check_busy(1);
	if (((df_endaddr) % 32) != 0) {
		df_endaddr += 32;
	}
	tp_log_info("write data to df mode\n");
	for (i = df_startaddr; i < df_endaddr; i += 32) {
		// we should do delay when the size is equal to 512 bytes
		buf[0] = (uint8_t)ILITEK_TP_CMD_WRITE_DATA;
		for (k = 0; k < 32; k++) {
			buf[1 + k] = CTPM_FW[i + k + 32];
		}
		if (ilitek_i2c_write(buf, 33) < 0) {
			tp_log_err("%s, ilitek_i2c_write_and_read\n", __func__);
			return ILITEK_I2C_TRANSFER_ERR;
		}
		check_busy(1);
		upgrade_status = ((i - df_startaddr) * 100) / (df_endaddr - df_startaddr);
		//tp_log_info("%c ilitek ILITEK: Firmware Upgrade(Data flash), %02d%c.\n",0x0D,upgrade_status,'%');
	}

	buf[0] = (uint8_t)0xC7;
	if (ilitek_i2c_write(buf, 1) < 0) {
		tp_log_err("ilitek_i2c_write\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}
	check_busy(1);
	buf[0] = (uint8_t)0xC7;
	ilitek_i2c_write_and_read(buf, 1, 10, buf, 4);
	tp_log_info("upgrade end write c7 read 0x%X, 0x%X, 0x%X, 0x%X\n", buf[0], buf[1], buf[2], buf[3]);
	if (CRC_DF != buf[1] * 256 + buf[0]) {
		tp_log_err("CRC DF compare error\n");
		//return ILITEK_UPDATE_FAIL;
	} else {
		tp_log_info("CRC DF compare Right\n");
	}
	buf[0] = (uint8_t)ILITEK_TP_CMD_WRITE_ENABLE;	//0xc4
	buf[1] = 0x5A;
	buf[2] = 0xA5;
	buf[3] = 0x00;
	buf[4] = (ap_endaddr + 1) >> 16;
	buf[5] = ((ap_endaddr + 1) >> 8) & 0xFF;
	buf[6] = ((ap_endaddr + 1)) & 0xFF;
	buf[7] = 0;
	buf[8] = (CRC_AP & 0xFFFF) >> 8;
	buf[9] = CRC_AP & 0xFFFF;
	ret = ilitek_i2c_write(buf, 10);
	if (ret < 0) {
		tp_log_err("ilitek_i2c_write\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}
	check_busy(1);
	if (((ap_endaddr + 1) % 32) != 0) {
		tp_log_info("ap_endaddr += 32\n");
		ap_endaddr += 32;
	}
	tp_log_info("write data to AP mode\n");
	for (i = ap_startaddr; i < ap_endaddr; i += 32) {
		buf[0] = (uint8_t)ILITEK_TP_CMD_WRITE_DATA;
		for (k = 0; k < 32; k++) {
			buf[1 + k] = CTPM_FW[i + k + 32];
		}
		if (ilitek_i2c_write(buf, 33) < 0) {
			tp_log_err("ilitek_i2c_write\n");
			return ILITEK_I2C_TRANSFER_ERR;
		}
		check_busy(1);
		upgrade_status = ((i - ap_startaddr) * 100) / (ap_endaddr - ap_startaddr);
		//tp_log_info("%c ilitek ILITEK: Firmware Upgrade(AP), %02d%c.\n",0x0D,upgrade_status,'%');
	}
	for (i = 0; i < 20; i++) {
		buf[0] = (uint8_t)0xC7;
		if (ilitek_i2c_write(buf, 1) < 0) {
			tp_log_err("ilitek_i2c_write\n");
			return ILITEK_I2C_TRANSFER_ERR;
		}
		check_busy(1);
		buf[0] = (uint8_t)0xC7;
		ret = ilitek_i2c_write_and_read(buf, 1, 10, buf, 4);
		if (ret < 0) {
			tp_log_err("ilitek_i2c_write_and_read 0xc7\n");
			return ILITEK_I2C_TRANSFER_ERR;
		}
		tp_log_info("upgrade end write c7 read 0x%X, 0x%X, 0x%X, 0x%X\n", buf[0], buf[1], buf[2], buf[3]);
		if (CRC_AP != buf[1] * 256 + buf[0]) {
			tp_log_err("CRC compare error retry\n");
			//return ILITEK_TP_UPGRADE_FAIL;
		} else {
			tp_log_info("CRC compare Right\n");
			break;
		}
	}
	if (i >= 20) {
		tp_log_err("CRC compare error\n");
		return ILITEK_TP_UPGRADE_FAIL;
	}
	return 0;
}
static int32_t ilitek_upgrade_ptl_1_8(uint32_t df_startaddr, uint32_t df_endaddr, uint32_t ap_startaddr, uint32_t ap_endaddr, uint8_t *CTPM_FW)
{
	int32_t ret = 0, upgrade_status = 0, i = 0, k = 0, CRC_DF = 0, CRC_AP = 0, data_len = 2048;
	uint8_t buf[64] = { 0 };
	uint8_t *txbuf;
	txbuf = kmalloc(data_len + 1, GFP_KERNEL);
	if (txbuf == NULL)
		return -ENOMEM;
	for (i = df_startaddr + 2; i < df_endaddr; i++) {
		CRC_DF = UpdateCRC(CRC_DF, CTPM_FW[i + 32]);
	}
	tp_log_info("CRC_DF = 0x%X\n", CRC_DF);
	for (i = ap_startaddr; i < ap_endaddr - 1; i++) {
		CRC_AP = UpdateCRC(CRC_AP, CTPM_FW[i + 32]);
	}
	tp_log_info("CRC_AP = 0x%x\n", CRC_AP);
	buf[0] = (uint8_t)ILITEK_TP_CMD_SET_DATA_LENGTH;	//0xc9
	buf[1] = (uint8_t)(data_len & 0xFF);
	buf[2] = (uint8_t)(data_len >> 8);
	ret = ilitek_i2c_write(buf, 3);
	if (ret < 0) {
		tp_log_err("ilitek_i2c_write\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}
	buf[0] = (uint8_t)ILITEK_TP_CMD_WRITE_ENABLE;	//0xc4
	buf[1] = 0x5A;
	buf[2] = 0xA5;
	buf[3] = 0x00;
	buf[4] = ((ap_endaddr)) & 0xFF;
	buf[5] = ((ap_endaddr) >> 8) & 0xFF;
	buf[6] = (ap_endaddr) >> 16;
	buf[7] = CRC_AP & 0xFFFF;
	buf[8] = (CRC_AP & 0xFFFF) >> 8;
	buf[9] = 0;
	ret = ilitek_i2c_write(buf, 10);
	if (ret < 0) {
		tp_log_err("ilitek_i2c_write\n");
		return ILITEK_I2C_TRANSFER_ERR;
	}
	check_busy(10);
	tp_log_info("write data to AP mode\n");
	for (i = ap_startaddr; i < ap_endaddr; i += data_len) {
		txbuf[0] = (uint8_t)ILITEK_TP_CMD_WRITE_DATA;
		for (k = 0; k < data_len; k++) {
			if(k % 0x10 == 0)
				printk("\n0x%05x:", i + k);
			txbuf[1 + k] = CTPM_FW[i + 32 + k];
			printk("%02x", txbuf[1 + k]);
		}
		if(i + data_len > ap_endaddr)
			data_len = ap_endaddr - i + 1;
		if (ilitek_i2c_write(txbuf, data_len + 1) < 0) {
			tp_log_err("ilitek_i2c_write\n");
			return ILITEK_I2C_TRANSFER_ERR;
		}
		check_busy(10);
		upgrade_status = ((i - ap_startaddr) * 100) / (ap_endaddr - ap_startaddr);
		//tp_log_info("%c ilitek ILITEK: Firmware Upgrade(AP), %02d%c.\n",0x0D,upgrade_status,'%');
	}
	for (i = 0; i < 20; i++) {
		buf[0] = (uint8_t)0xC7;
		ret = ilitek_i2c_write_and_read(buf, 1, 10, buf, 2);
		if (ret < 0) {
			tp_log_err("ilitek_i2c_write_and_read 0xc7\n");
			return ILITEK_I2C_TRANSFER_ERR;
		}
		tp_log_info("upgrade end write c7 read 0x%X, 0x%X, 0x%X, 0x%X\n", buf[0], buf[1], buf[2], buf[3]);
		if (CRC_AP != buf[1] * 256 + buf[0]) {
			tp_log_err("CRC compare error retry\n");
			//return ILITEK_TP_UPGRADE_FAIL;
		} else {
			tp_log_info("CRC compare Right\n");
			break;
		}
	}
	if (i >= 20) {
		tp_log_err("CRC compare error\n");
		return ILITEK_TP_UPGRADE_FAIL;
	}
	kfree(txbuf);
	return 0;
}

int32_t ilitek_upgrade_frimware(uint32_t df_startaddr, uint32_t df_endaddr, uint32_t df_checksum,
				 uint32_t ap_startaddr, uint32_t ap_endaddr, uint32_t ap_checksum, uint8_t *CTPM_FW)
{
	int32_t ret = 0, retry = 0;
	uint8_t buf[64] = { 0 };
Retry:
	if (retry < 2) {
		retry++;
	} else {
		tp_log_err("retry 2 times upgrade fail\n");
		return ret;
	}

	tp_log_info("upgrade firmware start	reset\n");
	ilitek_reset(300);
	if (api_protocol_set_testmode(true) < ILITEK_SUCCESS)
		goto Retry;
	check_busy(1);
	//check ic type
	if (api_protocol_set_cmd(ILITEK_TP_CMD_GET_KERNEL_VERSION, NULL, buf) < ILITEK_SUCCESS)
		goto Retry;

	ret = ilitek_changetoblmode(true);
	if (ret) {
		tp_log_err("change to bl mode err ret = %d\n", ret);
		goto Retry;
	} else {
		tp_log_info("ilitek change to bl mode ok\n");
	}
	if (api_protocol_set_cmd(ILITEK_TP_CMD_GET_PROTOCOL_VERSION, NULL, buf) < ILITEK_SUCCESS)
		goto Retry;
	tp_log_info("bl protocol version %d.%d\n", buf[0], buf[1]);
	ilitek_data->bl_ver[0] = buf[0];
	ilitek_data->bl_ver[1] = buf[1];
		ret = api_protocol_set_cmd(ILITEK_TP_CMD_GET_KERNEL_VERSION, NULL, buf);
		df_startaddr = buf[2] * 256 * 256 + buf[3] * 256 + buf[4];
		tp_log_info("df_start_addr = %x\n", (int32_t)df_startaddr);
		if (buf[0] != 0x05) {
			ilitek_data->page_number = 16;
			tp_log_info("ilitek page_number = 16, page is 512 bytes\n");
		} else {
			ilitek_data->page_number = 8;
			tp_log_info("ilitek page_number = 8, page is 256 bytes\n");
		}
		buf[0] = 0xc7;
		ret = ilitek_i2c_write_and_read(buf, 1, 10, buf, 1);
		tp_log_info("0xc7 read= %x\n", buf[0]);
	if(ilitek_data->bl_ver[0] == 0x1 && ilitek_data->bl_ver[1] == 0x8) {
		ret = ilitek_upgrade_ptl_1_8(df_startaddr, df_endaddr, ap_startaddr, ap_endaddr, CTPM_FW);
		if (ret < 0) {
			tp_log_err("ilitek_upgrade_ptl_1_8 err ret = %d\n", ret);
	}
	} else if(ilitek_data->bl_ver[0] == 0x1 && ilitek_data->bl_ver[1] == 0x7) {
		//df_startaddr = 0xF000;
		ret = ilitek_upgrade_ptl_1_7(df_startaddr, df_endaddr, ap_startaddr, ap_endaddr, CTPM_FW);
		if (ret < 0) {
			tp_log_err("ilitek_upgrade_ptl_1_7 err ret = %d\n", ret);
			//goto Retry;
		}
	} else if(ilitek_data->bl_ver[0] == 0x1 && ilitek_data->bl_ver[1] == 0x6) {
		//df_startaddr = 0x1F000;
		if (df_startaddr < df_endaddr) {
			ilitek_data->has_df = true;
		} else {
			ilitek_data->has_df = false;
		}
		ret = ilitek_upgrade_ptl_1_6(df_startaddr, df_endaddr, df_checksum, ap_startaddr, ap_endaddr, ap_checksum, CTPM_FW);
		if (ret < 0) {
			tp_log_err("ilitek_upgrade_ptl_1_6 err ret = %d\n", ret);
			//goto Retry;
		}
	} else {
		tp_log_err("not support is protocol, BL protocol:%d.%d\n", ilitek_data->bl_ver[0], ilitek_data->bl_ver[1]);
		goto Retry;
	}
	tp_log_info("upgrade firmware completed	reset\n");
	ilitek_reset(300);

	ret = ilitek_changetoblmode(false);
	if (ret) {
		tp_log_err("change to ap mode err\n");
		goto Retry;
	} else {
		tp_log_info("ilitek change to ap mode ok\n");
	}
	ret = api_protocol_init_func();
	return 0;
}

#ifdef ILITEK_UPDATE_FW
int32_t ilitek_boot_upgrade(void)
{
	int32_t ret = 0, i = 0, ap_len = 0, df_len = 0;
	uint32_t ap_startaddr = 0, df_startaddr = 0, ap_endaddr = 0, df_endaddr = 0, ap_checksum = 0, df_checksum = 0;
	uint8_t firmware_ver[8] = { 0 };
	uint8_t compare_version_count = 0;
	const struct firmware *fw;
	uint8_t *CTPM_FW_BIN = NULL;
	ap_startaddr = (CTPM_FW[0] << 16) + (CTPM_FW[1] << 8) + CTPM_FW[2];
	ap_endaddr = (CTPM_FW[3] << 16) + (CTPM_FW[4] << 8) + CTPM_FW[5];
	ap_checksum = (CTPM_FW[6] << 16) + (CTPM_FW[7] << 8) + CTPM_FW[8];
	df_startaddr = (CTPM_FW[9] << 16) + (CTPM_FW[10] << 8) + CTPM_FW[11];
	ilitek_data->upgrade_mcu_ver[0] = CTPM_FW[10];
	ilitek_data->upgrade_mcu_ver[1] = CTPM_FW[11];
	df_endaddr = (CTPM_FW[12] << 16) + (CTPM_FW[13] << 8) + CTPM_FW[14];
	df_checksum = (CTPM_FW[15] << 16) + (CTPM_FW[16] << 8) + CTPM_FW[17];
	firmware_ver[0] = CTPM_FW[18];
	firmware_ver[1] = CTPM_FW[19];
	firmware_ver[2] = CTPM_FW[20];
	firmware_ver[3] = CTPM_FW[21];
	firmware_ver[4] = CTPM_FW[22];
	firmware_ver[5] = CTPM_FW[23];
	firmware_ver[6] = CTPM_FW[24];
	firmware_ver[7] = CTPM_FW[25];
	df_len = (CTPM_FW[26] << 16) + (CTPM_FW[27] << 8) + CTPM_FW[28];
	ap_len = (CTPM_FW[29] << 16) + (CTPM_FW[30] << 8) + CTPM_FW[31];
	tp_log_info("ilitek ap_startaddr=0x%X, ap_endaddr=0x%X, ap_checksum=0x%X, ap_len = %d\n", ap_startaddr, ap_endaddr, ap_checksum, ap_len);
	tp_log_info("ilitek df_startaddr=0x%X, df_endaddr=0x%X, df_checksum=0x%X, df_len = %d\n", df_startaddr, df_endaddr, df_checksum, df_len);
	compare_version_count = 8;
	tp_log_info("compare_version_count = %d\n", compare_version_count);
	if (!(ilitek_data->force_update)) {
		for (i = 0; i < compare_version_count; i++) {
			tp_log_info("ilitek_data.firmware_ver[%d] = %d, firmware_ver[%d] = %d\n", i, ilitek_data->firmware_ver[i], i, firmware_ver[i]);
			if (firmware_ver[i] < ilitek_data->firmware_ver[i]) {
				i = compare_version_count;
				break;
			}
			if (firmware_ver[i] > ilitek_data->firmware_ver[i]) {
				break;
			}
		}
		if (i >= compare_version_count) {
			tp_log_info("firmware version is older so not upgrade\n");
			return 1;
		}
	}
	ret = -1;
	if ((ilitek_data->upgrade_mcu_ver[0] != 0 || ilitek_data->upgrade_mcu_ver[1] != 0)
		&& (ilitek_data->mcu_ver[0] != ilitek_data->upgrade_mcu_ver[0] || ilitek_data->mcu_ver[1] != ilitek_data->upgrade_mcu_ver[1])) {
		tp_log_info("upgrade file mismatch!!! ic is ILI%02X%02X, upgrade file is ILI%02X%02X\n", ilitek_data->mcu_ver[1],
				ilitek_data->mcu_ver[0], ilitek_data->upgrade_mcu_ver[1], ilitek_data->upgrade_mcu_ver[0]);
	} else if (ilitek_data->upgrade_mcu_ver[0] == 0 && ilitek_data->upgrade_mcu_ver[1] == 0 && ilitek_data->mcu_ver[0] != 0x03
			&& ilitek_data->mcu_ver[0] != 0x09) {
		tp_log_info("upgrade file  mismatch!!! ic is ILI%02X%02X, upgrade file is maybe ILI230X\n", ilitek_data->mcu_ver[1],
				ilitek_data->mcu_ver[0]);
	} else {
		ret = ilitek_upgrade_frimware(df_startaddr, df_endaddr, df_checksum, ap_startaddr, ap_endaddr, ap_checksum, CTPM_FW);
	}
	if (ret < 0) {
		tp_log_err("upgrade fail\n");
		return ret;
	}
	if (ILITEK_UPGRADE_WITH_BIN) {
		if (CTPM_FW_BIN) {
			vfree(CTPM_FW_BIN);
			CTPM_FW_BIN = NULL;
		}
	}
	return 0;
}
#endif
