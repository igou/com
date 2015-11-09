/*
 * =====================================================================================
 *
 *       Filename:  app_at_cmd_envelope.c
 *
 *    Description:  envelope function with at cmd implement
 *
 *        Version:  1.0
 *        Created:  2014-1-11 15:56:08
 *       Revision:  none
 *       Compiler:  armcc
 *
 *         Author:  Jumping (apple), zhangping@sim.com
 *   Organization:  www.sim.com
 *
 * =====================================================================================
 */

/* #####   HEADER FILE INCLUDES   ################################################### */
#include "eat_interface.h"
#include "app_at_cmd_envelope.h"
#include "inc.h"

/* #####   MACROS  -  LOCAL TO THIS SOURCE FILE   ################################### */
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define MODEM_READ_BUFFER_LEN 2048
#define AT_CMD_ARRAY_SHORT_LENGTH 30
#define AT_CMD_ARRAY_MID_LENGTH 130
#define AT_CMD_ARRAY_LONG_LENGTH 300

/* #####   TYPE DEFINITIONS  -  LOCAL TO THIS SOURCE FILE   ######################### */

typedef struct UrcEntityQueueTag
{
    UrcEntity urcEntityArray[URC_QUEUE_COUNT]; /*AT命令队列*/
    u8 count;                                 /*当前执行的index*/
}UrcEntityQueue;

typedef struct AtCmdEntityQueueTag
{
    u8 current;                                 /*当前执行的index*/
    u8 last;                                    /*队列中最后一条的index */
    u8 funFirst;                                /*功能模块的第一条*/
    u8 funLast;                                 /*功能模块的最后一条*/
    AtCmdEntity atCmdEntityArray[AT_CMD_QUEUE_COUNT]; /*AT 命令队列*/

}AtCmdEntityQueue;
/* #####   DATA TYPES  -  LOCAL TO THIS SOURCE FILE   ############################### */


/* #####   PROTOTYPES  -  LOCAL TO THIS SOURCE FILE   ############################### */
static AtCmdRsp AtCmdCbDefault(u8* pRspStr);
static AtCmdRsp AtCmdCb_cpin(const u8* pRspStr);
static AtCmdRsp AtCmdCb_creg(const u8* pRspStr);
static AtCmdRsp AtCmdCb_cgatt(const u8* pRspStr);
static AtCmdRsp AtCmdCb_sapbr(u8* pRspStr);
static AtCmdRsp ccalr_cb(const u8* pRspStr);
static AtCmdRsp clvl_cb(const u8* pRspStr);
static AtCmdRsp cmic_cb(const u8* pRspStr);
static AtCmdRsp echo_cb(const u8* pRspStr);

static eat_bool simcom_atcmd_queue_head_out(void);
static eat_bool simcom_atcmd_queue_tail_out(void);
static eat_bool simcom_atcmd_queue_fun_out(void);
static eat_bool AtCmdOverTimeStart(void);
static eat_bool AtCmdOverTimeStop(void);
static void eat_timer1_handler(void);
static void eat_modem_ready_read_handler(void);
//static void global_urc_handler(u8* pUrcStr, u16 len);
static eat_bool AtCmdDelayExe(u16 delay);
//static AtCmdRsp AtCmdCb_ftpgettofs(u8* pRspStr);
//static void UrcCb_FTPGETTOFS(u8* pRspStr, u16 len);

static AtCmdEntityQueue s_atCmdEntityQueue={0};
static u8 modem_read_buf[MODEM_READ_BUFFER_LEN]; 
static u8* s_atCmdWriteAgain = NULL;
static u8 s_atExecute = 0;
static ResultNotifyCb p_gsmInitCb = NULL;
static ResultNotifyCb p_gprsBearCb = NULL;
static ResultNotifyCb p_get2fsCb = NULL;
static ResultNotifyCb p_get2fsFinalUserCb = NULL;

/* #####   FUNCTION DEFINITIONS  -  EXPORTED FUNCTIONS   ############################ */
static AtCmdRsp playback_cb(u8 *pRspStr);
// CSCS
eat_bool simcom_gsm_init(u8* cpin,ResultNotifyCb pResultCb)
{
   #define ATCMIC  "AT+CMIC=0,7"AT_CMD_END
   #define ATECHO  "AT+ECHO=0,96,253,16388,20488"AT_CMD_END
    eat_bool result = FALSE;
    AtCmdEntity atCmdInit[]={
        {"AT"AT_CMD_END,4,NULL},
        {"AT"AT_CMD_END,4,NULL},
        {AT_CMD_DELAY"2000",10,NULL}, 
        {"AT+CREG=2"AT_CMD_END,11,NULL},//开启网络自动上报
        {"AT+CLCC=1"AT_CMD_END,11,NULL},
  /*under below: DO NOT REMOVE OR ADD*/
        {"AT+CPIN?"AT_CMD_END,10,AtCmdCb_cpin},
        {"AT+CPIN=",0,NULL},//TODO:此at的下标变化，必须改动下面代码atCmdInit[6].p_atCmdStr，将改动以更好的可移植性
        {AT_CMD_DELAY"5000",10,NULL},
        {"AT+CREG?"AT_CMD_END,10,AtCmdCb_creg},
        {AT_CMD_DELAY"2000",10,NULL},
  /*above: DO NOT REMOVE OR ADD */
        {"AT+CCALR?"AT_CMD_END,11,ccalr_cb},
        {"AT+CPBS=\"SM\""AT_CMD_END,14, NULL},
        {"AT+CPBF"AT_CMD_END,9, pb_cb},
        {"AT+CLVL=75"AT_CMD_END,12,clvl_cb},
        {ATCMIC,sizeof(ATCMIC)-1,cmic_cb},
        {ATECHO,sizeof(ATECHO)-1,echo_cb},
        {AT_CMD_DELAY"2000",10,NULL},/* DO NOT REMOVE */
        {"AT+CGATT?"AT_CMD_END,11,AtCmdCb_cgatt},
    };
    u8 pAtCpin[15] = {0};
    sprintf(pAtCpin,"AT+CPIN=%s%s",cpin,AT_CMD_END);
    atCmdInit[6].p_atCmdStr = pAtCpin;
    atCmdInit[6].cmdLen = strlen(pAtCpin);
    result = simcom_atcmd_queue_fun_append(atCmdInit,sizeof(atCmdInit) / sizeof(atCmdInit[0]));
    if(result)
        p_gsmInitCb = pResultCb;
    return result;
}
/* 
 * ===  FUNCTION  ======================================================================
 *         Name: simcom_atcmd_queue_init
 *  Description: initial at cmd queue 
 *        Input:
 *       Output:
 *       Return:
 *               void::
 *       author: Jumping create at 2014-1-13
 * =====================================================================================
 */
void simcom_atcmd_queue_init(void)
{
    u8 i = 0;
    u8 first = MIN(s_atCmdEntityQueue.current,s_atCmdEntityQueue.funFirst);
    for ( i=first; i<=s_atCmdEntityQueue.last; i++) {
        AtCmdEntity* atCmdEnt = NULL;
        atCmdEnt = &(s_atCmdEntityQueue.atCmdEntityArray[i]);
        if(atCmdEnt->p_atCmdStr){
            eat_mem_free(atCmdEnt->p_atCmdStr);
            atCmdEnt->p_atCmdStr = NULL;
        }
        atCmdEnt->p_atCmdCallBack = NULL;
    }
    memset(&s_atCmdEntityQueue,0,sizeof(AtCmdEntityQueue));
}
/* 
 * ===  FUNCTION  ======================================================================
 *         Name: simcom_atcmd_queue_append
 *  Description: append a entity at the last of at cmd queue 
 *        Input:
 *               atCmdEntity:: at cmd entity
 *       Output:
 *       Return:
 *               FALSE: some error is ocur
 *               TRUE: append success
 *       author: Jumping create at 2014-1-13
 * =====================================================================================
 */
