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

#include "mp_common.h"

#define RUN_OPEN_TEST	0
#define RUN_SHORT_TEST	1

u32 g_in_mp_test;

mutual_mp_test_t *mp_test_data;
mutual_mp_test_result_t *mp_test_result;
struct mp_main_func *p_mp_func;

int ana_count = 0;
u8 ana_ver[100] = {0};
int mapping_barker_ini = 0;

static void mp_calc_golden_range(int *goldenTbl, u16 weight, u16 weight_up, int *maxTbl, int *minTbl, int length)
{
    int i, value = 0, value_up = 0;
    
    for (i = 0; i < length; i++) {
        value = (int)weight * abs(goldenTbl[i]) / 100;
        value_up = (int)weight_up * abs(goldenTbl[i]) / 100;
        maxTbl[i] = goldenTbl[i] + value + value_up;
        minTbl[i] = goldenTbl[i] - value;
    }
}


static int mp_load_ini(char * p_file_path)
{
    int ret = 0, nSize = 0;
    char *token = NULL, str[512] = {0};
    long s_to_long = 0;
    mp_info(PRINT_MPTEST, "*** %s() ***\n", __func__);
    
    if(ilitek_mp_parse(p_file_path) < 0) {
        mp_err(PRINT_MPTEST,"Failed to parse file = %s\n", p_file_path);
        return -1;
    }
    
    mp_info(PRINT_MPTEST, "Parsed %s successfully!\n", p_file_path);
    mp_test_data = kcalloc(1, sizeof(*mp_test_data), GFP_KERNEL);
    mp_test_result = kcalloc(1, sizeof(*mp_test_result), GFP_KERNEL);
    
    if(ERR_ALLOC_MEM(mp_test_result) || ERR_ALLOC_MEM(mp_test_data)) {
        pr_err("Failed to allocate mp_test mem \n");
        return -1;
    }
    
    token = kmalloc(100, GFP_KERNEL);
    ilitek_ms_get_ini_data("INFOMATION", "MAPPING_BARKER_INI", str);
    mapping_barker_ini = ilitek_ms_atoi(str);
    mp_info(PRINT_MPTEST, " mapping_barker_ini = %d \n", mapping_barker_ini);
    ilitek_ms_get_ini_data("UI_CONFIG", "OpenTest", str);
    mp_test_data->ui_config.b_open_test = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("UI_CONFIG", "ShortTest", str);
    mp_test_data->ui_config.b_short_test = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("UI_CONFIG", "WpTest", str);
    mp_test_data->ui_config.b_wp_test = ilitek_ms_atoi(str);
    mp_test_data->ana_version = kmalloc(FILENAME_MAX * sizeof(char), GFP_KERNEL);
    
    if(ERR_ALLOC_MEM(mp_test_data->ana_version)) {
        pr_err("Failed to allocate Ana mem \n");
        return -1;
    }
    
    ilitek_ms_get_ini_data("UI_CONFIG", "ANAGEN_VER", str);
    strcpy(mp_test_data->ana_version, str);
    ana_count = ilitek_ms_ini_split_u8_array(mp_test_data->ana_version, ana_ver);
    mp_info(PRINT_MPTEST, "Ana count = %d , mem = %p\n", ana_count, mp_test_data->ana_version);
    ilitek_ms_get_ini_data("SENSOR", "DrvNum", str);
    mp_test_data->sensor_info.num_drv = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("SENSOR", "SenNum", str);
    mp_test_data->sensor_info.num_sen = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("SENSOR", "KeyNum", str);
    mp_test_data->sensor_info.num_key = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("SENSOR", "KeyLine", str);
    mp_test_data->sensor_info.num_keyLine = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("SENSOR", "GrNum", str);
    mp_test_data->sensor_info.num_gr = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("OPEN_TEST_N", "CSUB_REF", str);
    mp_test_data->open_test_csub = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("OPEN_TEST_N", "CFB_REF", str);
    mp_test_data->open_test_cfb = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("OPEN_TEST_N", "OPEN_MODE", str);
    mp_test_data->open_mode = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("OPEN_TEST_N", "FIXED_CARRIER", str);
    mp_test_data->open_fixed_carrier = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("OPEN_TEST_N", "FIXED_CARRIER1", str);
    mp_test_data->open_fixed_carrier1 = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("OPEN_TEST_N", "CHARGE_PUMP", str);
    mp_test_data->open_test_chargepump = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("INFOMATION", "MutualKey", str);
    mp_test_data->mutual_key = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("INFOMATION", "Pattern_type", str);
    mp_test_data->pattern_type = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("INFOMATION", "1T2R_MODEL", str);
    mp_test_data->Pattern_model = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("RULES", "DC_Range", str);
    mp_test_data->toast_info.persent_dc_va_range = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("RULES", "DC_Range_UP", str);
    mp_test_data->toast_info.persent_dc_va_range_up = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("RULES", "DC_Ratio", str);
    mp_test_data->toast_info.persent_dc_va_ratio = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("RULES", "DC_Ratio_UP", str);
    mp_test_data->toast_info.persent_dc_va_ratio_up = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("RULES", "DC_Border_Ratio", str);
    mp_test_data->toast_info.persent_dc_border_ratio = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("RULES", "opentestmode", str);
    mp_test_data->open_test_mode = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("RULES", "shorttestmode", str);
    mp_test_data->short_test_mode = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("BASIC", "DEEP_STANDBY", str);
    mp_test_data->deep_standby = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("BASIC", "DEEP_STANDBY_TIMEOUT", str);
    mp_test_data->deep_standby_timeout = ilitek_ms_atoi(str);
    
    if ((mp_test_data->mutual_key == 1) && (mp_test_data->mutual_key == 2)) {
        ilitek_ms_get_ini_data("SENSOR", "KEY_CH", str);
        mp_test_data->sensor_info.key_ch = ilitek_ms_atoi(str);
    }
    
    ilitek_ms_get_ini_data("OPEN_TEST_N", "KEY_SETTING_BY_FW", str);
    mp_test_data->open_key_setting_by_fw = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("OPEN_TEST_N", "INVERT_MODE", str);
    mp_test_data->inverter_mode = ilitek_ms_atoi(str);
    ilitek_ms_get_ini_data("CDTIME", "OPEN_CHARGE", str);
    mp_test_data->open_charge = ilitek_ms_atoi(str);
    strcpy(token, str);
    ret = kstrtol((const char *)token, 0, &s_to_long);
    
    if(ret == 0)
        mp_test_data->open_charge = s_to_long;
        
    ilitek_ms_get_ini_data("CDTIME", "OPEN_DUMP", str);
    mp_test_data->open_dump = ilitek_ms_atoi(str);
    strcpy(token, str);
    ret = kstrtol((const char *)token, 0, &s_to_long);
    
    if(ret == 0)
        mp_test_data->open_dump = s_to_long;
        
    ilitek_ms_get_ini_data("CDTIME", "SHORT_Charge", str);
    mp_test_data->short_charge = ilitek_ms_atoi(str);
    strcpy(token, str);
    ret = kstrtol((const char *)token, 0, &s_to_long);
    
    if(ret == 0)
        mp_test_data->short_charge = s_to_long;
        
    ilitek_ms_get_ini_data("CDTIME", "SHORT_Dump1", str);
    mp_test_data->short_dump1 = ilitek_ms_atoi(str);
    strcpy(token, str);
    ret = kstrtol((const char *)token, 0, &s_to_long);
    
    if(ret == 0)
        mp_test_data->short_dump1 = s_to_long;
        
    mp_info(PRINT_MPTEST, "ANAGEN_VER:    [%s]\n", mp_test_data->ana_version);
    mp_info(PRINT_MPTEST, "OpenTest:      [%d]\n", mp_test_data->ui_config.b_open_test);
    mp_info(PRINT_MPTEST, "ShortTest:      [%d]\n", mp_test_data->ui_config.b_short_test);
    mp_info(PRINT_MPTEST, "WPTest:      [%d]\n", mp_test_data->ui_config.b_wp_test);
    mp_info(PRINT_MPTEST, "DrvNum:      [%d]\n", mp_test_data->sensor_info.num_drv);
    mp_info(PRINT_MPTEST, "SenNum:      [%d]\n", mp_test_data->sensor_info.num_sen);
    mp_info(PRINT_MPTEST, "KeyNum:      [%d]\n", mp_test_data->sensor_info.num_key);
    mp_info(PRINT_MPTEST, "KeyLine:      [%d]\n", mp_test_data->sensor_info.num_keyLine);
    mp_info(PRINT_MPTEST, "DEEP_STANDBY = [%d] \n", mp_test_data->deep_standby);
    mp_info(PRINT_MPTEST, "GrNum:      [%d]\n", mp_test_data->sensor_info.num_gr);
    mp_info(PRINT_MPTEST, "CSUB_REF:      [%d]\n", mp_test_data->open_test_csub);
    mp_info(PRINT_MPTEST, "CFB_REF:      [%d]\n", mp_test_data->open_test_cfb);
    mp_info(PRINT_MPTEST, "OPEN_MODE:      [%d]\n", mp_test_data->open_mode);
    mp_info(PRINT_MPTEST, "FIXED_CARRIER:      [%d]\n", mp_test_data->open_fixed_carrier);
    mp_info(PRINT_MPTEST, "FIXED_CARRIER1:      [%d]\n", mp_test_data->open_fixed_carrier1);
    mp_info(PRINT_MPTEST, "CHARGE_PUMP:      [%d]\n", mp_test_data->open_test_chargepump);
    mp_info(PRINT_MPTEST, "MutualKey:      [%d]\n", mp_test_data->mutual_key);
    mp_info(PRINT_MPTEST, "KEY_CH:      [%d]\n", mp_test_data->sensor_info.key_ch);
    mp_info(PRINT_MPTEST, "Pattern_type:      [%d]\n", mp_test_data->pattern_type);
    mp_info(PRINT_MPTEST, "Pattern_model:      [%d]\n", mp_test_data->Pattern_model);
    mp_info(PRINT_MPTEST, "DC_Range:      [%d]\n", mp_test_data->toast_info.persent_dc_va_range);
    mp_info(PRINT_MPTEST, "DC_Ratio:      [%d]\n", mp_test_data->toast_info.persent_dc_va_ratio);
    mp_info(PRINT_MPTEST, "DC_Range_up:      [%d]\n", mp_test_data->toast_info.persent_dc_va_range_up);
    mp_info(PRINT_MPTEST, "DC_Ratio_up:      [%d]\n", mp_test_data->toast_info.persent_dc_va_ratio_up);
    mp_info(PRINT_MPTEST, "KEY_SETTING_BY_FW:      [%d]\n", mp_test_data->open_key_setting_by_fw);
    mp_info(PRINT_MPTEST, "INVERT_MODE:      [%d]\n", mp_test_data->inverter_mode);
    mp_info(PRINT_MPTEST, "DEEP_STANDBY_TIMEOUT:      [%d]\n", mp_test_data->deep_standby_timeout);
    mp_info(PRINT_MPTEST, "OPEN_CHARGE:      [%d]\n", mp_test_data->open_charge);
    mp_info(PRINT_MPTEST, "OPEN_DUMP:      [%d]\n", mp_test_data->open_dump);
    mp_info(PRINT_MPTEST, "SHORT_Charge:      [%d]\n", mp_test_data->short_charge);
    mp_info(PRINT_MPTEST, "SHORT_Dump1:      [%d]\n", mp_test_data->short_dump1);
    
    if(mp_test_data->sensor_info.num_key > 0) {
        ilitek_ms_get_ini_data("SENSOR", "Key_Drv_o", str);
        mp_test_data->sensor_info.key_drv_o = ilitek_ms_atoi(str);
        ilitek_ms_get_ini_data("SENSOR", "KEYSEN", str);
        mp_test_data->key_sen = kcalloc(mp_test_data->sensor_info.num_key, sizeof(int), GFP_KERNEL);
        
        if(ERR_ALLOC_MEM(mp_test_data->key_sen)) {
            mp_err(PRINT_MPTEST,"Failed to allocate mp_test_data->key_sen mem\n");
            return -1;
        }
        
        ilitek_ms_ini_split_int_array(str, mp_test_data->key_sen);
        ilitek_ms_get_ini_data("SENSOR", "KEY_TYPE", str);
        mp_test_data->sensor_info.key_type = kmalloc(64 * sizeof(char), GFP_KERNEL);
        
        if(ERR_ALLOC_MEM(mp_test_data->sensor_info.key_type)) {
            mp_err(PRINT_MPTEST,"Failed to allocate mp_test_data->sensor_info.key_type mem\n");
            return -1;
        }
        
        strcpy(mp_test_data->sensor_info.key_type, str);
    }
    
    mp_test_data->ui_config.s_support_iic = kmalloc(FILENAME_MAX * sizeof(char), GFP_KERNEL);
    
    if(ERR_ALLOC_MEM(mp_test_data->ui_config.s_support_iic)) {
        mp_err(PRINT_MPTEST,"Failed to allocate mp_test_data->ui_config.s_support_iic mem\n");
        return -1;
    }
    
    memset(mp_test_data->ui_config.s_support_iic, 0, FILENAME_MAX * sizeof(char));
    
    if(ilitek_ms_get_ini_data("UI_CONFIG", "SupportIC", str) != 0)
        strcpy(mp_test_data->ui_config.s_support_iic, str);
        
    mp_info(PRINT_MPTEST, "SupportIC:      [%s]\n", mp_test_data->ui_config.s_support_iic);
    mp_test_data->project_name = (char *)kmalloc(FILENAME_MAX * sizeof(char), GFP_KERNEL);
    
    if(ERR_ALLOC_MEM(mp_test_data->ui_config.s_support_iic)) {
        mp_err(PRINT_MPTEST,"Failed to allocate mp_test_data->project_name mem\n");
        return -1;
    }
    
    memset(mp_test_data->project_name, 0, FILENAME_MAX * sizeof(char));
    
    if(ilitek_ms_get_ini_data("INFOMATION", "PROJECT", str) != 0)
        strcpy(mp_test_data->project_name, str);
        
    mp_info(PRINT_MPTEST, "PROJECT:      [%s]\n", mp_test_data->project_name);
    mp_test_data->goldensample_ch_0 = (int *)kcalloc(MAX_MUTUAL_NUM, sizeof(int), GFP_KERNEL);
    
    if(ERR_ALLOC_MEM(mp_test_data->goldensample_ch_0)) {
        mp_err(PRINT_MPTEST,"Failed to allocate mp_test_data->goldensample_ch_0 mem\n");
        return -1;
    }
    
    nSize = ilitek_ms_ini_split_golden(mp_test_data->goldensample_ch_0, mp_test_data->sensor_info.num_sen);
    mp_info(PRINT_MPTEST, "The number of Golden line = %d\n", nSize);
    mp_test_data->goldensample_ch_0_max = kcalloc(MAX_MUTUAL_NUM, sizeof(int), GFP_KERNEL);
    mp_test_data->goldensample_ch_0_max_avg = kcalloc(MAX_MUTUAL_NUM, sizeof(int), GFP_KERNEL);
    mp_test_data->goldensample_ch_0_min = kcalloc(MAX_MUTUAL_NUM, sizeof(int), GFP_KERNEL);
    mp_test_data->goldensample_ch_0_min_avg = kcalloc(MAX_MUTUAL_NUM, sizeof(int), GFP_KERNEL);
    
    if(ERR_ALLOC_MEM(mp_test_data->goldensample_ch_0_max) || ERR_ALLOC_MEM(mp_test_data->goldensample_ch_0_max_avg) ||
       ERR_ALLOC_MEM(mp_test_data->goldensample_ch_0_min) || ERR_ALLOC_MEM(mp_test_data->goldensample_ch_0_min_avg)) {
        mp_err(PRINT_MPTEST,"Failed to allocate Goldensample mem\n");
        return -1;
    }
    
    if (mp_test_data->sensor_info.num_drv && mp_test_data->sensor_info.num_sen) {
        mp_test_data->pad2_drive = kmalloc(mp_test_data->sensor_info.num_drv * sizeof(u16), GFP_KERNEL);
        
        if(ERR_ALLOC_MEM(mp_test_data->pad2_drive)) {
            mp_err(PRINT_MPTEST,"Failed to allocate pad2_drive mem\n");
            return -1;
        }
        
        ilitek_ms_get_ini_data("PAD_TABLE", "DRIVE", str);
        mp_info(PRINT_MPTEST, "PAD_TABLE(DRIVE):      [%s]\n", str);
        p_mp_func->drive_len = ilitek_ms_ini_split_u16_array(str, mp_test_data->pad2_drive);
        mp_test_data->pad2_sense = kmalloc(mp_test_data->sensor_info.num_sen * sizeof(u16), GFP_KERNEL);
        
        if(ERR_ALLOC_MEM(mp_test_data->pad2_sense)) {
            mp_err(PRINT_MPTEST,"Failed to allocate pad2_sense mem\n");
            return -1;
        }
        
        ilitek_ms_get_ini_data("PAD_TABLE", "SENSE", str);
        mp_info(PRINT_MPTEST, "PAD_TABLE(SENSE):      [%s]\n", str);
        p_mp_func->sense_len = ilitek_ms_ini_split_u16_array(str, mp_test_data->pad2_sense);
    }
    
    if (mp_test_data->sensor_info.num_gr) {
        mp_test_data->pad2_gr = kmalloc(mp_test_data->sensor_info.num_gr * sizeof(u16), GFP_KERNEL);
        
        if(ERR_ALLOC_MEM(mp_test_data->pad2_gr)) {
            mp_err(PRINT_MPTEST,"Failed to allocate pad2_gr mem\n");
            return -1;
        }
        
        ilitek_ms_get_ini_data("PAD_TABLE", "GR", str);
        printk("PAD_TABLE(GR):      [%s]\n", str);
        p_mp_func->gr_len = ilitek_ms_ini_split_u16_array(str, mp_test_data->pad2_gr);
    }
    
    ilitek_ms_get_ini_data("RULES", "SHORTVALUE", str);
    mp_test_data->sensor_info.thrs_short = ilitek_ms_atoi(str);
    mp_info(PRINT_MPTEST, "SHORTVALUE:      [%d]\n", mp_test_data->sensor_info.thrs_short);
    ilitek_ms_get_ini_data("RULES", "ICPINSHORT", str);
    mp_test_data->sensor_info.thrs_ic_pin = ilitek_ms_atoi(str);
    mp_info(PRINT_MPTEST, "ICPINSHORT:      [%d]\n", mp_test_data->sensor_info.thrs_ic_pin);
    mp_kfree(token);
    mp_info(PRINT_MPTEST, "MEM free token\n");
    return 0;
}

