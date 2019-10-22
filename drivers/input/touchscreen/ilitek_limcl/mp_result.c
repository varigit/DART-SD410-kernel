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

#include <linux/vmalloc.h>
#include "mp_common.h"

void ilitek_mp_save_result(int result)
{
    int i = 0, j = 0, length = 0;
    char *SetCsvData = NULL;
    char CsvPATHName[64];
    struct file *f = NULL;
    char *head = NULL;
    char *line = NULL;
    loff_t pos;
    mm_segment_t fs;
    SetCsvData = vmalloc(400 * 1024 * sizeof(char));
    head = kmalloc(1024 * sizeof(char), GFP_KERNEL);
    line = kmalloc(1024 * sizeof(char), GFP_KERNEL);
    
    if(ERR_ALLOC_MEM(SetCsvData) || ERR_ALLOC_MEM(SetCsvData)
       || ERR_ALLOC_MEM(SetCsvData)) {
        mp_err(PRINT_MPTEST,"Failed to allocate CSV memory \n");
        return;
    }
    
    strcpy(line, "\n");
    strcpy(head, "Golden 0 Max,,");
    length += sprintf(SetCsvData + length, "%s",  head);
    
    for(i = 0; i < MAX_MUTUAL_NUM; i++) {
        length += sprintf(SetCsvData + length, "%d,", mp_test_result->p_golden_ch_max[i]);
    }
    
    length += sprintf(SetCsvData + length, "%s",  line);
    strcpy(head, "Golden 0,,");
    length += sprintf(SetCsvData + length, "%s",  head);
    
    for(i = 0; i < MAX_MUTUAL_NUM; i++) {
        length += sprintf(SetCsvData + length, "%d,", mp_test_result->p_golden_ch[i]);
    }
    
    length += sprintf(SetCsvData + length, "%s",  line);
    strcpy(head, "Golden 0 Min,,");
    length += sprintf(SetCsvData + length, "%s",  head);
    
    for(i = 0; i < MAX_MUTUAL_NUM; i++) {
        length += sprintf(SetCsvData + length, "%d,", mp_test_result->p_golden_ch_min[i]);
    }
    
    length += sprintf(SetCsvData + length, "%s",  line);
    strcpy(head, "test_0_deltaC,,");
    length += sprintf(SetCsvData + length, "%s",  head);
    
    for(i = 0; i < MAX_MUTUAL_NUM; i++) {
        length += sprintf(SetCsvData + length, "%d,", mp_test_result->open_result_data[i]);
    }
    
    length += sprintf(SetCsvData + length, "%s",  line);
    // length += sprintf(SetCsvData+length, "test_0_ratio,[%1d~%2d],", mp_test_result->ratio_avg_max, mp_test_result->ratio_avg_min);
    // for(i = 0; i < MAX_MUTUAL_NUM; i++)
    // {
    // 	length+=sprintf(SetCsvData+length, "%1d,", mp_test_result->p_golden_ch_max_avg[i]);
    // }
    // length += sprintf(SetCsvData+length,"%s",  line);
    // length += sprintf(SetCsvData+length, "test_border_ratio,[%1d~%2d] ", mp_test_result->border_ratio_avg_max, mp_test_result->border_ratio_avg_min);
    // for(i = 0; i < MAX_MUTUAL_NUM; i++)
    // {
    // 	sprintf(SetCsvData, "%s", ",");
    // 	f->f_op->write(f, SetCsvData, strlen(SetCsvData) * sizeof(char), &f->f_pos);
    // }
    // length += sprintf(SetCsvData+length, "Platform Version :%s\n", tpinfo.PlatformVersion);
    // length += sprintf(SetCsvData+length, "Device Driver Version : %s\n", DEVICE_DRIVER_RELEASE_VERSION);
    // length += sprintf(SetCsvData+length, "FW Version : %s\n", tpinfo.FwVersion);
    // length += sprintf(SetCsvData+length, "Main Block FW Version : %s\n", tpinfo.MainBlockFWVersion);
    // length += sprintf(SetCsvData+length, "Info Block FW Version : %s\n", tpinfo.InfoBlockFWVersion);
    length += sprintf(SetCsvData + length, "ANA_Version : %s\n", mp_test_data->ana_version);
    length += sprintf(SetCsvData + length, "SupportIC : %s\n", mp_test_data->ui_config.s_support_iic);
    length += sprintf(SetCsvData + length, "Project name : %s\n", mp_test_data->project_name);
    length += sprintf(SetCsvData + length, "Mapping table name : %s\n", mp_test_result->map_tbl_sec);
    length += sprintf(SetCsvData + length, "DC_Range=%d\n", mp_test_data->toast_info.persent_dc_va_range);
    length += sprintf(SetCsvData + length, "DC_Range_up=%d\n", mp_test_data->toast_info.persent_dc_va_range_up);
    length += sprintf(SetCsvData + length, "DC_Ratio=%d\n", mp_test_data->toast_info.persent_dc_va_ratio);
    length += sprintf(SetCsvData + length, "DC_Border_Ratio=%d\n", mp_test_data->toast_info.persent_dc_border_ratio);
    length += sprintf(SetCsvData + length, "DC_Ratio_up=%d\n\n", mp_test_data->toast_info.persent_dc_va_ratio_up);
    /* Golden Values */
    strcpy(head, "Golden,,");
    length += sprintf(SetCsvData + length, "%s",  head);
    
    for(i = 0; i < mp_test_data->sensor_info.num_drv; i++) {
        length += sprintf(SetCsvData + length, "D%d,", i + 1);
    }
    
    length += sprintf(SetCsvData + length, "%s", line);
    
    for(j = 0; j < mp_test_data->sensor_info.num_sen; j++) {
        length += sprintf(SetCsvData + length, ",S%d,", j + 1);
        
        for(i = 0; i < mp_test_data->sensor_info.num_drv; i++) {
            if (mp_test_result->p_golden_ch[j * mp_test_data->sensor_info.num_drv + i] == NULL_DATA) { //for mutual key
                length += sprintf(SetCsvData + length, "%s", ",");
            } else {
                length += sprintf(SetCsvData + length, "%.2d,", mp_test_result->p_golden_ch[j * mp_test_data->sensor_info.num_drv + i]);
            }
        }
        
        length += sprintf(SetCsvData + length, "%s", line);
    }
    
    length += sprintf(SetCsvData + length, "%s", line);
    /* Deltac */
    strcpy(head, "DeltaC,,");
    length += sprintf(SetCsvData + length, "%s",  head);
    
    for(i = 0; i < mp_test_data->sensor_info.num_drv; i++) {
        length += sprintf(SetCsvData + length, "D%d,", i + 1);
    }
    
    length += sprintf(SetCsvData + length, "%s", line);
    
    for(j = 0; j < mp_test_data->sensor_info.num_sen; j++) {
        length += sprintf(SetCsvData + length, ",S%d,", j + 1);
        
        for(i = 0; i < mp_test_data->sensor_info.num_drv; i++) {
            if (mp_test_result->open_result_data[j * mp_test_data->sensor_info.num_drv + i] == NULL_DATA) { //for mutual key
                length += sprintf(SetCsvData + length, "%s", ",");
            } else {
                length += sprintf(SetCsvData + length, "%1d,", mp_test_result->open_result_data[j * mp_test_data->sensor_info.num_drv + i]);
            }
        }
        
        length += sprintf(SetCsvData + length, "%s", line);
    }
    
    /* Printing the result of Deltac */
    if(mp_test_result->open_result == ITO_TEST_OK) {
        length += sprintf(SetCsvData + length, "DeltaC_Result:PASS\n");
    } else {
        if(mp_test_result->p_check_fail[0] == 1) {
            length += sprintf(SetCsvData + length, "DeltaC_Result:FAIL\nFail Channel:");
            
            for (i = 0; i < MAX_MUTUAL_NUM; i++) {
                if (mp_test_result->open_fail_channel[i] == PIN_NO_ERROR)
                    continue;
                    
                length += sprintf(SetCsvData + length, "D%1d.S%2d", mp_test_result->open_fail_channel[i] % 100, mp_test_result->open_fail_channel[i] / 100);
            }
            
            length += sprintf(SetCsvData + length, "%s", line);
        } else {
            length += sprintf(SetCsvData + length, "DeltaC_Result:PASS\n");
        }
    }
    
    /* Ration */
    strcpy(head, "\nRatio,,");
    length += sprintf(SetCsvData + length, "%s",  head);
    
    for(i = 0; i < mp_test_data->sensor_info.num_drv; i++) {
        length += sprintf(SetCsvData + length, "D%d,", i + 1);
    }
    
    length += sprintf(SetCsvData + length, "%s", line);
    
    for(j = 0; j < mp_test_data->sensor_info.num_sen; j++) {
        length += sprintf(SetCsvData + length, ",S%d,", j + 1);
        
        for(i = 0; i < mp_test_data->sensor_info.num_drv; i++) {
            if (mp_test_result->open_result_data[j * mp_test_data->sensor_info.num_drv + i] == NULL_DATA) { //for mutual key
                length += sprintf(SetCsvData + length, "%s", ",");
            } else {
                length += sprintf(SetCsvData + length, "%1d,", mp_test_result->p_golden_ch_max_avg[j * mp_test_data->sensor_info.num_drv + i]);
            }
        }
        
        length += sprintf(SetCsvData + length, "%s", line);
    }
    
    /* Printing the result of Ratio */
    if(mp_test_result->open_result == 1) {
        length += sprintf(SetCsvData + length, "Ratio_Result:PASS\n");
    } else {
        if(mp_test_result->p_check_fail[1] == 1) {
            length += sprintf(SetCsvData + length, "Ratio_Result:FAIL\nFail Channel:");
            
            for (i = 0; i < MAX_MUTUAL_NUM; i++) {
                if (mp_test_result->open_fail_channel[i] == PIN_NO_ERROR)
                    continue;
                    
                length += sprintf(SetCsvData + length, "D%1d.S%2d", mp_test_result->open_ratio_fail_channel[i] % 100, mp_test_result->open_ratio_fail_channel[i] / 100);
            }
            
            length += sprintf(SetCsvData + length, "%s", line);
        } else {
            length += sprintf(SetCsvData + length, "Ratio_Result:PASS\n");
        }
    }
    
    /* Short Pin */
    length += sprintf(SetCsvData + length, "\n\nShortValue=%d\n\n", mp_test_data->sensor_info.thrs_short);
//	strcpy(head, "Pin Number,,");
    // length += sprintf(SetCsvData+length,"%s",  head);
    // max_channel = MAX_CHANNEL_NUM_28XX;
    // if (tpinfo.ChipType == CHIP_TYPE_MSG28XXA)
    // 	max_channel = MAX_CHANNEL_NUM_30XX;
    // for (i = 0; i < max_channel; i ++)
    // {
    // 	if (mp_test_result->p_ic_pin_channel[i] == 0)
    // 		continue;
    // 	length += sprintf(SetCsvData+length, "P%d,",mp_test_result->p_ic_pin_channel[i]);
    // }
    // strcpy(head, "\nDeltaR,,");
    // length+=sprintf(SetCsvData+length,"%s",  head);
    // for (i = 0; i < max_channel; i ++)
    // {
    // 	if (mp_test_result->p_ic_pin_channel[i] == 0)
    // 		continue;
    // 	length += sprintf(SetCsvData+length, "%1dM,",mp_test_result->p_ic_pin_short_r_data[i]);
    // }
    // strcpy(SetCsvData, "\nResultData,,");
    // length+=sprintf(SetCsvData+length,"%s",  head);
    // for (i = 0; i < max_channel; i ++)
    // {
    // 	if (mp_test_result->p_ic_pin_channel[i] == 0)
    // 		continue;
    // 	length += sprintf(SetCsvData+length, "%d,",mp_test_result->p_ic_pin_short_result_data[i]);
    // }
    // if (mp_test_result->short_result == ITO_TEST_OK)
    // {
    // 	length += sprintf(SetCsvData+length, "\nICPin Short Test:PASS\n");
    // }
    // else
    // {
    // 	if (mp_test_result->p_check_fail[2] == 1)
    // 	{
    // 		length += sprintf(SetCsvData+length, "\nICPin Short Test:FAIL\nFail Channel:,,");
    // 		for (i = 0; i < max_channel; i++)
    // 		{
    // 			if (mp_test_result->p_ic_pin_short_fail_channel[i] == 0)
    // 				continue;
    // 			length += sprintf(SetCsvData+length, "P%d,",mp_test_result->p_ic_pin_short_fail_channel[i]);
    // 		}
    // 	}
    // 	else
    // 	{
    // 		length += sprintf(SetCsvData+length, "\nICPin Short Test:PASS\n");
    // 	}
    // }
    strcpy(head, "\ndeltaR,,");
    length += sprintf(SetCsvData + length, "%s",  head);
    
    for (i = 0; i < 10; i ++) {
        length += sprintf(SetCsvData + length, "%d,", i + 1);
    }
    
    for (i = 0; i < (mp_test_data->sensor_info.num_sen); i ++) {
        if ((i % 10) == 0) {
            length += sprintf(SetCsvData + length, "\n,S%d,", i);
        }
        
        if(mp_test_result->short_r_data[i] > 1000)
            length += sprintf(SetCsvData + length, "%1dM,",  mp_test_result->short_r_data[i] / 1000);
        else
            length += sprintf(SetCsvData + length, "%3dK,",  mp_test_result->short_r_data[i]);
            
        //length += sprintf(SetCsvData+length, "%dM,", mp_test_result->short_r_data[i]);
    }
    
    length += sprintf(SetCsvData + length, "%s", line);
    
    for (i = 0; i < (mp_test_data->sensor_info.num_drv); i ++) {
        if ((i % 10) == 0) {
            length += sprintf(SetCsvData + length, "\n,D%d,", i);
        }
        
        if(mp_test_result->short_r_data[i + mp_test_data->sensor_info.num_sen] > 1000)
            length += sprintf(SetCsvData + length, "%1dM,",  mp_test_result->short_r_data[i + mp_test_data->sensor_info.num_sen] / 1000);
        else
            length += sprintf(SetCsvData + length, "%3dK,",  mp_test_result->short_r_data[i + mp_test_data->sensor_info.num_sen]);
    }
    
    length += sprintf(SetCsvData + length, "%s", line);
    
    for (i = 0; i < mp_test_data->sensor_info.num_gr; i ++) {
        if ((i % 10) == 0) {
            length += sprintf(SetCsvData + length, "\n,GR%d,", i);
        }
        
        length += sprintf(SetCsvData + length, "%d,",  mp_test_result->short_result_data[i + mp_test_data->sensor_info.num_sen + mp_test_data->sensor_info.num_drv]);
    }
    
    /* ITO Short */
    if (mp_test_result->short_result == ITO_TEST_OK) {
        strcpy(head, "\nITO Short Test:PASS,");
        length += sprintf(SetCsvData + length, "%s",  head);
    } else {
        //if (mp_test_result->p_check_fail[3] == 1)
        {
            strcpy(head, "\nITO Short Test:FAIL\nFail Channel:,,");
            length += sprintf(SetCsvData + length, "%s",  head);
            
            for (i = 0; i < mp_test_data->sensor_info.num_sen; i++) {
                if (mp_test_result->p_short_fail_channel[i] == PIN_UN_USE)
                    continue;
                    
                length += sprintf(SetCsvData + length, "S%d,", i + 1);
            }
            
            for (; i < mp_test_data->sensor_info.num_sen + mp_test_data->sensor_info.num_drv; i++) {
                if (mp_test_result->p_short_fail_channel[i] == PIN_UN_USE)
                    continue;
                    
                length += sprintf(SetCsvData + length, "D%d,", i + 1 - mp_test_data->sensor_info.num_sen);
            }
            
            for (; i < mp_test_data->sensor_info.num_sen + mp_test_data->sensor_info.num_drv + mp_test_data->sensor_info.num_gr; i++) {
                if (mp_test_result->p_short_fail_channel[i] == PIN_UN_USE)
                    continue;
                    
                length += sprintf(SetCsvData + length, "GR%d", i + 1 - mp_test_data->sensor_info.num_sen - mp_test_data->sensor_info.num_drv);
            }
        }
        // else
        // {
        // 	strcpy(head, "\nITO Short Test:PASS,");
        // 	length += sprintf(SetCsvData+length,"%s",  head);
        // }
    }
    
    strcpy(head, "\nResultData,,");
    length += sprintf(SetCsvData + length, "%s",  head);
    
    for (i = 0; i < 10; i ++) {
        length += sprintf(SetCsvData + length, "%d,", i + 1);
    }
    
    for (i = 0; i < (mp_test_data->sensor_info.num_sen); i ++) {
        if ((i % 10) == 0) {
            length += sprintf(SetCsvData + length, "\n,S%d,", i);
        }
        
        length += sprintf(SetCsvData + length, "%d,",  mp_test_result->short_result_data[i]);
    }
    
    for (i = 0; i < (mp_test_data->sensor_info.num_drv); i ++) {
        if ((i % 10) == 0) {
            length += sprintf(SetCsvData + length, "\n,D%d,", i);
        }
        
        length += sprintf(SetCsvData + length, "%d,",  mp_test_result->short_result_data[i + mp_test_data->sensor_info.num_sen]);
    }
    
    for (i = 0; i < (mp_test_data->sensor_info.num_gr); i ++) {
        if ((i % 10) == 0) {
            length += sprintf(SetCsvData + length, "\n,GR%d,", i);
        }
        
        length += sprintf(SetCsvData + length, "%d,",  mp_test_result->short_result_data[i + mp_test_data->sensor_info.num_sen + mp_test_data->sensor_info.num_drv]);
    }
    
    memset(CsvPATHName, 0, sizeof(CsvPATHName));
    mp_info(PRINT_MPTEST, "Final Result = %d \n", result);
    
    if(result == 1)
        sprintf(CsvPATHName, "%s%s", "/sdcard/", "mp_pass.csv");
    else
        sprintf(CsvPATHName, "%s%s", "/sdcard/", "mp_fail.csv");
        
    mp_info(PRINT_MPTEST, "CSV : %s , length = %d\n", CsvPATHName, length);
    f = filp_open(CsvPATHName, O_CREAT | O_RDWR , 0);
    
    if(ERR_ALLOC_MEM(f)) {
        mp_info(PRINT_MPTEST, "%s:%d: Failed to open file \n", __func__, __LINE__);
        goto out;
    }
    
    fs = get_fs();
    set_fs(KERNEL_DS);
    pos = 0;
    vfs_write(f, SetCsvData, length, &pos);
    //filp_close(f, NULL);
    set_fs(fs);
out:

    if(!ERR_ALLOC_MEM(f))
        filp_close(f, NULL);
        
    vfree(SetCsvData);
    SetCsvData = NULL;
    mp_kfree(head);
    mp_kfree(line);
    mp_info(PRINT_MPTEST, "MEM free head\n");
    mp_info(PRINT_MPTEST, "MEM free line\n");
}