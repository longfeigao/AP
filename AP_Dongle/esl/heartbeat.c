#include "heartbeat.h"
#include "cc2640r2_rf.h"
#include "timer.h"
#include "bsp.h"
#include "debug.h"
#include <string.h>
#include "crc16.h"
#include <stdio.h>
#include <stdlib.h>

static UINT8 hb_timer = 0;
static INT32 _esl_uplink_num = 0;
//timeout: ��λs
UINT8 set_timer(INT32 timeout)
{
	UINT8 timer = 0;
	
	if(timeout != 0)
	{	
		if(timeout > 4294) //0xffFFffFF/1000000
		{
			timeout = 4294;
			pinfo("warning: set_timer timeout change to 4294.\r\n");
		}
		
		if((timer=TIM_Open(timeout*1000, 1, TIMER_UP_CNT, TIMER_ONCE)) == TIMER_UNKNOW)
		{
			perr("set_timer open.\r\n");
		}
	}
	else
	{
		timer = 0;
	}
	
	return timer;
}

UINT8 check_timer_timeout(UINT8 timer)
{
	if(timer > getTimerNum())
	{
		return 0;
	}
	else
	{
		return TIM_CheckTimeout(timer);
	}
}

void close_timer(UINT8 timer)
{
	if(timer < getTimerNum())
	{
		TIM_Close(timer);
		timer = 0;
	}
}

/* 
len == 16
0xc0:g2;
0xf0:old g3;
0x50:old g3 firmware
len == 26
0x80: new e31
*/
static INT32 _check_hb_data(UINT8 *src, UINT8 len)
{
	UINT8 ctrl = src[0] & 0xF0;
	UINT8 id[4] = {0};
	UINT16 read_crc=0, cal_crc=0;
	
	if(len == 16) 
	{
		//need check crc
		memcpy(&read_crc, src+len-2, sizeof(read_crc));
		if(ctrl == 0xC0)
		{
			cal_crc = CRC16_CaculateStepByStep(cal_crc, src, len-2);
			if(cal_crc == read_crc)
			{
				return 0;
			}
		}
		else if((ctrl==0xF0)||(ctrl==0x50)||(ctrl==0xE0))
		{
			memcpy(id, src+5, sizeof(id));
			cal_crc = CRC16_CaculateStepByStep(cal_crc, src, len-2);
			cal_crc = CRC16_CaculateStepByStep(cal_crc, id, sizeof(id));
			if(cal_crc == read_crc)
			{
				return 0;
			}
		}
	}
	else if(len == 26)
	{
		if(ctrl == 0x80)
		{
			return 0;
		}
		
		if(ctrl == 0x70)
		{
			return 1;
		}
	}

	return -1;
}

static INT32 _compare_esl_uplink_info_by_index(const void *info1, const void *info2)
{
	esl_uplink_info_t *p1 = (esl_uplink_info_t *)info1;
	esl_uplink_info_t *p2 = (esl_uplink_info_t *)info2;
	return (p1->index - p2->index);
}

static void _sort_esl_uplink_info_by_index(esl_uplink_info_t *start, INT32 num)
{
	qsort(start, num, sizeof(esl_uplink_info_t), _compare_esl_uplink_info_by_index);
}

static INT32 _compare_esl_uplink_info_by_eslid(const void *info1, const void *info2)
{
	esl_uplink_info_t *p1 = (esl_uplink_info_t *)info1;
	esl_uplink_info_t *p2 = (esl_uplink_info_t *)info2;
	return memcmp(p1->id, p2->id, 4);
}

static esl_uplink_info_t *_get_esl_uplink_info(esl_uplink_info_t *start, INT32 num, UINT8 *eslid)
{
	esl_uplink_info_t key;
	memcpy(key.id, eslid, 4);
	qsort(start, num, sizeof(esl_uplink_info_t), _compare_esl_uplink_info_by_eslid);
	return (esl_uplink_info_t *)bsearch(&key, start, num, sizeof(esl_uplink_info_t), _compare_esl_uplink_info_by_eslid);
}

static esl_uplink_info_t *_append_esl_uplink_info(esl_uplink_info_t *start, INT32 *num, UINT8 *eslid, UINT8 sid)
{
	esl_uplink_info_t *pinfo = NULL;
	INT32 i;
	
	if(*num >= NUM_OF_MAX_ESL_UPLINK_INFO)
	{
		_sort_esl_uplink_info_by_index(start, NUM_OF_MAX_ESL_UPLINK_INFO);
		pinfo = start;
		memcpy(pinfo->id, eslid, 4);
		pinfo->sessionid = sid;
		pinfo->modby = 12;
		pinfo->index = NUM_OF_MAX_ESL_UPLINK_INFO;
		for(i = 0; i < NUM_OF_MAX_ESL_UPLINK_INFO; i++) //update the index
		{
			pinfo->index--;
			pinfo++;
		}
		
		return start;
	}
	else
	{
		pinfo = start + *num;
		memcpy(pinfo->id, eslid, 4);
		pinfo->sessionid = sid;
		pinfo->modby = 12;
		pinfo->index = *num;
		*num = *num + 1;

		return pinfo;
	}
}