static int mp_main_init_var(void)
{
    mp_calc_golden_range(mp_test_data->goldensample_ch_0,
                         mp_test_data->toast_info.persent_dc_va_range, mp_test_data->toast_info.persent_dc_va_range_up,
                         mp_test_data->goldensample_ch_0_max, mp_test_data->goldensample_ch_0_min, MAX_MUTUAL_NUM);
    mp_test_result->ratio_avg_max = (int) (100 + mp_test_data->toast_info.persent_dc_va_ratio + mp_test_data->toast_info.persent_dc_va_ratio_up) / 100;
    mp_test_result->ratio_avg_min = (int) (100 - mp_test_data->toast_info.persent_dc_va_ratio) / 100;
    mp_test_result->border_ratio_avg_max = (int) (100 + mp_test_data->toast_info.persent_dc_border_ratio + mp_test_data->toast_info.persent_dc_va_ratio_up) / 100;
    mp_test_result->border_ratio_avg_min = (int) (100 - mp_test_data->toast_info.persent_dc_border_ratio) / 100;
    mp_test_result->p_check_fail =               kcalloc(TEST_ITEM_NUM, sizeof(int), GFP_KERNEL);
    mp_test_result->open_result_data =           kcalloc(MAX_MUTUAL_NUM, sizeof(int), GFP_KERNEL);
    mp_test_result->open_fail_channel =          kcalloc(MAX_MUTUAL_NUM, sizeof(int), GFP_KERNEL);
    mp_test_result->open_ratio_fail_channel =     kcalloc(MAX_MUTUAL_NUM, sizeof(int), GFP_KERNEL);
    mp_test_result->p_golden_ch =                kcalloc(MAX_MUTUAL_NUM, sizeof(int), GFP_KERNEL);
    mp_test_result->p_golden_ch_max =            kcalloc(MAX_MUTUAL_NUM, sizeof(int), GFP_KERNEL);
    mp_test_result->p_golden_ch_max_avg =        kcalloc(MAX_MUTUAL_NUM, sizeof(int), GFP_KERNEL);
    mp_test_result->p_golden_ch_min =            kcalloc(MAX_MUTUAL_NUM, sizeof(int), GFP_KERNEL);
    mp_test_result->p_golden_ch_min_avg =        kcalloc(MAX_MUTUAL_NUM, sizeof(int), GFP_KERNEL);
    mp_test_result->p_short_fail_channel =         kcalloc(p_mp_func->max_channel_num, sizeof(int), GFP_KERNEL);
    mp_test_result->short_result_data =          kcalloc(p_mp_func->max_channel_num, sizeof(int), GFP_KERNEL);
    mp_test_result->p_ic_pin_channel =             kcalloc(p_mp_func->max_channel_num, sizeof(int), GFP_KERNEL);
    mp_test_result->p_ic_pin_short_fail_channel =    kcalloc(p_mp_func->max_channel_num, sizeof(int), GFP_KERNEL);
    mp_test_result->p_ic_pin_short_result_data =     kcalloc(p_mp_func->max_channel_num, sizeof(int), GFP_KERNEL);
    mp_test_result->p_ic_pin_short_r_data =          kcalloc(p_mp_func->max_channel_num, sizeof(int), GFP_KERNEL);
    mp_test_result->short_r_data =               kcalloc(p_mp_func->max_channel_num, sizeof(int), GFP_KERNEL);
    
    /* Check allocated memory status  */
    if(ERR_ALLOC_MEM(mp_test_result->p_check_fail) || ERR_ALLOC_MEM(mp_test_result->open_result_data) ||
       ERR_ALLOC_MEM(mp_test_result->open_fail_channel) || ERR_ALLOC_MEM(mp_test_result->open_ratio_fail_channel)) {
        mp_err(PRINT_MPTEST,"Failed to allocate channels' mem\n");
        return -1;
    }
    
    if(ERR_ALLOC_MEM(mp_test_result->p_golden_ch) || ERR_ALLOC_MEM(mp_test_result->p_golden_ch_max) ||
       ERR_ALLOC_MEM(mp_test_result->p_golden_ch_max_avg) || ERR_ALLOC_MEM(mp_test_result->p_golden_ch_min) ||
       ERR_ALLOC_MEM(mp_test_result->p_golden_ch_min_avg)) {
        mp_err(PRINT_MPTEST,"Failed to allocate p_golden_ch' mem\n");
        return -1;
    }
    
    if(ERR_ALLOC_MEM(mp_test_result->p_short_fail_channel) || ERR_ALLOC_MEM(mp_test_result->short_result_data) ||
       ERR_ALLOC_MEM(mp_test_result->p_ic_pin_channel) || ERR_ALLOC_MEM(mp_test_result->p_ic_pin_short_fail_channel) ||
       ERR_ALLOC_MEM(mp_test_result->p_ic_pin_short_result_data)) {
        mp_err(PRINT_MPTEST,"Failed to allocate p_short_fail_channel' mem\n");
        return -1;
    }
    
    if(ERR_ALLOC_MEM(mp_test_result->p_ic_pin_short_r_data) || ERR_ALLOC_MEM(mp_test_result->short_r_data)) {
        mp_err(PRINT_MPTEST,"Failed to allocate p_ic_pin_short_r_data' mem\n");
        return -1;
    }
    
    return 0;
}

