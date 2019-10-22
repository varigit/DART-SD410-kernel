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
#ifndef _ILITEK_COMMON_H_
#define _ILITEK_COMMON_H_
/* Includes of headers ------------------------------------------------------*/
#include <linux/sched.h>
/* Extern define ------------------------------------------------------------*/
//driver information
#define DERVER_VERSION_MAJOR 										5
#define DERVER_VERSION_MINOR 										6
#define CUSTOMER_ID 												0
#define MODULE_ID													0
#define PLATFORM_ID													0
#define PLATFORM_MODULE												0
#define ENGINEER_ID													0
//
#define ILITEK_TP_MODE_APPLICATION									0x5A
#define ILITEK_TP_MODE_BOOTLOADER									0x55

#define ILITEK_TP_CMD_WRITE_ENABLE									0xC4
#define ILITEK_TP_CMD_WRITE_DATA									0xC3
#define ILITEK_TP_CMD_SLEEP											0x30
#define ILITEK_TP_CMD_GET_TOUCH_INFORMATION							0x10
//error code
#define ILITEK_TP_UPGRADE_FAIL										(-5)
#define ILITEK_I2C_TRANSFER_ERR										(-4)
#define ILITEK_TP_CHANGETOBL_ERR									(-3)
#define ILITEK_TP_CHANGETOAP_ERR									(-2)
#define ILITEK_FAIL                                                 (-1)
#define ILITEK_SUCCESS                                              (0)
#define ILITEK_ERR_LOG_LEVEL 										(1)
#define ILITEK_INFO_LOG_LEVEL 										(3)
#define ILITEK_DEBUG_LOG_LEVEL 										(4)
#define ILITEK_DEFAULT_LOG_LEVEL									(3)

#define ILITEK_KEYINFO_HEADER                                       4
#define ILITEK_KEYINFO_FORMAT_LENGTH                                5
#define ILITEK_KEYINFO_FIRST_PACKET                                 29
#define ILITEK_KEYINFO_OTHER_PACKET                                 25
#define ILITEK_KEYINFO_FORMAT_ID                                    0
#define ILITEK_KEYINFO_FORMAT_X_MSB                                 1
#define ILITEK_KEYINFO_FORMAT_X_LSB                                 2
#define ILITEK_KEYINFO_FORMAT_Y_MSB                                 3
#define ILITEK_KEYINFO_FORMAT_Y_LSB                                 4

