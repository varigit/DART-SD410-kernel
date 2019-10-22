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

/*
 * This parser accpets the size of 600*512 to store values from ini file.
 * It would be doubled in kernel as key and value seprately.
 * Strongly suggest that do not modify the size unless you know how much
 * size of your kernel stack is.
 */
#define PARSER_MAX_CFG_BUF          (512*2)
#define PARSER_MAX_KEY_NUM	        (600*2)

#define PARSER_MAX_KEY_NAME_LEN	    100
#define PARSER_MAX_KEY_VALUE_LEN	2000

typedef struct _st_ini_file_data {
    char section_name[PARSER_MAX_KEY_NAME_LEN];
    char key_name[PARSER_MAX_KEY_NAME_LEN];
    char key_value[PARSER_MAX_KEY_VALUE_LEN];
    int section_name_len;
    int key_name_len;
    int key_value_len;
} st_ini_file_data;

st_ini_file_data ms_ini_file_data[PARSER_MAX_KEY_NUM];

int ini_items = 0;
char m_cfg_ssl = '[';
char m_cfg_ssr = ']';
char m_cfg_nis = ':';
char m_cfg_nts = '#';
char m_cfg_eqs = '=';

static int isdigit_t(int x)
{
    if(x <= '9' && x >= '0')
        return 1;
    else
        return 0;
}

static int isspace_t(int x)
{
    if(x == ' ' || x == '\t' || x == '\n' || x == '\f' || x == '\b' || x == '\r')
        return 1;
    else
        return 0;
}

static long atol_t(char *nptr)
{
    int c; /* current char */
    long total; /* current total */
    int sign; /* if ''-'', then negative, otherwise positive */
    
    /* skip whitespace */
    while ( isspace_t((int)(unsigned char)*nptr) )
        ++nptr;
        
    c = (int)(unsigned char) * nptr++;
    sign = c; /* save sign indication */
    
    if (c == '-' || c == '+')
        c = (int)(unsigned char) * nptr++; /* skip sign */
        
    total = 0;
    
    while (isdigit_t(c)) {
        total = 10 * total + (c - '0'); /* accumulate digit */
        c = (int)(unsigned char) * nptr++; /* get next char */
    }
    
    if (sign == '-')
        return -total;
    else
        return total; /* return result, negated if necessary */
}

int ilitek_ms_atoi(char *nptr)
{
    return (int)atol_t(nptr);
}

int ms_ini_file_get_line(char *filedata, char *buffer, int maxlen)
{
    int i = 0;
    int j = 0;
    int iRetNum = -1;
    char ch1 = '\0';
    
    for(i = 0, j = 0; i < maxlen; j++) {
        ch1 = filedata[j];
        iRetNum = j + 1;
        
        if(ch1 == '\n' || ch1 == '\r') { //line end
            ch1 = filedata[j + 1];
            
            if(ch1 == '\n' || ch1 == '\r') {
                iRetNum++;
            }
            
            break;
        } else if(ch1 == 0x00) {
            iRetNum = -1;
            break; //file end
        } else {
            buffer[i++] = ch1;
        }
    }
    
    buffer[i] = '\0';
    return iRetNum;
}

static char *ms_ini_str_trim_r(char * buf)
{
    int len, i;
    char tmp[1024] = {0};
    len = strlen(buf);
    
    for(i = 0; i < len; i++) {
        if (buf[i] != ' ')
            break;
    }
    
    if (i < len) {
        strncpy(tmp, (buf + i), (len - i));
    }
    
    strncpy(buf, tmp, len);
    return buf;
}

static char *ms_ini_str_trim_l(char * buf)
{
    int len, i;
    char tmp[1024];
    memset(tmp, 0, sizeof(tmp));
    len = strlen(buf);
    memset(tmp, 0x00, len);
    
    for(i = 0; i < len; i++) {
        if (buf[len - i - 1] != ' ')
            break;
    }
    
    if (i < len) {
        strncpy(tmp, buf, len - i);
    }
    
    strncpy(buf, tmp, len);
    return buf;
}