static int mp_start_test(void)
{
    int i, ret = 0, test_retry=3,ans=-1;

    mp_info(PRINT_MPTEST, "*** %s() ***\n", __func__);
    ilitek_reset(HARDWARE_RESET_DELAY_TIME);

    ret = mp_main_init_var();
    if(ret < 0)
        goto out;

    if(mp_test_data->ui_config.b_open_test == 1) {        
        for(i = 0; i < test_retry; i++)
        {
                 if(ilitek_mp_new_flow_main(RUN_OPEN_TEST)>=0)
                 {
                        ans=0;
                        break;
                 }
        }
        mp_test_result->open_result = ans;        
        
        for (i = 0; i < MAX_MUTUAL_NUM; i++) {
            mp_test_result->p_golden_ch[i] = mp_test_data->goldensample_ch_0[i];
            mp_test_result->p_golden_ch_max[i] = mp_test_data->goldensample_ch_0_max[i];
            mp_test_result->p_golden_ch_min[i] = mp_test_data->goldensample_ch_0_min[i];
        }
    } else {
        mp_test_result->open_result = ITO_NO_TEST;
    }
    
    /* Short Test */
        ans = -1;
        if(mp_test_data->ui_config.b_short_test == 1) {
        for(i = 0; i < test_retry; i++)
        {
                 if(ilitek_mp_new_flow_main(RUN_SHORT_TEST)>=0)
                 {
                        ans=0;
                        break;
                 }
        }
        mp_test_result->short_result = ans;
    } else {
        mp_test_result->short_result = ITO_NO_TEST;
    }
    
    /* Return final result */
    if(mp_test_result->open_result == ITO_NO_TEST) {
        ret = mp_test_result->short_result;
    } else if(mp_test_result->short_result == ITO_NO_TEST) {
        ret = mp_test_result->open_result;
    } else {
        if(mp_test_result->short_result == ITO_TEST_OK &&
           mp_test_result->open_result == ITO_TEST_OK)
            ret = ITO_TEST_OK;
        else
            ret = ITO_TEST_FAIL;
    }
    
    tp_log_info("*************Result(%d): Short = %d, Open = %d \n*************", ret, mp_test_result->short_result,
           mp_test_result->open_result);
out:

    ilitek_reset(HARDWARE_RESET_DELAY_TIME);
    mdelay(10);
    return ret;
}