#define ILITEK_IOCTL_MAX_TRANSFER									5000
#define ILITEK_HEX_UPGRADE_SIZE										256 * 1024 + 32
#define debug_level(level, fmt, arg...) do {\
	if (level <= ilitek_data->ilitek_log_level_value) {\
		if (level == ILITEK_ERR_LOG_LEVEL) {\
			printk(" %s ERR  line = %d %s : "fmt, "ILITEK", __LINE__, __func__, ##arg);\
		} else if (level == ILITEK_INFO_LOG_LEVEL) {\
			printk(" %s INFO line = %d %s : "fmt, "ILITEK", __LINE__, __func__, ##arg);\
		} else if (level == ILITEK_DEBUG_LOG_LEVEL) {\
			printk(" %s DEBUG line = %d %s : "fmt, "ILITEK", __LINE__, __func__, ##arg);\
		} \
	} \
} while (0)

#define tp_log_err(fmt, arg...) debug_level(ILITEK_ERR_LOG_LEVEL, fmt, ##arg)
#define tp_log_info(fmt, arg...) debug_level(ILITEK_INFO_LOG_LEVEL, fmt, ##arg)
#define tp_log_debug(fmt, arg...) debug_level(ILITEK_DEBUG_LOG_LEVEL, fmt, ##arg)
/* Extern typedef -----------------------------------------------------------*/
struct ilitek_protocol_info {
    uint32_t ver;
    unsigned char ver_major;
    unsigned char ver_mid;
    unsigned char ver_minor;
    uint32_t cmd_count;
};

struct ilitek_key_info {
	int32_t id;
	int32_t x;
	int32_t y;
	int32_t status;
	int32_t flag;
};

struct ilitek_ts_data {
	int (*process_and_report)(void);
    int ilitek_log_level_value;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct regulator *vdd;
	struct regulator *vdd_i2c;
	struct regulator *vcc_io;
	int irq_gpio;
	int reset_gpio;
	bool system_suspend;
	unsigned char firmware_ver[8];
	unsigned char mcu_ver[8];
	unsigned char bl_ver[4];
	int upgrade_FW_info_addr;
	unsigned char upgrade_mcu_ver[4];
	int protocol_ver;
	int tp_max_x;
	int tp_max_y;
	int tp_min_x;
	int tp_min_y;
	int screen_max_x;
	int screen_max_y;
	int screen_min_x;
	int screen_min_y;
	int max_tp;
	int max_btn;
	int x_ch;
	int y_ch;
	int keycount;
	int key_xlen;
	int key_ylen;
	struct ilitek_key_info keyinfo[10];
	bool irq_status;
	bool irq_trigger;
	struct task_struct *irq_thread;
	spinlock_t irq_lock;
	bool is_touched;
	bool touch_key_hold_press;
	int touch_flag[10];
	int slave_count;

#if defined(CONFIG_FB) || defined(CONFIG_DRM)
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
//#ifdef ILITEK_UPDATE_FW
	bool force_update;
	bool has_df;
	int page_number;
	struct task_struct *update_thread;
//#endif
	bool firmware_updating;
	bool operation_protection;
	bool unhandle_irq;
#ifdef ILITEK_GESTURE
	bool enable_gesture;
#endif

#ifdef ILITEK_ESD_PROTECTION
	struct workqueue_struct *esd_wq;
	struct delayed_work esd_work;
	bool esd_check;
	unsigned long esd_delay;
#endif

	struct kobject *ilitek_func_kobj;
	struct mutex ilitek_mutex;
#ifdef ILITEK_TUNING_NODE
	bool debug_node_open;
	int debug_data_frame;
	wait_queue_head_t inq;
	unsigned char debug_buf[1024][64];
	struct mutex ilitek_debug_mutex;
#endif
	bool ilitek_repeat_start;
	bool ilitek_exit_report;
	uint32_t irq_trigger_count;
	uint32_t irq_handle_count;
    struct ilitek_protocol_info ptl;
};
/* Extern macro -------------------------------------------------------------*/
#define CEIL(n, d) ((n % d) ? (n / d) + 1 : (n / d ))
/* Extern variables ---------------------------------------------------------*/
extern uint32_t irq_handle_count;
extern char ilitek_driver_information[];
extern struct ilitek_ts_data *ilitek_data;
/* Extern function prototypes -----------------------------------------------*/
/* Extern functions ---------------------------------------------------------*/
extern void ilitek_resume(void);
extern void ilitek_suspend(void);

extern int ilitek_main_probe(struct ilitek_ts_data *ilitek_data);
extern int ilitek_main_remove(struct ilitek_ts_data *ilitek_data);
extern void ilitek_reset(int delay);
extern int ilitek_poll_int(void);
extern int ilitek_i2c_write(uint8_t *cmd, int length);
extern int ilitek_i2c_read(uint8_t *data, int length);
extern int ilitek_i2c_write_and_read(uint8_t *cmd,
			int write_len, int delay, uint8_t *data, int read_len);

extern void ilitek_irq_enable(void);
extern void ilitek_irq_disable(void);
extern int ilitek_read_tp_info(void);

#ifdef ILITEK_UPDATE_FW
extern int ilitek_boot_upgrade(void);
#endif
extern int ilitek_upgrade_frimware(uint32_t df_startaddr, uint32_t df_endaddr, uint32_t df_checksum,
			uint32_t ap_startaddr, uint32_t ap_endaddr, uint32_t ap_checksum, unsigned char *CTPM_FW);

#ifdef ILITEK_TUNING_MESSAGE
extern bool ilitek_debug_flag;
#endif
#ifdef ILITEK_TOOL
extern int ilitek_create_tool_node(void);
extern int ilitek_remove_tool_node(void);
#endif
#endif