static esl_uplink_info_t *_handle_esl_uplink_data(esl_uplink_info_t *start, INT32 *num, UINT8 *data, INT32 len)
{
	UINT8 *eslid = data+1;
	UINT8 sid = *(data+5);
	esl_uplink_info_t *pinfo = _get_esl_uplink_info(start, *num, eslid);
	
	if(pinfo != NULL)
	{
		if(pinfo->sessionid == sid)
		{
			pinfo->modby = pinfo->modby == 6 ? 12 : (pinfo->modby-1);
		}
		else
		{
			pinfo->sessionid = sid;
			pinfo->modby = 12;
		}
	}
	else
	{
		pinfo = _append_esl_uplink_info(start, num, eslid, sid);
	}
	
	log_print("uplink info @ 0x%08X, num = %d\r\n", (int)start, (int)*num);
	phex((UINT8 *)start, sizeof(esl_uplink_info_t)*NUM_OF_MAX_ESL_UPLINK_INFO);
	
	return pinfo;
}

static void _ack_birang(INT32 apid, INT32 modby)
{
	BSP_Delay100US((apid%modby)*4);
}

static INT32 _ack_the_esl(UINT8 *uplink_data, INT32 len, UINT8 status)
{
	UINT8 *eslid = uplink_data+1;
	UINT8 eslch = *(uplink_data+6);
	UINT8 ack_buf[9] = {0};
	UINT16 crc = 0;
	
	ack_buf[0] = uplink_data[0]; //ctrl
	memcpy(ack_buf+1, eslid, 4);//id
	ack_buf[5] = *(uplink_data+5); //session id
	ack_buf[6] = status;
	crc = CRC16_CaculateStepByStep(crc, ack_buf, 7);
	crc = CRC16_CaculateStepByStep(crc, eslid, 4);
	memcpy(ack_buf+7, &crc, sizeof(crc));
	set_power_rate(RF_DEFAULT_POWER, DATA_RATE_500K);
    set_frequence(eslch);
    send_data(eslid, ack_buf, sizeof(ack_buf), 2000);
//	send_data(eslid, ack_buf, sizeof(ack_buf), eslch, 2000);
	//pinfo("_ack_the_esl %02X-%02X-%02X-%02X, %d: ", eslid[0], eslid[1], eslid[2], eslid[3], eslch);
	//phex(ack_buf, sizeof(ack_buf));
	return 0;
}

extern UINT32 core_idel_flag;

