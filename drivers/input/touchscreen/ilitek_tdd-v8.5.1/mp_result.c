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

void mp_save_result(int result)
{
	int i = 0, j = 0, length = 0;
	char *SetCsvData = NULL;
	char CsvPATHName[64] = {0};
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
		mp_err("Failed to allocate CSV memory \n");
		return;
	}

	strcpy(line, "\n");
	if(mp_test_data->UIConfig.bOpenTest == 1)
	{
		strcpy(head, "Golden 0 Max,,");
		length += sprintf(SetCsvData+length,"%s",  head);
		for(i = 0; i < MAX_MUTUAL_NUM; i++)
		{
			length += sprintf(SetCsvData + length, "%d,", mp_test_result->pGolden_CH_Max[i]);
		}
		length += sprintf(SetCsvData+length,"%s",  line);
		
		strcpy(head, "Golden 0,,");
		length += sprintf(SetCsvData+length,"%s",  head);	
		for(i = 0; i < MAX_MUTUAL_NUM; i++)
		{
			length += sprintf(SetCsvData + length, "%d,", mp_test_result->pGolden_CH[i]);
		}
		length += sprintf(SetCsvData+length,"%s",  line);
		
		strcpy(head, "Golden 0 Min,,");
		length += sprintf(SetCsvData+length,"%s",  head);
		for(i = 0; i < MAX_MUTUAL_NUM; i++)
		{
			length += sprintf(SetCsvData + length, "%d,", mp_test_result->pGolden_CH_Min[i]);	
		}
		length += sprintf(SetCsvData+length,"%s",  line);
		
		strcpy(head, "test_0_deltaC,,");
		length += sprintf(SetCsvData+length,"%s",  head);	
		for(i = 0; i < MAX_MUTUAL_NUM; i++)
		{
			length += sprintf(SetCsvData + length, "%d,", mp_test_result->pOpenResultData[i]);		
		}
		length += sprintf(SetCsvData+length,"%s",  line);	

		length += sprintf(SetCsvData+length, "ANA_Version : %s\n", mp_test_data->ana_version);
		length += sprintf(SetCsvData+length, "SupportIC : %s\n", mp_test_data->UIConfig.sSupportIC);
		length += sprintf(SetCsvData+length, "Project name : %s\n", mp_test_data->project_name);
		length += sprintf(SetCsvData+length, "Mapping table name : %s\n", mp_test_result->mapTbl_sec);
		length += sprintf(SetCsvData+length, "DC_Range=%d\n", mp_test_data->ToastInfo.persentDC_VA_Range);
		length += sprintf(SetCsvData+length, "DC_Range_up=%d\n", mp_test_data->ToastInfo.persentDC_VA_Range_up);
		length += sprintf(SetCsvData+length, "DC_Ratio=%d\n", mp_test_data->ToastInfo.persentDC_VA_Ratio);
		length += sprintf(SetCsvData+length, "DC_Border_Ratio=%d\n", mp_test_data->ToastInfo.persentDC_Border_Ratio);
		length += sprintf(SetCsvData+length, "DC_Ratio_up=%d\n\n", mp_test_data->ToastInfo.persentDC_VA_Ratio_up);
	
		/* Golden Values */
		strcpy(head, "Golden,,");
		length += sprintf(SetCsvData+length,"%s",  head);
		for(i = 0; i < mp_test_data->sensorInfo.numDrv; i++)
		{
			length += sprintf(SetCsvData+length, "D%d,", i+1);
		}
		length += sprintf(SetCsvData+length,"%s", line);

		for(j = 0; j < mp_test_data->sensorInfo.numSen; j++)
		{
			length += sprintf(SetCsvData+length, ",S%d,", j+1);
			for(i = 0; i < mp_test_data->sensorInfo.numDrv; i++)
			{
				if (mp_test_result->pGolden_CH[j * mp_test_data->sensorInfo.numDrv + i] == NULL_DATA) //for mutual key
				{
					length += sprintf(SetCsvData+length, "%s", ",");	
				}
				else
				{
					length += sprintf(SetCsvData+length, "%.2d,", mp_test_result->pGolden_CH[j * mp_test_data->sensorInfo.numDrv + i]);	
				}
			}
			length += sprintf(SetCsvData+length,"%s", line);
		}
		length += sprintf(SetCsvData+length,"%s", line);

		/* Deltac */
		strcpy(head, "DeltaC,,");
		length += sprintf(SetCsvData+length,"%s",  head);	
		for(i = 0; i < mp_test_data->sensorInfo.numDrv; i++)
		{
			length += sprintf(SetCsvData+length, "D%d,", i+1);	
		}
		length += sprintf(SetCsvData+length,"%s", line);

		for(j = 0; j < mp_test_data->sensorInfo.numSen; j++)
		{
			length += sprintf(SetCsvData+length, ",S%d,", j+1);	
			for(i = 0; i < mp_test_data->sensorInfo.numDrv; i++)
			{
				if (mp_test_result->pOpenResultData[j * mp_test_data->sensorInfo.numDrv + i] == NULL_DATA) //for mutual key
				{
					length += sprintf(SetCsvData+length, "%s", ",");	
				}
				else
				{
					length+=sprintf(SetCsvData+length, "%1d,", mp_test_result->pOpenResultData[j * mp_test_data->sensorInfo.numDrv + i]);	
				}
			}
			length+=sprintf(SetCsvData+length,"%s", line);
		}

		/* Printing the result of Deltac */
		if(mp_test_result->nOpenResult == ITO_TEST_OK)
		{
			length += sprintf(SetCsvData+length, "DeltaC_Result:PASS\n");	
		}
		else
		{
			if(mp_test_result->pCheck_Fail[0] == 1)
			{
				length += sprintf(SetCsvData+length, "DeltaC_Result:FAIL\nFail Channel:");
				for (i = 0; i < MAX_MUTUAL_NUM; i++)
				{
					if (mp_test_result->pOpenFailChannel[i] == PIN_NO_ERROR)
						continue;

					length += sprintf(SetCsvData+length,"D%1d.S%2d", mp_test_result->pOpenFailChannel[i] % 100, mp_test_result->pOpenFailChannel[i] / 100);
				}
				length += sprintf(SetCsvData+length,"%s", line);
			}
			else
			{
				length+=sprintf(SetCsvData+length, "DeltaC_Result:PASS\n");
			}
		}
	}

	if(mp_test_data->UIConfig.bUniformityTest == 1)
	{
		strcpy(head, "\n\nUniformity,,");
		length += sprintf(SetCsvData+length,"%s",  head);
		length += sprintf(SetCsvData+length,"\n Golden Max,,<%d => PASS", mp_test_data->Unifor_BG_Golden_Max);
		length += sprintf(SetCsvData+length,"\n Golden Min,,>%d => PASS", mp_test_data->Unifor_BG_Golden_Min);
		length += sprintf(SetCsvData+length,"\n BG Sum Data, %d", mp_test_result->Unifor_sum);
		strcpy(head, "\n\nUniformity Resilt");
		if(mp_test_result->nUniformityResult == ITO_TEST_FAIL)
				strcpy(head, "\nUniformity  Result : FAIL,,");
		else
				strcpy(head, "\nUniformity  Result : PASS,,");
		length += sprintf(SetCsvData+length,"%s",  head);
	}
	if(mp_test_data->UIConfig.bShortTest == 1)
	{
		/* Short Pin */
		length += sprintf(SetCsvData+length, "\n\nShortValue=%d\n\n", mp_test_data->sensorInfo.thrsICpin);

		strcpy(head, "\ndeltaR,,");
		length+=sprintf(SetCsvData+length,"%s",  head);
		for (i = 0; i < 10; i ++)
		{
			length += sprintf(SetCsvData+length, "%d,", i+1);
		}
		for (i = 0; i < (mp_test_data->sensorInfo.numSen); i ++)
		{
			if ((i % 10) == 0)
			{
				length += sprintf(SetCsvData+length, "\n,S%d,", i);
			}
			if(mp_test_result->pShortRData[i] > 1000)
				length += sprintf(SetCsvData+length, "%1dM,",  mp_test_result->pShortRData[i] / 1000);
			else
				length += sprintf(SetCsvData+length, "%3dK,",  mp_test_result->pShortRData[i]);
			//length += sprintf(SetCsvData+length, "%dM,", mp_test_result->pShortRData[i]);
		}
		length += sprintf(SetCsvData+length,"%s", line);

		for (i = 0; i < (mp_test_data->sensorInfo.numDrv); i ++)
		{
			if ((i % 10) == 0)
			{
				length += sprintf(SetCsvData+length, "\n,D%d,", i);
			}
			if(mp_test_result->pShortRData[i + mp_test_data->sensorInfo.numSen] > 1000)
				length += sprintf(SetCsvData+length, "%1dM,",  mp_test_result->pShortRData[i + mp_test_data->sensorInfo.numSen] / 1000);
			else
				length += sprintf(SetCsvData+length, "%3dK,",  mp_test_result->pShortRData[i + mp_test_data->sensorInfo.numSen]);
		}
		length += sprintf(SetCsvData+length,"%s", line);

		for (i = 0; i < mp_test_data->sensorInfo.numGr; i ++)
		{
			if ((i % 10) == 0)
			{
				length += sprintf(SetCsvData+length, "\n,GR%d,", i);
			}

			if(mp_test_result->pShortRData[i + mp_test_data->sensorInfo.numSen + mp_test_data->sensorInfo.numDrv] > 1000)
				length += sprintf(SetCsvData+length, "%1dM,",  mp_test_result->pShortRData[i + mp_test_data->sensorInfo.numSen + mp_test_data->sensorInfo.numDrv] / 1000);
			else
				length += sprintf(SetCsvData+length, "%3dK,",  mp_test_result->pShortRData[i + mp_test_data->sensorInfo.numSen + mp_test_data->sensorInfo.numDrv]);
		}

		/* ITO Short */
		if (mp_test_result->nShortResult == ITO_TEST_OK)
		{
			strcpy(head, "\nITO Short Test:PASS,");
			length += sprintf(SetCsvData+length,"%s",  head);
		}
		else 
		{
			strcpy(head, "\nITO Short Test:FAIL\nFail Channel:,,");
			length += sprintf(SetCsvData+length,"%s",  head);
			for (i = 0; i < mp_test_data->sensorInfo.numSen; i++)
			{
				if (mp_test_result->pShortFailChannel[i] == PIN_UN_USE)
					continue;
				length += sprintf(SetCsvData+length, "S%d,", i + 1);
			}
			for (; i < mp_test_data->sensorInfo.numSen + mp_test_data->sensorInfo.numDrv; i++) {
				if (mp_test_result->pShortFailChannel[i] == PIN_UN_USE)
					continue;
				length += sprintf(SetCsvData+length, "D%d,", i + 1 - mp_test_data->sensorInfo.numSen);
			}
			for (; i < mp_test_data->sensorInfo.numSen + mp_test_data->sensorInfo.numDrv + mp_test_data->sensorInfo.numGr; i++) {
				if (mp_test_result->pShortFailChannel[i] == PIN_UN_USE)
					continue;
				length+=sprintf(SetCsvData+length, "GR%d", i + 1 - mp_test_data->sensorInfo.numSen - mp_test_data->sensorInfo.numDrv);
			}
		}
		strcpy(head, "\nResultData,,");
		length += sprintf(SetCsvData+length,"%s",  head);
		for (i = 0; i < 10; i ++)
		{
			length += sprintf(SetCsvData+length, "%d,", i+1);
		}
		for (i = 0; i < (mp_test_data->sensorInfo.numSen); i ++)
		{
			if ((i % 10) == 0)
			{
				length += sprintf(SetCsvData+length, "\n,S%d,", i);
			}
			length += sprintf(SetCsvData+length, "%d,",  mp_test_result->pShortResultData[i]);
		}

		for (i = 0; i < (mp_test_data->sensorInfo.numDrv); i ++)
		{
			if ((i % 10) == 0)
			{
				length += sprintf(SetCsvData+length, "\n,D%d,", i);
			}
			length += sprintf(SetCsvData+length, "%d,",  mp_test_result->pShortResultData[i + mp_test_data->sensorInfo.numSen]);
		}

		for (i = 0; i < (mp_test_data->sensorInfo.numGr); i ++)
		{
			if ((i % 10) == 0)
			{
				length += sprintf(SetCsvData+length, "\n,GR%d,", i);
			}
			length += sprintf(SetCsvData+length, "%d,",  mp_test_result->pShortResultData[i + mp_test_data->sensorInfo.numSen + mp_test_data->sensorInfo.numDrv]);	
		}
	}
	pr_info("CSV: Final Result = %d \n",result);

	if(result == 1)
		sprintf(CsvPATHName,"%s%s", "/sdcard/","mp_pass.csv");
	else
		sprintf(CsvPATHName,"%s%s", "/sdcard/","mp_fail.csv");

	pr_info("CSV : %s , length = %d\n", CsvPATHName, length);
	f = filp_open(CsvPATHName, O_CREAT | O_RDWR | O_TRUNC, 0);
	if(ERR_ALLOC_MEM(f))
	{
		mp_info("%s:%d: Failed to open file \n",__func__,__LINE__);
		goto out;
	}
	fs = get_fs();
   	set_fs(KERNEL_DS);
	pos = 0;
    vfs_write(f, SetCsvData, length, &pos);
	set_fs(fs);
out:
	if(!ERR_ALLOC_MEM(f))
		filp_close(f, NULL);
	vfree(SetCsvData);
	SetCsvData = NULL;
	mp_kfree((void **)&head);
	mp_kfree((void **)&line);
}