static void ms_init_key_data(void)
{
    int i = 0;
    mp_info(PRINT_MPTEST, " %s\n", __func__ );
    ini_items = 0;
    
    for(i = 0; i < PARSER_MAX_KEY_NUM; i++) {
        memset(ms_ini_file_data[i].section_name, 0, PARSER_MAX_KEY_NAME_LEN);
        memset(ms_ini_file_data[i].key_name, 0, PARSER_MAX_KEY_NAME_LEN);
        memset(ms_ini_file_data[i].key_value, 0, PARSER_MAX_KEY_VALUE_LEN);
        ms_ini_file_data[i].section_name_len = 0;
        ms_ini_file_data[i].key_name_len = 0;
        ms_ini_file_data[i].key_value_len = 0;
    }
}

static int ms_ini_get_key_data(char *filedata)
{
    int i, ret = 0, n = 0, dataoff = 0, iEqualSign = 0;
    char *ini_buf = NULL, *tmsection_name = NULL;
    
    if (filedata == NULL) {
        mp_err(PRINT_MPTEST,"INI filedata is NULL\n");
        ret = -EINVAL;
        goto out;
    }
    
    ini_buf = kzalloc((PARSER_MAX_CFG_BUF + 1) * sizeof(char), GFP_KERNEL);
    
    if (ERR_ALLOC_MEM(ini_buf)) {
        mp_err(PRINT_MPTEST,"Failed to allocate ini_buf memory, %ld\n", PTR_ERR(ini_buf));
        ret = -ENOMEM;
        goto out;
    }
    
    tmsection_name = kzalloc((PARSER_MAX_CFG_BUF + 1) * sizeof(char), GFP_KERNEL);
    
    if (ERR_ALLOC_MEM(tmsection_name)) {
        mp_err(PRINT_MPTEST,"Failed to allocate tmsection_name memory, %ld\n", PTR_ERR(tmsection_name));
        ret = -ENOMEM;
        goto out;
    }
    
    while(1) {
        if(ini_items > PARSER_MAX_KEY_NUM) {
            mp_err(PRINT_MPTEST,"MAX_KEY_NUM: Out Of Length\n");
            goto out;
        }
        
        n = ms_ini_file_get_line(filedata + dataoff, ini_buf, PARSER_MAX_CFG_BUF);
        
        if (n < 0) {
            mp_err(PRINT_MPTEST,"End of Line\n");
            goto out;
        }
        
        dataoff += n;
        n = strlen(ms_ini_str_trim_l(ms_ini_str_trim_r(ini_buf)));
        
        if(n == 0 || ini_buf[0] == m_cfg_nts)
            continue;
            
        /* Get section names */
        if(n > 2 && ((ini_buf[0] == m_cfg_ssl && ini_buf[n - 1] != m_cfg_ssr))) {
            mp_err(PRINT_MPTEST,"Bad Section:%s\n\n", ini_buf);
            ret = -EINVAL;
            goto out;
        }
        
        if(ini_buf[0] == m_cfg_ssl) {
            ms_ini_file_data[ini_items].section_name_len = n - 2;
            
            if(PARSER_MAX_KEY_NAME_LEN < ms_ini_file_data[ini_items].section_name_len) {
                mp_err(PRINT_MPTEST,"MAX_KEY_NAME_LEN: Out Of Length\n");
                ret = -1;
                goto out;
            }
            
            ini_buf[n - 1] = 0x00;
            strcpy((char *)tmsection_name, ini_buf + 1);
            mp_info(PRINT_MPTEST, "Section Name:%s, Len:%d\n", tmsection_name, n - 2);
            continue;
        }
        
        /* copy section's name without square brackets to its real buffer */
        strcpy( ms_ini_file_data[ini_items].section_name, tmsection_name);
        ms_ini_file_data[ini_items].section_name_len = strlen(tmsection_name);
        iEqualSign = 0;
        
        for(i = 0; i < n; i++) {
            if(ini_buf[i] == m_cfg_eqs ) {
                iEqualSign = i;
                break;
            }
        }
        
        if(0 == iEqualSign)
            continue;
            
        /* Get key names */
        ms_ini_file_data[ini_items].key_name_len = iEqualSign;
        
        if(PARSER_MAX_KEY_NAME_LEN < ms_ini_file_data[ini_items].key_name_len) {
            mp_err(PRINT_MPTEST,"MAX_KEY_NAME_LEN: Out Of Length\n\n");
            ret = -1;
            goto out;
        }
        
        memcpy(ms_ini_file_data[ini_items].key_name,
               ini_buf, ms_ini_file_data[ini_items].key_name_len);
        /* Get a value with its key */
        ms_ini_file_data[ini_items].key_value_len = n - iEqualSign - 1;
        
        if(PARSER_MAX_KEY_VALUE_LEN < ms_ini_file_data[ini_items].key_value_len) {
            printk("MAX_KEY_VALUE_LEN: Out Of Length\n\n");
            ret = -1;
            goto out;
        }
        
        memcpy(ms_ini_file_data[ini_items].key_value,
               ini_buf + iEqualSign + 1, ms_ini_file_data[ini_items].key_value_len);
        mp_info(PRINT_MPTEST, "%s = %s\n", ms_ini_file_data[ini_items].key_name,
                ms_ini_file_data[ini_items].key_value);
        ini_items++;
    }
    
out:
    mp_kfree(ini_buf);
    mp_kfree(tmsection_name);
    return ret;
}

