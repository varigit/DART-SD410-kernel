/*
 * Copyright (C) 2006-2017 ILITEK TECHNOLOGY CORP.
 *
 * Description: ILITEK I2C touchscreen driver for linux platform.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Dicky Chiang
 * Maintain: Luca Hsu, Tigers Huang
 */

#ifndef __MP_COMMON_H__
#define __MP_COMMON_H__

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/kobject.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include "ilitek_ts.h"


#define mp_err(level,fmt, arg...) do {\
		if (level & print_debug_level)	\
		    pr_err("ILITEK_MP_ERROR %d: (%s, %d): " fmt,level,__func__, __LINE__,##arg);	\
	} while (0)
	
#define mp_info(level,fmt, arg...) do {\
		if (level & print_debug_level)	\
		    pr_info("ILITEK_MP_INFO%d: (%s, %d): " fmt,level,__func__, __LINE__,##arg);	\
	} while (0)	
	
#define mp_debug(level,fmt, arg...) do {\
		if (level & print_debug_level)	\
		    pr_info(fmt,##arg);	\
	} while (0)	

#define ERR_ALLOC_MEM(X)	((IS_ERR(X) || X == NULL) ? 1 : 0)

#define TEST_ITEM_NUM 8
#define FILENAME_MAX 4096
#define MAX_MUTUAL_NUM 1904
#define MAX_CHANNEL_DRV  30
#define MAX_CHANNEL_SEN  20
#define MAX_CHANNEL_NUM_28XX	60
#define MAX_CHANNEL_NUM_30XX    49
#define MAX_SUBFRAMES_30XX      28
#define MAX_AFENUMs_30XX 2
#define NULL_DATA -3240
#define PIN_NO_ERROR 0xFFFF
#define IIR_MAX 32600
#define PIN_UN_USE 0xABCD

//MPTest Result
#define ITO_NO_TEST                                   0
#define ITO_TEST_OK                                   1
#define ITO_TEST_FAIL                               -1
#define ITO_TEST_GET_TP_TYPE_ERROR     3
#define ITO_TEST_UNDEFINED_ERROR        4
#define ITO_TEST_PROCESSING		      5

#define INI_PATH "/sdcard/autoSettings.ini"

typedef enum {
    MUTUAL_MODE = 0x5705,
    MUTUAL_SINE = 0x5706,
    MUTUAL_KEY = 0x6734,         // latter FW v1007.
    MUTUAL_SINE_KEY = 0x6733,    // latter FW v1007.
    SELF = 0x6278,
    WATERPROOF = 0x7992,
    MUTUAL_SINGLE_DRIVE = 0x0158,
    SINE_PHASE = 0xECFA,
    SET_PHASE,
    DEEP_STANDBY = 0x6179,
    GET_BG_SUM = 0x7912,
} ilitek_camaro_test_fw_mode_e;

typedef enum {
    ONE_DAC_ENABLE = 0,
    TWO_DAC_ENABLE = 1,
} ito_test_dac_status;

typedef enum {
    SINGLE = 0,
    MULTI = 1,
} ito_test_screen_type;

typedef enum {
    eDAC_0,
    eDAC_1,
} ito_test_dac;

typedef struct {
    u16 X;
    u16 Y;
} mutual_mapping_t;

typedef struct {
    char * s_support_iic;
    int bm_dk_Verify;
    int b_current_test;
    int b_chip_verify;
    int b_fw_update;
    int b_fw_test;
    int b_open_test;
    int b_short_test;
    int b_wp_test;
    int b_function_test;
    int b_auto_start;
    int b_auto_mation;
    int b_trigger_mode;
    int b_ts_mode;
    int b_ts_enable;
    int b_phase_k_test;
} mutual_ui_config_t;

typedef struct {
    u16 persent_dc_va_range;
    u16 persent_dc_va_ratio;
    
    u16 persent_dc_border_ratio;
    u16 persent_dc_va_range_up;
    u16 persent_dc_va_ratio_up;
    
    u16 persent_dg_range;
    u16 persent_dg_ratio;
    u16 persent_dg_range_up;
    u16 persent_dg_ratio_up;
    u16 persent_water_dg_range;
} mutual_toast_t;

typedef struct {
    u16 num_key;
    u16 num_keyLine;
    u16 num_dummy;
    u16 num_triangle;
    u16 key_drv;
    u16 key_ch;
    u16 key_drv_o;
    char *key_type;
    int thrs_short;
    int thrs_ic_pin;
    int thrs_open;
    int thrs_water;
    
    u16 num_sen;
    u16 num_drv;
    u16 num_gr;
    mutual_mapping_t *mapping;
} mutual_sensor_t;

