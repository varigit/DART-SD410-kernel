/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Luca Hsu <luca_hsu@ilitek.com>
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
#ifndef __ILITEK_PROTOCOL_H__
#define __ILITEK_PROTOCOL_H__

/* Private typedef -----------------------------------------------------------*/
typedef int protocol_func(uint8_t *inbuf, uint8_t *outbuf);

typedef struct
{
    uint8_t cmd;
    protocol_func *func;
    uint8_t *name;
} PROTOCOL_MAP;
/* Private define ------------------------------------------------------------*/
//command spec
#define ILITEK_TP_CMD_GET_TP_RESOLUTION								0x20
#define ILITEK_TP_CMD_GET_SCREEN_RESOLUTION							0x21
#define ILITEK_TP_CMD_GET_KEY_INFORMATION							0x22
#define ILITEK_TP_CMD_GET_FIRMWARE_VERSION							0x40
#define ILITEK_TP_CMD_GET_PROTOCOL_VERSION							0x42
#define ILITEK_TP_CMD_GET_KERNEL_VERSION							0x61
#define ILITEK_TP_CMD_SET_FUNC_MODE     							0x68
#define ILITEK_TP_CMD_GET_SYSTEM_BUSY   							0x80
#define ILITEK_TP_CMD_READ_MODE										0xC0
#define ILITEK_TP_CMD_SET_APMODE									0xC1
#define ILITEK_TP_CMD_SET_BLMODE									0xC2
#define ILITEK_TP_CMD_SET_DATA_LENGTH								0xC9

#define ILITEK_TP_CMD_CONTROL_TESTMODE								0xF2

#define ILITEK_TP_SYSTEM_READY  									0x50
#define ILITEK_TP_NORMAL_MODE                                       0x0
#define ILITEK_TP_GLOVE_MODE                                        0x1
#define PROTOCOL_CMD_NUM_V3		(sizeof(ptl_func_map_v3)/sizeof(PROTOCOL_MAP))
#define PROTOCOL_CMD_NUM_V6		(sizeof(ptl_func_map_v6)/sizeof(PROTOCOL_MAP))
/* Extern variables ----------------------------------------------------------*/
extern PROTOCOL_MAP *ptl_cb_func;
/* Extern functions ---------------------------------------------------------*/
extern int api_protocol_get_mcuver(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_check_mode(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_get_fwver(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_get_ptl(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_get_sc_resolution(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_get_key_info(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_get_tp_resolution(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_init_func(void);
extern int api_protocol_set_cmd(uint8_t cmd, uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_write_enable(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_set_apmode(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_set_blmode(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_system_busy(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_control_tsmode(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_set_testmode(bool testmode);
extern int api_protocol_get_v6_tp_resolution(uint8_t *inbuf, uint8_t *outbuf);
extern int api_protocol_get_funcmode(void);
extern int api_protocol_set_funcmode(int mode);
extern int ilitek_read_data_and_report_3XX(void);
#endif