int ilitek_ms_ini_split_u16_array(char * key, u16 * pBuf)
{
    char * s = key;
    char * pToken;
    int nCount = 0;
    int ret;
    long s_to_long = 0;
    
    //s = kmalloc(512, GFP_KERNEL);
    //memset(s, 0, sizeof(*s));
    
    //strcpy(s, key);
    //printk("%s: s = %p, key = %p\n",__func__,s,key);
    //printk("%s: s = %s\n",__func__,s);
    if(isspace_t((int)(unsigned char)*s) == 0) {
        while((pToken = strsep(&s, ",")) != NULL) {
            //printk("%s: pToken = %s\n",__func__,pToken);
            ret = kstrtol(pToken, 0, &s_to_long);
            
            if(ret == 0)
                pBuf[nCount] = s_to_long;
            else
                mp_info(PRINT_MPTEST, "convert string too long, ret = %d\n", ret);
                
            nCount++;
        }
    }
    
    //kfree(s);
    return nCount;
}

int ilitek_ms_ini_split_golden(int *pBuf, int line)
{
    char *pToken = NULL;
    int nCount = 0;
    int ret;
    int num_count = 0;
    long s_to_long = 0;
    char szSection[100] = {0};
    char str[100] = {0};
    char *s = NULL;
    
    while(num_count < line) {
        sprintf(szSection, "Golden_CH_%d", num_count);
        mp_info(PRINT_MPTEST, "szSection = %s \n", szSection);
        ilitek_ms_get_ini_data("RULES", szSection, str);
        s = str;
        
        while((pToken = strsep(&s, ",")) != NULL) {
            ret = kstrtol(pToken, 0, &s_to_long);
            
            if(ret == 0)
                pBuf[nCount] = s_to_long;
            else
                mp_info(PRINT_MPTEST, "convert string too long, ret = %d\n", ret);
                
            nCount++;
        }
        
        num_count++;
    }
    
    return nCount;
}