typedef struct {
    mutual_ui_config_t ui_config;
    int log_result;
    int log_fw_result;
    
    int Enable;
    
    char * ana_version;
    char * project_name;
    char * binname;
    char * version_fw;
    u16 slave_i2c_id;
    char * station_now;
    char * inipassword;
    u16 mutual_key;
    u16 pattern_type;
    u16 Pattern_model;
    
    int crc_check;
    
    mutual_sensor_t sensor_info;
    mutual_toast_t toast_info;
    int fpc_threshold;
    int key_dc_threshold;
    int key_timer;
    int open_test_csub;
    int open_test_cfb;
    int open_mode;
    int open_fixed_carrier;
    int open_fixed_carrier1;
    int open_test_chargepump;
    int open_test_mode;
    int short_test_mode;
    int inverter_mode;
    int current_threshold;
    int current_threshold_powerdown;
    
    int open_charge;
    int open_dump;
    int short_charge;
    int short_dump1;
    int short_dump2;
    int water_charge;
    int water_dump;
    
    int * key_sen;
    
    int * goldensample_ch_0_max_avg;
    int * goldensample_ch_0_max;
    int * goldensample_ch_0;
    int * goldensample_ch_0_min;
    int * goldensample_ch_0_min_avg;
    
    int * phase_golden_max;
    int * phase_golden;
    int * phase_golden_min;
    
    int * phase_water_golden_max;
    int * phase_water_golden;
    int * phase_water_golden_min;
    
    u16 * pad2_sense;
    u16 * pad2_drive;
    u16 * pad2_gr;
    
    u16 * phase_freq;
    u16 freq_num;
    u16 phase_time;
    u16 band_num;
    u16 * pgd;
    u16 * water_pgd;
    u16 * water_sense;
    u16 * water_drive;
    u16 charge_pump;
    u16 raw_type;
    u16 noise_thd;
    u16 sub_frame;
    u16 afe_num;
    u16 phase_sen_num;
    u16 * phase_sense;
    u16 water_sen_num;
    u16 water_drv_num;
    int update_bin;
    int force_phaseK;
    int update_info;
    int log_phasek;
    int border_drive_phase;
    int sw_calibration;
    u8 phase_version;
    u8 mapping_version;
    int deep_standby;
    int deep_standby_timeout;
    int open_key_setting_by_fw;
    u8 post_idle;
    u8 self_sample_hi;
    u8 self_sample_lo;
} mutual_mp_test_t;

typedef struct {
    int open_result;
    int short_result;
    int water_proof_result;
    
    int ratio_avg_max;
    int ratio_avg_min;
    int border_ratio_avg_max;
    int border_ratio_avg_min;
    
    int * open_result_data;
    int * open_fail_channel;
    int * open_ratio_fail_channel;
    int * short_result_data;
    int * short_r_data;
    int * p_ic_pin_short_result_data;
    int * p_ic_pin_short_r_data;
    int * p_ic_pin_channel;
    int * p_short_fail_channel;
    int * p_ic_pin_short_fail_channel;
    int * p_water_proof_result_data;
    int * p_water_proof_fail_channel;
    
    int *p_check_fail;
    int *p_result_delta_c;
    int *p_golden_ch_max_avg;
    int *p_golden_ch_min_avg;
    int *p_golden_ch_max;
    int *p_golden_ch_min;
    int *p_golden_ch;
    char *map_tbl_sec;
} mutual_mp_test_result_t;

enum new_flow_test_type {
    TYPE_RESET = 0x0,
    TYPE_OPEN = 0x1,
    TYPE_SHORT = 0x2,
    TYPE_BootPalm = 0x3,
    TYPE_KPhase = 0x4,
    TYPE_SELF = 0x5,
    TYPE_DQPhase = 0x6,
};

struct mp_main_func {
    int chip_type;
    int fout_data_addr;
    int max_channel_num;
    int sense_len;
    int drive_len;
    int gr_len;
    int (*check_mp_switch)(void);
    int (*enter_mp_mode)(void);
    int (*open_judge)(int *deltac_data, int size);
    int (*short_judge)(int *deltac_data);
};

extern mutual_mp_test_t *mp_test_data;
extern mutual_mp_test_result_t *mp_test_result;
extern struct mp_main_func *p_mp_func;

extern int ilitek_mp_new_flow_main(int item);
extern int ilitek_mp_start_test(int chip_type, char *p_file_path);

extern int ilitek_mp_parse(char *path);
extern int ilitek_ms_get_ini_data(char *section, char *ItemName, char *returnValue);
extern int ilitek_ms_ini_split_u8_array(char * key, u8 * pBuf);
extern int ilitek_ms_ini_split_int_array(char * key, int * pBuf);
extern int ilitek_ms_ini_split_golden(int *pBuf, int line);
extern int ilitek_ms_ini_split_u16_array(char * key, u16 * pBuf);
extern int ilitek_ms_atoi(char *nptr);


extern int ilitek_camaro_check_mp_switch(void);
extern int ilitek_camaro_enter_mp_mode(void);
extern int ilitek_camaro_short_judge(int *deltac_data);
extern int ilitek_camaro_open_judge(int *deltac_data, int deltac_size);

extern void ilitek_camaro_db_bus_read_dq_mem_start(void);
extern void ilitek_camaro_db_bus_read_dq_mem_end(void);

extern void stop_mcu(void);
extern void start_mcu(void);


extern void ilitek_mp_save_result(int result);

static inline void mp_kfree(void *mem)
{
    if(mem != NULL) {
        kfree(mem);
        mem = NULL;
    }
}

static inline u8 check_thrs(s32 nValue, s32 nMax, s32 nMin)
{
    return ((nValue <= nMax && nValue >= nMin) ? 1 : 0);
}

#endif