static INT32 _hb_recv(g3_hb_table_t *table, UINT8 (*uplink)(UINT8 *src, UINT32 len))
{
	UINT8 *ptr = NULL;
	INT32 recv_len_total = 0, len = 0, ret = 0;
	UINT16 cmd = 0;
	UINT32 cmd_len = 0; //data len + sizeof(num)
	esl_uplink_info_t *pesluplinkinfo = NULL;
	
	pinfo("_hb_recv(), id=0x%02X-0x%02X-0x%02X-0x%02X, ch=%d, recv_datarate=%d, recv_len=%d, interval=%d, timeout=%d, lenout=%d, numout=%d, loop=%d\r\n", \
			table->id[0], table->id[1], table->id[2], table->id[3], table->channel, table->recv_bps, \
			table->recv_len, table->interval, table->timeout, table->lenout, table->numout, table->loop);
	
	/* reset recv buf */
	table->num = 0;
	table->data_len = 8;
	ptr = table->data_buf+8;

	hb_timer = set_timer(table->timeout);
	set_power_rate(RF_DEFAULT_POWER, table->recv_bps);
	rf_preset_hb_recv(true);
	enter_txrx();
	cc2592Cfg(CC2592_RX_HG_MODE);
	while(1)
	{
		if(core_idel_flag == 1)
		{
			pinfo("back to idel\r\n");
			core_idel_flag = 0;
			recv_len_total = -1;
			break;
		}
		if((table->data_len+table->recv_len+1) > G3_HB_BUF_SIZE)
		{
			pinfo("memout\r\n");
			break;
		}
		
		if(((table->data_len+table->recv_len+1)>table->lenout) && (table->lenout!=0))
		{
			pinfo("lenout\r\n");
			break;
		}
		if(check_timer_timeout(hb_timer) == 1)
		{
			pinfo("timeout\r\n");
			break;
		}
			
		if((table->num>=table->numout) && (table->numout!=0))
		{
			pinfo("numout\r\n");
			break;
		}
		
		if(table->num>=60000)
		{
			pinfo("numout 60000\r\n");
			break;
		}
		GPIO_toggleDio(DEBUG_TEST);
		len = recv_data_for_hb(table->id, ptr, table->recv_len, table->channel, 2000000);
		if(len <= 0)
		{
			BSP_Delay1MS(table->interval);
			continue;
		}
		GPIO_toggleDio(DEBUG_TEST);
		len = (len == table->recv_len ? table->recv_len : 16);
		ret = _check_hb_data(ptr, len);
		GPIO_toggleDio(DEBUG_TEST);
		if(ret == 0)
		{
			table->num += 1;
			*(ptr+len) = get_hb_rssi();
			pdebug("_hb_recv() num: %08d:", table->num);
			pdebughex(ptr, len+1);
			ptr += len+1;
			table->data_len += len+1;
			recv_len_total += len+1;
		}
		else if(ret == 1)
		{
			if(table->apid > 0)
			{
				*(ptr+len) = get_rssi();
				break;
			}
			else
			{
				ret = 0;
			}
		}
		BSP_Delay1MS(table->interval);
	}

	close_timer(hb_timer);
	exit_txrx();
	rf_preset_hb_recv(false);
	
	if(ret == 1) //need ack the esl and uplink the data //todo
	{
		pesluplinkinfo = _handle_esl_uplink_data(table->esl_uplink_info, &_esl_uplink_num, ptr, len);
		_ack_birang(table->apid, pesluplinkinfo->modby);
		_ack_the_esl(ptr, len, 0);
		cmd = 0x1021;
		cmd_len = len + 1;
		memcpy(table->uplink_buf, &cmd, sizeof(cmd));
		memcpy(table->uplink_buf+sizeof(cmd), &cmd_len, sizeof(cmd_len));
		memcpy(table->uplink_buf+sizeof(cmd)+sizeof(cmd_len), ptr, cmd_len);
		uplink(table->uplink_buf, sizeof(cmd)+sizeof(cmd_len)+cmd_len);
		pinfo("_hb_recv() send esl uplink\r\n");
		BSP_Delay1MS(100);      //todo
	}
	
	if(recv_len_total >= 0)
	{
		cmd = 0x1020;
		cmd_len = recv_len_total + 2; //data len + sizeof(num)
		memcpy(table->data_buf, (UINT8 *)&cmd, 2);//set cmd
		memcpy(table->data_buf+2, (UINT8 *)&cmd_len, 4);//set len
		memcpy(table->data_buf+6, (UINT8 *)&table->num, 2);//set len
		uplink(table->data_buf, sizeof(cmd)+sizeof(cmd_len)+cmd_len);
		pinfo("_hb_recv() len=%d, num=%d\r\n", recv_len_total, table->num);
	}
	
	return recv_len_total;
}

INT32 common_recv_parse_cmd(UINT8 *pCmd, INT32 cmdLen, common_recv_table_t *table)
{
	UINT8 num = 0;
	UINT8 *ptr = pCmd;
	
	memcpy(table->id, ptr, 4);
	ptr += 4;
	num++;
	
	table->channel = *ptr;
	ptr += 1;
	num++;
	
	memcpy((UINT8 *)&table->recv_bps, ptr, 2);
	ptr += 2;
	num++;
	
	table->recv_len = *ptr;
	ptr += 1;
	num++;
	
	memcpy((UINT8 *)&table->interval, ptr, 4);
	ptr += 4;
	num++;

	memcpy((UINT8 *)&table->timeout, ptr, 4);
	ptr += 4;
	num++;

	memcpy((UINT8 *)&table->lenout, ptr, 4);
	ptr += 4;
	num++;

	memcpy((UINT8 *)&table->numout, ptr, 4);
	ptr += 4;
	num++;
	
	memcpy((UINT8 *)&table->loop, ptr, 4);
	ptr += 4;
	num++;
	
	memcpy((UINT8 *)&table->apid, ptr, 4);
	ptr += 4;
	num++;

//	pdebug("hb table data:");
//	pdebughex((UINT8 *)table, sizeof(g3_hb_table_t));
	
	return num;
}

INT32 heartbeat_mainloop(UINT8 *pCmd, INT32 cmdLen, g3_hb_table_t *table, UINT8 (*uplink)(UINT8 *src, UINT32 len))
{
	common_recv_parse_cmd(pCmd, cmdLen, table);
	_hb_recv(table, uplink);
	return 0;
}

void heartbeat_init()
{
	_esl_uplink_num = 0;
}