void mp_main_free(void)
{
    mp_info(PRINT_MPTEST, "*** %s *** \n", __func__);
    mp_kfree(mp_test_data->ana_version);
    
    if(mp_test_data->sensor_info.num_key > 0) {
        mp_kfree(mp_test_data->key_sen);
        mp_kfree(mp_test_data->sensor_info.key_type);
        mp_info(PRINT_MPTEST, "MEM free mp_test_data->key_sen\n");
        mp_info(PRINT_MPTEST, "MEM free mp_test_data->sensor_info.key_type\n");
    }
    
    mp_kfree(mp_test_data->ui_config.s_support_iic );
    mp_kfree(mp_test_data->project_name);
    mp_kfree(mp_test_data->goldensample_ch_0 );
    mp_kfree(mp_test_data->goldensample_ch_0_max);
    mp_kfree(mp_test_data->goldensample_ch_0_max_avg);
    mp_kfree(mp_test_data->goldensample_ch_0_min);
    mp_kfree(mp_test_data->goldensample_ch_0_min_avg);
    mp_kfree(mp_test_data->pad2_drive);
    mp_kfree(mp_test_data->pad2_sense);
    
    if (mp_test_data->sensor_info.num_gr) {
        mp_kfree(mp_test_data->pad2_gr);
        mp_info(PRINT_MPTEST, "MEM free mp_test_data->pad2_gr\n");
    }
    
    mp_kfree(mp_test_result->p_check_fail);
    mp_kfree(mp_test_result->open_result_data);
    mp_kfree(mp_test_result->open_fail_channel);
    mp_kfree(mp_test_result->open_ratio_fail_channel);
    mp_kfree(mp_test_result->p_golden_ch);
    mp_kfree(mp_test_result->p_golden_ch_max);
    mp_kfree(mp_test_result->p_golden_ch_max_avg);
    mp_kfree(mp_test_result->p_golden_ch_min);
    mp_kfree(mp_test_result->p_golden_ch_min_avg);
    mp_kfree(mp_test_result->p_short_fail_channel);
    mp_kfree(mp_test_result->short_result_data);
    mp_kfree(mp_test_result->short_r_data);
    mp_kfree(mp_test_result->p_ic_pin_channel);
    mp_kfree(mp_test_result->p_ic_pin_short_fail_channel);
    mp_kfree(mp_test_result->p_ic_pin_short_result_data);
    mp_kfree(mp_test_result->p_ic_pin_short_r_data);
    mp_kfree(mp_test_data);
    mp_kfree(mp_test_result);
    mp_kfree(p_mp_func);
}