eat_bool simcom_atcmd_queue_append(AtCmdEntity atCmdEntity)
{
                                                /* get first index */
    u8 first =  MAX(s_atCmdEntityQueue.current,s_atCmdEntityQueue.funFirst);

    if (atCmdEntity.p_atCmdStr == NULL){
        eat_trace("ERROR: at cmd str is null!");
        return FALSE;
    }

    if (atCmdEntity.p_atCmdCallBack == NULL)    /* set default callback function */
        atCmdEntity.p_atCmdCallBack = AtCmdCbDefault;

    if((s_atCmdEntityQueue.last + 1) % AT_CMD_QUEUE_COUNT == first){
        eat_trace("ERROR: at cmd queue is full!");
        return FALSE;                           /* the queue is full */
    }
    else{
        u8* pAtCmd = NULL; ;//= eat_mem_alloc(atCmdEntity.cmdLen);

        pAtCmd = eat_mem_alloc(atCmdEntity.cmdLen);

        if (!pAtCmd){
            eat_trace("ERROR: memory alloc error!");
            return FALSE;
        }

        memcpy(pAtCmd,atCmdEntity.p_atCmdStr,atCmdEntity.cmdLen);
        s_atCmdEntityQueue.atCmdEntityArray[s_atCmdEntityQueue.last].cmdLen = atCmdEntity.cmdLen;
        s_atCmdEntityQueue.atCmdEntityArray[s_atCmdEntityQueue.last].p_atCmdStr = pAtCmd;
        s_atCmdEntityQueue.atCmdEntityArray[s_atCmdEntityQueue.last].p_atCmdCallBack = atCmdEntity.p_atCmdCallBack;

        if(simcom_atcmd_queue_is_empty() && s_atExecute == 0) /* add first at cmd and execute it */
            eat_send_msg_to_user(0, EAT_USER_1, EAT_FALSE, 1, 0, NULL);

        s_atCmdEntityQueue.last = (s_atCmdEntityQueue.last + 1) %  AT_CMD_QUEUE_COUNT;
        return TRUE;
    }
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name: simcom_atcmd_queue_is_empty
 *  Description: judge that the queue is empty 
 *        Input:
 *       Output:
 *       Return:
 *               eat_bool::
 *       author: Jumping create at 2014-1-13
 * =====================================================================================
 */
eat_bool simcom_atcmd_queue_is_empty()
{
    return (s_atCmdEntityQueue.current == s_atCmdEntityQueue.last);
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name: simcom_atcmd_queue_fun_set
 *  Description: set at cmd func's range,from current id 
 *        Input:
 *               count:: the count of this at cmd func
 *       Output:
 *       Return:
 *               eat_bool::
 *       author: Jumping create at 2014-1-17
 * =====================================================================================
 */
eat_bool simcom_atcmd_queue_fun_set(u8 funCount)
{
    u8 i = 0;
    u8 first =  MAX(s_atCmdEntityQueue.current,s_atCmdEntityQueue.funFirst);
    u8 freeCount = 0;
    if (s_atCmdEntityQueue.funLast != s_atCmdEntityQueue.funFirst){
        eat_trace("ERROR: fun is exist!");
        return FALSE;                           /* just one func exist */
    }

    if(simcom_atcmd_queue_is_empty()){
        freeCount = AT_CMD_QUEUE_COUNT;
    }
    else {
        freeCount = (first - s_atCmdEntityQueue.last + AT_CMD_QUEUE_COUNT) % AT_CMD_QUEUE_COUNT;
    }
    if(funCount > freeCount) {
        eat_trace("ERROR: the queue is full! %d,%d,%d",first,funCount,freeCount);
        return FALSE;                           /* the space is poor */
    }

    s_atCmdEntityQueue.funFirst = s_atCmdEntityQueue.last;
    s_atCmdEntityQueue.funLast = (s_atCmdEntityQueue.last + funCount - 1 + AT_CMD_QUEUE_COUNT) % AT_CMD_QUEUE_COUNT;
    eat_trace("INFO: the at cmd func's range is %d-%d", s_atCmdEntityQueue.funFirst, s_atCmdEntityQueue.funLast);
    return TRUE;
}
/* 
 * ===  FUNCTION  ======================================================================
 *         Name: simcom_atcmd_queue_fun_append
 *  Description: add a group of at cmd to queue 
 *        Input:
 *               atCmdEntity[]:: at cmd group, can implemnt a function
 *       Output:
 *       Return:
 *               eat_bool::
 *       author: Jumping create at 2014-1-13
 * =====================================================================================
 */
eat_bool simcom_atcmd_queue_fun_append(AtCmdEntity atCmdEntity[],u8 funCount)
{
    u8 i = 0;
    eat_bool ret = FALSE;
    ret = simcom_atcmd_queue_fun_set(funCount);
    if(!ret)
        return FALSE;

    for ( i=0; i<funCount; i++) {
        eat_bool ret = FALSE;
        ret = simcom_atcmd_queue_append(atCmdEntity[i]);
        if (!ret){
            break;
        }
    }

    if(i != funCount){                           /* error is ocur */
        for ( i=funCount; i > 0; i--) {
            simcom_atcmd_queue_tail_out();
        }
        eat_trace("ERROR: add to queue is error!,%d",funCount);
        return FALSE;
    }

    eat_trace("INFO: funFirst:%d  funLast:%d", s_atCmdEntityQueue.funFirst, s_atCmdEntityQueue.funLast);
    return TRUE;
}


/* 
 * ===  FUNCTION  ======================================================================
 *         Name: app_at_cmd_envelope
 *  Description: at cmd deal with thread 
 *        Input:
 *               data::
 *       Output:
 *       Return:
 *               void::
 *       author: Jumping create at 2014-1-13
 * =====================================================================================
 */
void app_at_cmd_envelope(void *data)
{
    EatEvent_st event;
    while(EAT_TRUE)
    {
        eat_get_event_for_user(EAT_USER_1, &event);
        switch(event.event)
        {
            case EAT_EVENT_TIMER :
                
                switch ( event.data.timer.timer_id ) {
                    case EAT_TIMER_1:           /* dealy to excute at cmd */
                        eat_trace("INFO: EAT_TIMER_1 expire!");
                        eat_timer1_handler();
                        break;

                    case EAT_TIMER_2:           /* at cmd time over */
                        eat_trace("FATAL: at cmd rsp is overtime");
                        break;

                    default:	
                        break;
                }				/* -----  end switch  ----- */
                break;

            case EAT_EVENT_MDM_READY_RD:
                eat_modem_ready_read_handler();
                break;

            case EAT_EVENT_MDM_READY_WR:
                if (s_atCmdWriteAgain){
                    u16 lenAct = 0;
                    u16 len = strlen(s_atCmdWriteAgain);
                    lenAct = eat_modem_write(s_atCmdWriteAgain,len);
                    eat_mem_free(s_atCmdWriteAgain);
                    if(lenAct<len){
                        eat_trace("FATAL: modem write buffer is overflow!");
                    }

                }
                break;
            case EAT_EVENT_USER_MSG:
                AtCmdDelayExe(0);
                break;
            default:
                break;

        }
    }
}

/* #####   FUNCTION DEFINITIONS  -  LOCAL TO THIS SOURCE FILE   ##################### */

/* 
 * ===  FUNCTION  ======================================================================
 *         Name: AtCmdCbDefault
 *  Description: default function to deal with at cmd RSP code 
 *        Input:
 *               pRspStr::
 *       Output:
 *       Return:
 *               AtCmdRsp::
 *       author: Jumping create at 2014-1-13
 * =====================================================================================
 */
static AtCmdRsp AtCmdCbDefault(u8* pRspStr)
{
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    u8 *rspStrTable[ ] = {"OK","ERROR"};
    s16  rspType = -1;
    u8  i = 0;
    u8  *p = pRspStr;
    while(p) {
        /* ignore \r \n */
        while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
        {
            p++;
        }

        for (i = 0; i < sizeof(rspStrTable) / sizeof(rspStrTable[0]); i++)
        {
            if (!strncmp(rspStrTable[i], p, strlen(rspStrTable[i])))
            {
                rspType = i;
                break;
            }
        }

        p = (u8*)strchr(p,0x0a);
    }

    switch (rspType)
    {
        case 0:  /* OK */
        rspValue  = AT_RSP_CONTINUE;
        break;

        case 1:  /* ERROR */
        rspValue = AT_RSP_ERROR;
		if(p_get2fsCb) {
			p_get2fsCb(EAT_FALSE);
			p_get2fsCb = NULL;
		}

		if(p_gsmInitCb) {
			p_gsmInitCb(EAT_FALSE);
			p_gsmInitCb = NULL;
		}

		if(p_gprsBearCb) {
			p_gprsBearCb (EAT_FALSE);
			p_gprsBearCb = NULL;
		}
        break;

        default:
        rspValue = AT_RSP_WAIT;
        break;
    }

    return rspValue;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name: AtCmdDelayExe
 *  Description: dealy 10ms to execute at cmd 
 *        Input:
 *       Output:
 *       Return:
 *               eat_bool::
 *       author: Jumping create at 2014-1-13
 * =====================================================================================
 */
static eat_bool AtCmdDelayExe(u16 delay)
{
    eat_bool ret = FALSE;
    eat_trace("INFO: at cmd delay execute,%d",delay);
    if (delay == 0){
        delay = AT_CMD_EXECUTE_DELAY;
    }
    ret = eat_timer_start(EAT_TIMER_1,delay);
    if (ret)
        s_atExecute = 1;
    return ret;
}

static eat_bool AtCmdOverTimeStart(void)
{
    eat_bool ret = FALSE;
    ret = eat_timer_start(EAT_TIMER_2,AT_CMD_EXECUTE_OVERTIME);
    return ret;
}

static eat_bool AtCmdOverTimeStop(void)
{
    eat_bool ret = FALSE;
    ret = eat_timer_stop(EAT_TIMER_2);
    return ret;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name: simcom_atcmd_queue_head_out
 *  Description: delete the head of the queue 
 *        Input:
 *       Output:
 *       Return:
 *               eat_bool::
 *       author: Jumping create at 2014-1-13
 * =====================================================================================
 */
static eat_bool simcom_atcmd_queue_head_out(void)
{
    AtCmdEntity* atCmdEnt = NULL;

   if(s_atCmdEntityQueue.current == s_atCmdEntityQueue.last) /* the queue is empty */
       return FALSE;

   if(s_atCmdEntityQueue.current>=s_atCmdEntityQueue.funLast
           || s_atCmdEntityQueue.current<=s_atCmdEntityQueue.funFirst){
       atCmdEnt = &(s_atCmdEntityQueue.atCmdEntityArray[s_atCmdEntityQueue.current]);
       if(atCmdEnt->p_atCmdStr){
           eat_mem_free(atCmdEnt->p_atCmdStr);
           atCmdEnt->p_atCmdStr = NULL;
       }
       atCmdEnt->p_atCmdCallBack = NULL;
   }
   s_atCmdEntityQueue.current = (s_atCmdEntityQueue.current + 1) %  AT_CMD_QUEUE_COUNT;
   return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name: simcom_atcmd_queue_tail_out
 *  Description: delete the tail of the queue 
 *        Input:
 *       Output:
 *       Return:
 *               eat_bool::
 *       author: Jumping create at 2014-1-13
 * =====================================================================================
 */
static eat_bool simcom_atcmd_queue_tail_out(void)
{
    AtCmdEntity* atCmdEnt = NULL;
    if(s_atCmdEntityQueue.current == s_atCmdEntityQueue.last)
        return FALSE;
    s_atCmdEntityQueue.last = (s_atCmdEntityQueue.last + AT_CMD_QUEUE_COUNT - 1) % AT_CMD_QUEUE_COUNT;
    atCmdEnt = &(s_atCmdEntityQueue.atCmdEntityArray[s_atCmdEntityQueue.last]);
    if(atCmdEnt->p_atCmdStr){
        eat_mem_free(atCmdEnt->p_atCmdStr);
        atCmdEnt->p_atCmdStr = NULL;
    }
    atCmdEnt->p_atCmdCallBack = NULL;
    return TRUE;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name: simcom_atcmd_queue_fun_out
 *  Description: delete a group of at cmd from queue 
 *        Input:
 *       Output:
 *       Return:
 *               eat_bool::
 *       author: Jumping create at 2014-1-13
 * =====================================================================================
 */
static eat_bool simcom_atcmd_queue_fun_out(void)
{
    u8 i = 0;
   // if (s_atCmdEntityQueue.funLast == 0)        /* no at cmd func */
   //     return FALSE;  //注释掉这两行

    
    for ( i=s_atCmdEntityQueue.funFirst ;i<=s_atCmdEntityQueue.funLast ; i++) {
        AtCmdEntity* atCmdEnt = NULL;
        atCmdEnt = &(s_atCmdEntityQueue.atCmdEntityArray[i]);
        if(atCmdEnt->p_atCmdStr){
            eat_mem_free(atCmdEnt->p_atCmdStr);
            atCmdEnt->p_atCmdStr = NULL;
        }
        atCmdEnt->p_atCmdCallBack = NULL;
    }

    if(s_atCmdEntityQueue.current != s_atCmdEntityQueue.funLast){
        eat_trace("WARNING: at cmd func over did not at last!");
        s_atCmdEntityQueue.current = s_atCmdEntityQueue.funLast;
    }

    s_atCmdEntityQueue.current = (s_atCmdEntityQueue.current + 1) % AT_CMD_QUEUE_COUNT;
    s_atCmdEntityQueue.funLast = 0;
    s_atCmdEntityQueue.funFirst = 0;
    return TRUE;

}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name: eat_timer1_handler
 *  Description: 1 timer timeout handler
 *        Input:
 *       Output:
 *       Return:
 *               void::
 *       author: Jumping create at 2014-1-14
 * =====================================================================================
 */
static void eat_timer1_handler(void)
{
    u8* pCmd = s_atCmdEntityQueue.atCmdEntityArray[s_atCmdEntityQueue.current].p_atCmdStr;
    u16 len = s_atCmdEntityQueue.atCmdEntityArray[s_atCmdEntityQueue.current].cmdLen;
    s_atExecute = 0;

    if(pCmd){
        u16 lenAct = 0;
        eat_trace("DEBUG:at cmd is:%d,%s",s_atCmdEntityQueue.current,pCmd);
        if (!strncmp(AT_CMD_DELAY, pCmd, strlen(AT_CMD_DELAY))) {  /* delay some seconds to run next cmd */
            u32 delay;
            sscanf(pCmd,AT_CMD_DELAY"%d",&delay);
            simcom_atcmd_queue_head_out();
            AtCmdDelayExe(delay);
            return;
        }
        lenAct = eat_modem_write(pCmd,len);
        if(lenAct<len){
            eat_trace("ERROR: modem write buffer is overflow!");

            if(s_atCmdWriteAgain == NULL){
                s_atCmdWriteAgain = eat_mem_alloc(len-lenAct);
                if (s_atCmdWriteAgain)
                    memcpy(s_atCmdWriteAgain,pCmd+lenAct,len-lenAct);
                else
                    eat_trace("ERROR: mem alloc error!");
            }
            else 
                eat_trace("FATAL: EAT_EVENT_MDM_READY_WR may be lost!");
        }
        else{
            eat_trace("WrToModem:%s",pCmd);
            AtCmdOverTimeStart();
        }

    }
}
#if 0
/* 
 * ===  FUNCTION  ======================================================================
 *         Name: global_urc_handler
 *  Description: global urc check 
 *        Input:
 *               pUrcStr::
 *               len::
 *       Output:
 *       Return:
 *               void::
 *       author: Jumping create at 2014-1-14
 * =====================================================================================
 */
static void global_urc_handler(u8* pUrcStr, u16 len)
{
    u8  i = 0;
    u8  *p = pUrcStr;
    while(p) {
        /* ignore \r \n */
        while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
        {
            p++;
        }

        for (i = 0; i < s_urcEntityQueue.count; i++)
        {
            if (!strncmp(s_urcEntityQueue.urcEntityArray[i].p_urcStr, p, strlen(s_urcEntityQueue.urcEntityArray[i].p_urcStr))) {
                if(s_urcEntityQueue.urcEntityArray[i].p_urcCallBack)
                    s_urcEntityQueue.urcEntityArray[i].p_urcCallBack(p,len);
            }
        }

        p = (u8*)memchr(p,0x0a,0);
    }

}
#endif
/* 
 * ===  FUNCTION  ======================================================================
 *         Name: eat_modem_ready_read_handler
 *  Description: read data handler from modem 
 *        Input:
 *       Output:
 *       Return:
 *               void::
 *       author: Jumping create at 2014-1-14
 * =====================================================================================
 */
static void incoming(u8 *str);
static void eat_modem_ready_read_handler(void)
{
    u16 len = eat_modem_read(modem_read_buf,MODEM_READ_BUFFER_LEN);
    if (len > 0) {
        AtCmdRsp atCmdRsp = AT_RSP_ERROR;
        memset(modem_read_buf+len,0,MODEM_READ_BUFFER_LEN-len);
                                                /* global urc check */
//        global_urc_handler(modem_read_buf,len);
                                                /* at cmd rsp check */
        eat_trace("ReFrModem:%s,%d",modem_read_buf,len);
        if (s_atCmdEntityQueue.atCmdEntityArray[s_atCmdEntityQueue.current].p_atCmdCallBack){
            atCmdRsp = s_atCmdEntityQueue.atCmdEntityArray[s_atCmdEntityQueue.current].p_atCmdCallBack(modem_read_buf);

            eat_trace("INFO: callback return %d,%d",s_atCmdEntityQueue.current,atCmdRsp);
            switch ( atCmdRsp ) {
                case AT_RSP_ERROR:
                    eat_trace("ERROR: at cmd execute error, initial at cmd queue!");
                    simcom_atcmd_queue_init();
                    AtCmdOverTimeStop();
                    at_error = EAT_TRUE;
                    INCOMING_VALID();
                    eat_trace("pin 66 +handlererror");
                    crec(STR5,AT_END); /*异常语音提示*/  
                    break;
                case AT_RSP_CONTINUE:	
                case AT_RSP_FINISH:	
                    simcom_atcmd_queue_head_out();
                    AtCmdDelayExe(0);                        
                    AtCmdOverTimeStop();
                    break;

                case AT_RSP_FUN_OVER:	
                    simcom_atcmd_queue_fun_out();
                    AtCmdDelayExe(0);
                    AtCmdOverTimeStop();
                    break;

                case AT_RSP_WAIT:
                    break;

                default:	
                    if(atCmdRsp>=AT_RSP_STEP_MIN && atCmdRsp<=AT_RSP_STEP_MAX) {
                        s8 step = s_atCmdEntityQueue.current + atCmdRsp - AT_RSP_STEP;
                        eat_trace("DEBUG: cur %d,step %d",s_atCmdEntityQueue.current,step);
                        if(step<=s_atCmdEntityQueue.funLast && step>= s_atCmdEntityQueue.funFirst){
                            s_atCmdEntityQueue.current = step;
                            AtCmdDelayExe(0);
                            AtCmdOverTimeStop();
                        }
                        else{
                            eat_trace("ERROR: return of AtCmdRsp is error!");
                        }
                    }
                    break;
            }				/* -----  end switch  ----- */
        }         
        /* ADD BEGIN */
        else
        {
          u8  *p = modem_read_buf;
          u8  clcc[]="+CLCC: ";
          u8  creg[]="+CREG:";
          p = (u8*)strchr(p,'+');//add 12_25
          while(p) {
          /* ignore \r \n */
             while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
             {
                 p++;
             }        
             if (!strncmp(p,clcc,strlen(clcc))&&(POWER_ON < get_cal_status()))
             {  
                eat_trace("[function]%s():\"+CLCC\"->[Matched]",__FUNCTION__);
                incoming(modem_read_buf);//若来电，则自动接听      
             }
             else if (!strncmp(p,creg,strlen(creg)))
             { 
                /*+CREG: 1,"1816","F9A1" */
                u8 stat = 0;
                eat_trace("[function]%s():\"+CREG: \"->[Matched]",__FUNCTION__);
                sscanf(p,"+CREG: %d",&stat);
                eat_trace("network status report: %d",stat);
                switch(stat)
                {
                    case 1:    /* Registered, home network */
                    case 5:    /* Registered, roaming      */
                      NETWORK_VALID();
                      network_error = EAT_FALSE;
                      break;
                      
                    default:
                      NETWORK_INVALID();
                      simcom_atcmd_queue_init();
                      network_error = EAT_TRUE;
                      INCOMING_VALID();
                      crec(STR5,AT_END);                    
                      break;
                }
             }
             p = (u8*)strchr(p,AT_CMD_LF);
          }
        }
        /* ADD END */
    }
}

static AtCmdRsp AtCmdCb_cpin(const u8* pRspStr)
{
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    u8 *rspStrTable[ ] = {"ERROR","+CPIN: READY","+CPIN: SIM PIN","+CPIN: "};
    s16  rspType = -1;
    u8  i = 0;
    const u8  *p = pRspStr;
    while(p) {
        /* ignore \r \n */
        while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
        {
            p++;
        }

        for (i = 0; i < sizeof(rspStrTable) / sizeof(rspStrTable[0]); i++)
        {
            if (!strncmp(rspStrTable[i], p, strlen(rspStrTable[i])))
            {
                rspType = i;
                break;
            }
        }

        p = (u8*)strchr(p,0x0a);
    }

    switch (rspType)
    {

        case 0:  /* ERROR */
        rspValue = AT_RSP_ERROR;
        break;

        case 1:  /* READY */
        rspValue  = AT_RSP_STEP+3;
        break;

        case 2:  /* need sim pin */
        rspValue  = AT_RSP_STEP+1;
        break;

        default:
        break;
    }

    if(rspValue == AT_RSP_ERROR){
        eat_trace("at+cpin? return AT_RSP_ERROR");
		if(p_gsmInitCb) {
			p_gsmInitCb(EAT_FALSE);
			p_gsmInitCb = NULL;
		}
    }
    
    return rspValue;
}

static AtCmdRsp AtCmdCb_creg(const u8* pRspStr)
{
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    u8 *rspStrTable[ ] = {"ERROR","+CREG: "};
    s16  rspType = -1;
    u8  n = 0;
    u8  stat = 0;
    u8  i = 0;
    const u8  *p = pRspStr;
    static count = 0;
    while(p) {
        /* ignore \r \n */
        while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
        {
            p++;
        }

        for (i = 0; i < sizeof(rspStrTable) / sizeof(rspStrTable[0]); i++)
        {
            if (!strncmp(rspStrTable[i], p, strlen(rspStrTable[i])))
            {
                rspType = i;
                if (rspType == 1){
                    sscanf(p+strlen(rspStrTable[rspType]),"%d,%d",&n,&stat);
                }
                break;
            }
        }

        p = (u8*)strchr(p,0x0a);
    }

    switch (rspType)
    {

        case 0:  /* ERROR */
        rspValue = AT_RSP_ERROR;
        break;

        case 1:  /* +CREG */
        if(1 == stat){                          /* registered */
            rspValue  = AT_RSP_STEP+2;
            NETWORK_VALID();
        }
        else if(2 == stat){                     /* searching */
            if (count == 10){
                eat_trace("at+creg? return AT_RSP_ERROR");
                rspValue = AT_RSP_ERROR;
            }
            else {
                rspValue  = AT_RSP_STEP-1;
                count++;
            }
        }
        else {
            if(stat<=5&&stat>=3)
                rspValue = AT_RSP_ERROR;
            else 
  // +CREG: 1,"1816","F2C3"网络会在某个时候自动上报，有可能解析到stat=",误作为error
                rspValue = AT_RSP_WAIT;
            }
        break;

        default:
        break;
    }

    eat_trace("at+creg? return %d",rspValue);
    if(rspValue == AT_RSP_ERROR){
		if(p_gsmInitCb) {
			p_gsmInitCb(EAT_FALSE);
			p_gsmInitCb = NULL;
		}
    }

    return rspValue;
}
static AtCmdRsp AtCmdCb_cgatt(const u8* pRspStr)
{
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    u8 *rspStrTable[ ] = {"ERROR","+CGATT: 1","+CGATT: 0"};
    s16  rspType = -1;
    u8  i = 0;
    const u8  *p = pRspStr;
    static count = 0;
    while(p) {
        /* ignore \r \n */
        while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
        {
            p++;
        }

        for (i = 0; i < sizeof(rspStrTable) / sizeof(rspStrTable[0]); i++)
        {
            if (!strncmp(rspStrTable[i], p, strlen(rspStrTable[i])))
            {
                rspType = i;
                break;
            }
        }

        p = (u8*)strchr(p,AT_CMD_LF);
    }

    switch (rspType)
    {

#if 0
        case 0:  /* ERROR */
        rspValue = AT_RSP_ERROR;
        break;

        case 1:  /* attached */
        rspValue  = AT_RSP_FUN_OVER;
        eat_trace("at+cgatt? return AT_RSP_OK");
        set_cal_status(WAIT_CALL);  //开机准备完成，可以接受按键
        eat_trace("buttom is avaiable");
        if(p_gsmInitCb) {
            p_gsmInitCb(TRUE);
			p_gsmInitCb = NULL;
        }
        break;

        case 2:  /* detached */
        if (count == 5){
            eat_trace("at+cgatt? return AT_RSP_ERROR");
            rspValue = AT_RSP_ERROR;
            if(p_gsmInitCb){
                p_gsmInitCb(FALSE);
                p_gsmInitCb = NULL;
            }
        }
        else
            rspValue  = AT_RSP_STEP-1;

        count ++;
        break;

#else /*2015,5,19  cgatt报错，只打电话，可以不关心网络附着*/
        case 0:
        case 1:  
        case 2:
        rspValue  = AT_RSP_FUN_OVER;
        log_0("at+cgatt? return AT_RSP_OK");
        set_cal_status(WAIT_CALL);  //开机准备完成，可以接受按键
        log_2("buttom is avaiable");
        if(p_gsmInitCb) {
            p_gsmInitCb(TRUE);
			p_gsmInitCb = NULL;
        }
        break;
#endif
        default:
        rspValue = AT_RSP_WAIT;
        break;
    }

    return rspValue;
}

/*##########################################################*/
/*#####################cpbf 返回值回调函数#########################*/
/*用于查询PBH和NET是否好了
 *该函数会一直查询，直到可以读取电话本
 * AT+CCALR? +CCALR: 1  OK 一般会一次性全部回显返回
 */
static AtCmdRsp ccalr_cb(const u8* pRspStr)
{
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    s16 rspType = -1;
    const u8 *p = pRspStr;
    u8  i;
    u8  *rspStrTable[] = {"+CCALR: ",}; // +CCALR: n    (0,1)
    s8  n = -1;
    log_2("[function] %s()",__FUNCTION__);
    while(p) {
       /* ignore \r \n */
       while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
       {
           p++;
       }
//      p = strstr(p,rspStrTable[0]);
       for (i = 0; i < sizeof(rspStrTable) / sizeof(rspStrTable[0]); i++)
             {
                 if (!strncmp(rspStrTable[i], p, strlen(rspStrTable[i])))
                 {                                                       
                    sscanf(p,"+CCALR: %d",&n); 
                    break;
                 }
             }    
       p = (u8*)strchr(p,AT_CMD_LF);
    }
    
    switch (n)
    {
      case 0:
        rspValue = AT_RSP_STEP;//重复执行该条指令
        break;
        
      case 1:
        rspValue = AT_RSP_CONTINUE;
        break;
        
      default:
        rspValue = AT_RSP_WAIT;
        break;
    }
    return rspValue;
}
/*用于初始化通话语音*/
static AtCmdRsp clvl_cb(const u8* pRspStr) 
{ /* AT+CLVL=90 -> OK*/
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    s16 rspType = -1;
    const u8 *p = pRspStr;
    u8  i;
    u8  *rspStrTable[] = {"OK","ERROR"};
    log_2("[function] %s()",__FUNCTION__);
    while(p) {
       /* ignore \r \n */
       while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
       {
           p++;
       }       
       for (i = 0; i < sizeof(rspStrTable) / sizeof(rspStrTable[0]); i++)
             {
                 if (!strncmp(rspStrTable[i], p, strlen(rspStrTable[i])))
                 {
                     rspType = i;
                     break;
                 }
             }    
       p = (u8*)strchr(p,AT_CMD_LF);
    }
    switch (rspType)
    {
      case 0:
        rspValue = AT_RSP_CONTINUE;/*该AT是在simcom_gsm_init()中使用，ok还要继续执行其他AT命令*/
        break;
        
      case 1:
        rspValue = AT_RSP_ERROR;
        break;
        
      default:
        rspValue = AT_RSP_WAIT;
        break;
    }
    return rspValue;
}
static AtCmdRsp cmic_cb(const u8* pRspStr) 
{
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    s16 rspType = -1;
    const u8 *p = pRspStr;
    u8  i;
    u8  *rspStrTable[] = {"OK","ERROR"};
    log_2("[function] %s()",__FUNCTION__);
    while(p) {
      /* ignore \r \n */
      while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
      {
          p++;
      }       
      for (i = 0; i < sizeof(rspStrTable) / sizeof(rspStrTable[0]); i++)
            {
                if (!strncmp(rspStrTable[i], p, strlen(rspStrTable[i])))
                {
                    rspType = i;
                    break;
                }
            }    
      p = (u8*)strchr(p,AT_CMD_LF);
    }
    switch (rspType)
    {
     case 0:
       rspValue = AT_RSP_CONTINUE;/*该AT是在simcom_gsm_init()中使用，ok还要继续执行其他AT命令*/
       break;
       
     case 1:
       rspValue = AT_RSP_ERROR;
       break;
       
     default:
       rspValue = AT_RSP_WAIT;
       break;
    }
    return rspValue;
}
static AtCmdRsp echo_cb(const u8* pRspStr)
{
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    s16 rspType = -1;
    const u8 *p = pRspStr;
    u8  i;
    u8  *rspStrTable[] = {"OK","ERROR"};
    log_2("[function] %s()",__FUNCTION__);
    while(p) {
      /* ignore \r \n */
      while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
      {
          p++;
      }       
      for (i = 0; i < sizeof(rspStrTable) / sizeof(rspStrTable[0]); i++)
            {
                if (!strncmp(rspStrTable[i], p, strlen(rspStrTable[i])))
                {
                    rspType = i;
                    break;
                }
            }    
      p = (u8*)strchr(p,AT_CMD_LF);
    }
    switch (rspType)
    {
     case 0:
       rspValue = AT_RSP_CONTINUE;
       break;
       
     case 1:
       rspValue = AT_RSP_ERROR;
       break;
       
     default:
       rspValue = AT_RSP_WAIT;
       break;
    }
    return rspValue;
}

eat_bool get_incoming_num(u8 * pRspStr,u8 *phnum)
{  //+CLCC: 1,1,4,0,0,"PHONE NUMBER",129,"A"
    u8  *p = pRspStr;
    u8  i,clcc[]="+CLCC: ";
    eat_bool ret = EAT_FALSE;
    log_2("%s()",__FUNCTION__);
    while(p) {
        /* ignore \r \n */
        while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
        {
            p++;
        }        
        if (!strncmp(p,clcc,strlen(clcc)))
        {  
           u8 *s1,*s2;
           eat_trace("\"+CLCC\"->[Matched]");     
           /*+CPBF: 5,"12324434343",129,"E"*/
           s1 = (u8*)strchr(p,'\"');  /* s1->第1个'"';s2->第2个'"'*/          
           s2 = (u8*)strchr(s1+1,'\"'); 
           strncpy(phnum,s1+1,(u32)(s2-s1-1));
           return ret=EAT_TRUE;
        }
        p = (u8*)strchr(p,AT_CMD_LF);
    }
    return ret = EAT_FALSE;
}
/*************************************************************
 * 传入参数:      ReFrModem获取的整个字符串".....+CLCC: ......"
 * 返回值  :      电话的状态
 * Note:
 * 调用该函数的条件:
 *                已经明确知道字符串".....+CLCC: ......"中含有"+CLCC: "字符串
 ************************************************************/
static u8 get_clcc_status(const u8 *pRspStr)
{/* +CLCC: 1,1,4,0,0,"18782970552",129,""  */
    u8 stat=0;
    const u8 *p = pRspStr;
    u8 rspStr[] = {"+CLCC: "};

    if(NULL == p)
        return stat = 0;/*NULL pointer*/

    p = (u8*)strchr(p,'+');
    while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
    {
      p++;
    } 
    
    if (!strncmp(p,rspStr,strlen(rspStr)))
    {
     log_1("get_clcc_status():\"+CLCC\"->[Matched]");
     stat=*(p+11);/*返回的字符串第11位是状态位*/
     log_0("clcc status:%c",stat);
    }  
    else 
        log_0("clcc is not matched");
    return stat;
}
static void incoming(u8 *str)
{
   u8 stat=0;
   u8 *p=str;
   u8 phnum[PbLength]={0};
   log_2("GET INTO incoming()!!!");
   stat=get_clcc_status(p);
   if ('4'==stat)          /*incoming*/
   {
     if(get_incoming_num(p,phnum))
     {     
        log_2("[incoming] incoming phone number is [%s]",phnum);
        if (white_list(phnum))  
        {                            /*acceptable ，answer automaticly*/
          log_1("[incoming] Phone numbers is in the whitelist->[answer the phone]!!");
          ignore_ath_set(EAT_TRUE);
          INCOMING_VALID();
          log_2("pin 66 +incoming4");
          answer();
        }
        else
        {
          log_2("[incoming] Phone numbers is NOT in the whitelist->[hang up]!!");
          hang_up(AT_END);
        }
      }
   }
   else if('6'==stat)/*The call from the other side, the other side hang up*/
   {
     INCOMING_INVALID();
     log_2("pin 66 -incoming6");
     CHARGING_INVALID();
     status_init();
   }
}


static AtCmdRsp atd_cb(const u8 *pRspStr)
{/*atd1111111111111; return string:OK +CLCC: 1,0,2,0,0,"phone number",129,"A"*/
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    u8 *rspStrTable[ ] = {"+CLCC: ",};//,"RING","NO CARRIER"};
    s16 rspType = -1;
    u8  i = 0;
    const u8  *p = pRspStr;
    log_2("[function] %s",__FUNCTION__);
    while(p) {
        /* ignore \r \n */
        while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
        {
            p++;
        }

        for (i = 0; i < sizeof(rspStrTable) / sizeof(rspStrTable[0]); i++)
        {
            if (!strncmp(rspStrTable[i], p, strlen(rspStrTable[i])))
            {
                rspType = i;
                break;
            }
        }

        p = (u8*)strchr(p,AT_CMD_LF);
    }

    switch (rspType)
    {
        case 0:  /* +CLCC: 1,1,6,0,0,"PHONE NUMBER",129,"A" */
        {
          u8 stat;
          stat=get_clcc_status(pRspStr);
          switch(stat)
          {
           case '0':
              log_0("[dialing]The other party answer my calls，stop dialing");
              CHARGING_VALID();
              set_in_the_dialogue(EAT_TRUE);
              AtCmdOverTimeStop();  // stop timer EAT_TIMER_2
              atd_answer_flag = EAT_TRUE;
              eat_timer_stop(TIMER_25S); // stop dialing 
              eat_send_msg_to_user(EAT_USER_1, EAT_USER_0, EAT_FALSE, 1, "1",NULL);
              rspValue = AT_RSP_WAIT;
              break;
              
           case '6':
             if( EAT_TRUE == atd_answer_flag)
               { //返回6，属于电话接通后挂断的情况
                 log_0("[dialing]The other party hangs up my calls");
                 eat_send_msg_to_user(EAT_USER_1, EAT_USER_0, EAT_FALSE, 1, "0",NULL);
                 CHARGING_INVALID();
                 INCOMING_INVALID();
                 atd_answer_flag = EAT_FALSE;
                 rspValue = AT_RSP_FUN_OVER;
                 status_init();
                 break;
                }
              else
                {//返回 6，但是属于对方BUSY or NO ANSWER的情况
                 log_0("[dialing]BUSY OR NO ANSWER");
                 rspValue = AT_RSP_WAIT;
                 break;
                }             
              
           case '3':
             log_0("[dialing]alerting");
             rspValue = AT_RSP_WAIT;
             break;
             
           case '2':
             log_0("[dialing]dialing");
             rspValue = AT_RSP_WAIT;
             break;
             
           default:
              rspValue = AT_RSP_WAIT;
              log_0("OK+CLCC  default");
              break;
          }
        }
          break;

        case 1:  /* RING */
            rspValue  = AT_RSP_WAIT;
            break;

        case 2:  /* NO CARRIER */
            rspValue = AT_RSP_FUN_OVER;
            break;

        default:
            rspValue = AT_RSP_WAIT;
            break;
    }
    return rspValue;
}
static AtCmdRsp ata_cb(const u8 *pRspStr)
{
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    s16 rspType = -1;
    u8  i = 0;
    const u8 *p = pRspStr;
    u8  rspStr[] = {"+CLCC: "};
    u8  stat;
    log_2("[function] %s()",__FUNCTION__);
    while(p) {
        /* ignore \r \n */
        while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
        {
            p++;
        }
        if (!strncmp(p,rspStr,strlen(rspStr)))
        {
            log_2("%s():\"+CLCC\"->[Matched]",__FUNCTION__);
            stat=get_clcc_status(pRspStr);
            break;
        }
        
        p = (u8*)strchr(p,AT_CMD_LF);
    }
  
    switch (stat)
    {
       case '0':     /* +CLCC: 1,1,(stat),0,0,"PHONE NUMBER",129,"A" */ 
           eat_send_msg_to_user(EAT_USER_1, EAT_USER_0, EAT_FALSE,1, "1",EAT_NULL);
           log_0("[incoming]Answering the call");
           CHARGING_VALID();//计费通知
           set_in_the_dialogue(EAT_TRUE);
           AtCmdOverTimeStop();     //turn off EAT_TIEMR_2
           rspValue = AT_RSP_WAIT; 
           break;
           
       case '6':      /*hang up*/
           eat_send_msg_to_user(EAT_USER_1, EAT_USER_0, EAT_FALSE, 1, "0",EAT_NULL);
           log_0("[incoming] The other party hangs up [incoming] calls");
           CHARGING_INVALID();
           INCOMING_INVALID();
           log_2("pin 66 -ata_cb");
           set_in_the_dialogue(EAT_FALSE);
           rspValue = AT_RSP_FUN_OVER;
           log_0("ata return AT_RSP_OK");
           break;
           
       default:
           rspValue = AT_RSP_WAIT;
           break;
    }
    return rspValue;
}
/*
 * ATH return:
 * 1)OK;ATH命令之前，6状态已经返回的情况
 * 2)+CLCC: ....,6,..... OK;ATH之前，6状态还没返回
 */
static AtCmdRsp __ath_cb(const u8 *pRspStr,u8 type)
{
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    u8  *rspStrTable[ ] = {"+CLCC: ","OK","NO CARRIER"};
    s16 rspType = -1;
    u8  i = 0;
    const u8  *p = pRspStr;
    log_2("[function] %s",__FUNCTION__);
    while(p) {
        /* ignore \r \n */
        while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
        {
            p++;
        } 
        for (i = 0; i < sizeof(rspStrTable) / sizeof(rspStrTable[0]); i++)
        {
            if (!strncmp(rspStrTable[i], p, strlen(rspStrTable[i])))
            {
                rspType = i;
                if(0/*Note:该值与"+CLCC: "在数组中的索引值有关*/ == i)
                    goto __here__; //有时+CLCC与OK在一个字符串中返回，只要查到+CLCC: 就直接goto下一阶段
                break;
            }
        } 
        p = (u8*)strchr(p,AT_CMD_LF);
    }
__here__:
    switch (rspType)
    {
       case 0:  
       {
         u8 stat=0;
         stat=get_clcc_status(pRspStr);
         if('6'==stat)
           {
             if( EAT_TRUE == get_in_the_dialogue())
                {
                INCOMING_INVALID();
				CHARGING_INVALID();
                eat_send_msg_to_user(EAT_USER_1, EAT_USER_0, EAT_FALSE, 1,"0",NULL);
                atd_answer_flag = EAT_FALSE;
                }
             
             if(type)
                rspValue = AT_RSP_CONTINUE;
             else 
               {
                rspValue = AT_RSP_FUN_OVER;
                log_0("ath return AT_RSP_OK");
                status_init();
               }
             break;
           }
             rspValue = AT_RSP_ERROR;
       }
         break;
       case 1://拨号且未接通，对方挂断电话，在ath挂断之前，6状态已经返回，当前只返回OK
             if(type)
             {
               rspValue = AT_RSP_CONTINUE;
             }
             else 
             {   
			   rspValue = AT_RSP_FUN_OVER; 
			 }
           break;
       case 2:
           rspValue = AT_RSP_WAIT;
           break;
       default:
           rspValue = AT_RSP_WAIT;
           break;
    }
    return rspValue;
}

static AtCmdRsp __playback_cb(const u8 *pRspStr,u8 type)
{
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    u8  *rspStrTable[ ] = {/*must be first*/"+CREC: 0","OK","ERROR","+CREG: 1",};
    s16 rspType = -1;
    u8  i = 0;
    const u8  *p = pRspStr;
    log_2("[function] %s",__FUNCTION__);
    log_2("[%s]",p);
    while(p) {
        /* ignore \r \n */
        while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
        {
            p++;
        } 
        for (i = 0; i < sizeof(rspStrTable) / sizeof(rspStrTable[0]); i++)
        {
            if (!strncmp(rspStrTable[i], p, strlen(rspStrTable[i])))
            {
                rspType = i;
                if (0/*该值与"+CREC: 0"在数组中的索引值有关*/ == i)
                    goto __next__stage__;
                break;
            }
        } 
        p = (u8*)strchr(p,AT_CMD_LF);
    }
    
__next__stage__:
     switch (rspType)
    {

        case 0:  /* +CREC: 0 Idle*/
             //当模块出错时，会一直语音报错
             if(EAT_TRUE == at_error || EAT_TRUE == network_error)
                {
                 crec(STR5,AT_END);
                 rspValue = AT_RSP_CONTINUE;//一直持续下去
                 break;
                }
        case 2:/*ERROR*/
            if (AT_APPEND == type)
            rspValue = AT_RSP_CONTINUE;
            else 
            rspValue = AT_RSP_FUN_OVER;
           
            /*取消一键拨号或语音播放结束,要拉低66号引脚，但还需播放语音，所以在播放语
             *音完成后再拉低66号引脚，用clear_66来判断
             */
            if( EAT_TRUE == clear_66)
                {
                INCOMING_INVALID();                
                log_2("pin 66 -__playback_cb");
                clear_66 =EAT_FALSE;
                }
            set_prompt(IDLE);
            break;
        case 3:
            
             /*  
              * 修改if(EAT_TRUE == network_error):
              *   若任意有一个AT执行错误(AT_RSP_ERROR),at_error会被设置为true，
              *   则模块会一直报错，不管网络是否正常或是否恢复
              */
            if(EAT_TRUE == network_error && EAT_FALSE == at_error)
             {/*网络恢复*/
              network_error = EAT_FALSE;
              INCOMING_INVALID();
              log_2("pin 66 -__playbacd_cb3");
              rspValue = AT_RSP_FUN_OVER;
             }
            else /*语音播放期间，网络状态的正常上报*/
                rspValue = AT_RSP_WAIT;
            break;
             
        case 1:  /*OK*/    
        default:
            rspValue = AT_RSP_WAIT;
            set_prompt(BUSY);
        break;
    }
    return rspValue;
}

/*#####################  发送AT命令 ################################*/

// ATH是发送队列的最后一个AT指令，会清除发送队列
static AtCmdRsp ath_cb_end(u8 *pRspStr)
{
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    const u8  *p = pRspStr;
    log_2("[function] %s",__FUNCTION__);
    rspValue=__ath_cb(p,0);
    return rspValue;
}
// ATH后还有AT指令
static AtCmdRsp ath_cb_append(u8 *pRspStr)
{
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    u8  *p = pRspStr;
    log_2("[function] %s",__FUNCTION__);
    rspValue=__ath_cb(p,1);
    return rspValue;
}


eat_bool hang_up(At_add_t type)
{
    eat_bool result = FALSE;
    u8 i = 0;
    AtCmdEntity atCmdInit={"ATH"AT_CMD_END, 5,NULL};

    if(AT_APPEND == type)
      atCmdInit.p_atCmdCallBack = ath_cb_append;
    else 
      atCmdInit.p_atCmdCallBack = ath_cb_end;
    /* set at cmd func's range,must first set */
    result = simcom_atcmd_queue_fun_set(1);

    if(!result)                                 /* the space of queue is poor */
        return FALSE;
        
    simcom_atcmd_queue_append(atCmdInit);
    return TRUE;
}


eat_bool answer(void)
{
    eat_bool result = FALSE;
    u8 i = 0;
    AtCmdEntity atCmdInit={"ATA"AT_CMD_END, 5,ata_cb};
    log_2("[function] %s",__FUNCTION__);
    /* set at cmd func's range,must first set */
    result = simcom_atcmd_queue_fun_set(1);

    if(!result)                                 /* the space of queue is poor */
        return FALSE;
        
    simcom_atcmd_queue_append(atCmdInit);
    return TRUE;
}

eat_bool calling(pb_node *ptr)
{
    eat_bool result = FALSE;
    u8 i = 0;
    u8 phone_num[PbLength] = {0};
    AtCmdEntity atCmdInit={NULL, 0, atd_cb};
    log_2("[function] %s",__FUNCTION__);
    /* set at cmd func's range,must first set */
    result = simcom_atcmd_queue_fun_set(1);

    if(!result)                                 
        return FALSE;

    result=get_phone_num(ptr,phone_num);
    if (!result)
        return FALSE;
    {
      u8 pAtCmd[30] = {0};
      sprintf(pAtCmd,"ATD%s;"AT_CMD_END,phone_num);
      atCmdInit.p_atCmdStr = pAtCmd;
      atCmdInit.cmdLen = strlen(pAtCmd);
  
      simcom_atcmd_queue_append(atCmdInit);
    }
    return TRUE;
}

static AtCmdRsp playback_cb_append(const u8 *pRspStr)
{
    const u8 * pStr = pRspStr;
    AtCmdRsp  rspValue = AT_RSP_WAIT; 
    rspValue = __playback_cb(pStr,AT_APPEND);
    return rspValue;
}
static AtCmdRsp playback_cb_end(const u8 *pRspStr)
{
    const u8 * pStr = pRspStr;
    AtCmdRsp  rspValue = AT_RSP_WAIT; 
    rspValue = __playback_cb(pStr,AT_END);
    return rspValue;
}

eat_bool crec(const u8 *path,At_add_t type)
{
    eat_bool result = FALSE;
    u8 i = 0;
    u8 phone_num[PbLength] = {0};
    AtCmdEntity atCmdInit={NULL,0,NULL};

    if(NULL == path)
        return FALSE;
    if(AT_APPEND == type)
        atCmdInit.p_atCmdCallBack = playback_cb_append;
     else 
        atCmdInit.p_atCmdCallBack = playback_cb_end;
     
    result = simcom_atcmd_queue_fun_set(1);/*only one AT */
    if(!result)                                 
      return FALSE;

    {
    u8 At_buf[AUD_MAX_PATH_LEN] = {0};
    sprintf(At_buf,"AT+CREC=4,\"%s\",0,100"AT_CMD_END,path);
    atCmdInit.p_atCmdStr = At_buf;
    atCmdInit.cmdLen = strlen(At_buf);
    }
    simcom_atcmd_queue_append(atCmdInit);  

    return TRUE;
}
/*
 *  +CREC: 0  CRLF OK
 *  若两个同时在一个字符串中返回，会优先搜索+CREC: 0
 */
static AtCmdRsp  cancel_crec_cb(const u8 *pRspStr)
{
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    u8  *rspStrTable[ ] = {"OK","+CREC: 0","ERROR"};
    s16 rspType = -1;
    u8  i = 0;
    const  u8  *p = pRspStr;
    log_2("[function] %s",__FUNCTION__);
    log_2("[%s]",p);
    while(p) {
        /* ignore \r \n */
        while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
        {
            p++;
        } 
        for (i = 0; i < sizeof(rspStrTable) / sizeof(rspStrTable[0]); i++)
        {
            if (!strncmp(rspStrTable[i], p, strlen(rspStrTable[i])))
            {
                rspType = i;
                break;
            }
        } 
        p = (u8*)strchr(p,AT_CMD_LF);
    }
    
     switch (rspType)
    {
        case 0:  
        case 1:  /* +CREC: 0 Idle*/
            rspValue = AT_RSP_CONTINUE;
            INCOMING_INVALID();
            log_2("pin 66 -cancel_crec_cb");
            clear_66 =EAT_FALSE;//播放录音都会设该变量为true，若突然要终止播放，要false
            set_prompt(IDLE);
            break;
            
        case 2:           
            rspValue = AT_RSP_ERROR;
            break;
            
        default:
            rspValue = AT_RSP_WAIT;
        break;
    }
    return rspValue;

}
/*
 * Note:
 *     该函数用于其后会有其他AT指令;
 */
eat_bool cancel_crec_append(void)
{
    eat_bool result = FALSE;
    u8 i = 0;
    u8 phone_num[PbLength] = {0};
    AtCmdEntity atCmdInit={"AT+CREC=5"AT_CMD_END,11,cancel_crec_cb};

    result = simcom_atcmd_queue_fun_set(1);/*only one AT */
    if(!result)                                 
      return FALSE;

    simcom_atcmd_queue_append(atCmdInit);  
    return TRUE;
}

void atcmd_queue_head_out(void)
{
   simcom_atcmd_queue_fun_out();
}
/*  执行AT指令集合 */
void at_cluster(void)
{ 
    crec(STR1,AT_APPEND);
    clear_66 = EAT_TRUE;/*开机语音播放结束，拉低66号，设置通知标志*/
    simcom_gsm_init("1234",NULL);//GsmInitCallback); /*initialization->network cpin phonenumber etc */
}
void atcmdovertimestop(void)
{
   AtCmdOverTimeStop();
}