int ilitek_ms_ini_split_u8_array(char * key, u8 * pBuf)
{
    char * s = key;
    char * pToken;
    int nCount = 0;
    int ret;
    long s_to_long = 0;
    
    //s = kmalloc(512, GFP_KERNEL);
    //memset(s, 0, sizeof(*s));
    
    //strcpy(s, key);
    
    if(isspace_t((int)(unsigned char)*s) == 0) {
        while((pToken = strsep(&s, ".")) != NULL) {
            ret = kstrtol(pToken, 0, &s_to_long);
            
            if(ret == 0)
                pBuf[nCount] = s_to_long;
            else
                mp_info(PRINT_MPTEST, "convert string too long, ret = %d\n", ret);
                
            nCount++;
        }
    }
    
    //kfree(s);
    return nCount;
}

int ilitek_ms_ini_split_int_array(char * key, int * pBuf)
{
    char * s = key;
    char * pToken;
    int nCount = 0;
    int ret;
    long s_to_long = 0;
    
    if(isspace_t((int)(unsigned char)*s) == 0) {
        while((pToken = strsep(&s, ",")) != NULL) {
            ret = kstrtol(pToken, 0, &s_to_long);
            
            if(ret == 0)
                pBuf[nCount] = s_to_long;
            else
                mp_info(PRINT_MPTEST, "convert string too long, ret = %d\n", ret);
                
            nCount++;
        }
    }
    
    //kfree(s);
    return nCount;
}

int ms_ini_get_key(char * section, char * key, char * value)
{
    int i = 0;
    int ret = -1;
    int len = 0;
    len = strlen(key);
    
    for(i = 0; i < ini_items; i++) {
        if(strncmp(section, ms_ini_file_data[i].section_name,
                   ms_ini_file_data[i].section_name_len) != 0)
            continue;
            
        if(strncmp(key, ms_ini_file_data[i].key_name, len) == 0) {
            memcpy(value, ms_ini_file_data[i].key_value, ms_ini_file_data[i].key_value_len);
            //mp_info(PRINT_MPTEST, "key = %s, value:%s\n",ms_ini_file_data[i].key_name, value);
            ret = 0;
            break;
        }
    }
    
    return ret;
}

int ilitek_ms_get_ini_data(char *section, char *ItemName, char *returnValue)
{
    char value[512] = {0};
    int len = 0;
    
    if(returnValue == NULL) {
        mp_err(PRINT_MPTEST,"returnValue as NULL in function\n");
        return 0;
    }
    
    if(ms_ini_get_key(section, ItemName, value) < 0) {
        sprintf(returnValue, "%s", value);
        return 0;
    } else {
        len = sprintf(returnValue, "%s", value);
    }
    
    return len;
}

int ilitek_mp_parse(char *path)
{
    int ret = 0, fsize = 0;
    char *tmp = NULL;
    struct file *f = NULL;
    mm_segment_t old_fs;
    loff_t pos = 0;
    mp_info(PRINT_MPTEST, "path = %s\n", path);
    f = filp_open(path, O_RDONLY, 0);
    
    if(f == NULL) {
        mp_err(PRINT_MPTEST,"Failed to open the file at %ld.\n", PTR_ERR(f));
        return -ENOENT;
    }
    
    fsize = f->f_inode->i_size;
    mp_info(PRINT_MPTEST, "ini size = %d\n", fsize);
    
    if(fsize <= 0) {
        filp_close(f, NULL);
        return -EINVAL;
    }
    
    tmp = kzalloc(fsize + 1, GFP_KERNEL);
    
    if(ERR_ALLOC_MEM(tmp)) {
        mp_err(PRINT_MPTEST,"Failed to allocate ini data \n");
        return -ENOMEM;
    }
    
    /* ready to map user's memory to obtain data by reading files */
    old_fs = get_fs();
    set_fs(get_ds());
    vfs_read(f, tmp, fsize, &pos);
    set_fs(old_fs);
    ms_init_key_data();
    ret = ms_ini_get_key_data(tmp);
    
    if (ret < 0) {
        mp_err(PRINT_MPTEST,"Failed to get physical ini data, ret = %d\n", ret);
        goto out;
    }
    
    mp_info(PRINT_MPTEST, "Parsing INI file doen\n");
out:
    filp_close(f, NULL);
    mp_kfree(tmp);
    return ret;
}