int ilitek_mp_start_test(int chip_type, char *p_file_path)
{
    int ret = 0;
    mp_info(PRINT_MPTEST, "*** chip_type = 0x%x *** \n", chip_type);
    mp_info(PRINT_MPTEST, "*** iniPath = %s *** \n", p_file_path);
    mutex_lock(&ilitek_data->ilitek_mutex);
    g_in_mp_test = 1;
    mutex_unlock(&ilitek_data->ilitek_mutex);
    /* Init main structure members */
    p_mp_func = kmalloc(sizeof(struct mp_main_func), GFP_KERNEL);
    
    if(ERR_ALLOC_MEM(p_mp_func)) {
        mp_err(PRINT_MPTEST,"Failed to allocate mp_func mem\n");
        ret = -ENOMEM;
        goto out;
    }
    
    p_mp_func->chip_type = chip_type;
    
    if (chip_type == CHIP_TYPE_CAMARO) {
        p_mp_func->check_mp_switch = ilitek_camaro_check_mp_switch;
        p_mp_func->enter_mp_mode = ilitek_camaro_enter_mp_mode;
        p_mp_func->open_judge = ilitek_camaro_open_judge;
        p_mp_func->short_judge = ilitek_camaro_short_judge;
        p_mp_func->max_channel_num = MAX_CHANNEL_NUM_30XX;
        p_mp_func->fout_data_addr = 0x1361;
    }
    
    /* Parsing ini file and prepare to run MP test */
    ret = mp_load_ini(p_file_path);
    
    if(ret < 0) {
        mp_err(PRINT_MPTEST,"Failed to load ini\n");
        goto out;
    }
    
    ret = mp_start_test();
    ilitek_mp_save_result(ret);
out:
    mp_main_free();
    mutex_lock(&ilitek_data->ilitek_mutex);
    g_in_mp_test = 0;
    mutex_unlock(&ilitek_data->ilitek_mutex);
    return ret;
}