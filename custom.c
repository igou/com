#include <stdio.h>
#include <string.h>
#include "eat_modem.h"
#include "eat_interface.h"
#include "inc.h"
/********************************************************************
 * global Variables:  
 ********************************************************************/

/********************************************************************
 * Local Variables:  STATIC
 ********************************************************************/
static status g_status ={POWER_ON,EAT_FALSE,IDLE,EAT_FALSE};
//static PB     pbook = {0};
static eat_bool incoming_ignore_ath_flag = 0;
eat_bool clear_66 = EAT_FALSE;
eat_bool at_error = EAT_FALSE;
eat_bool network_error = EAT_FALSE;
eat_bool atd_answer_flag = EAT_FALSE;//向外打电话对方接与否的标志
static eat_bool cancel_delay = EAT_FALSE;

static pb_node pb_head = {&pb_head,&pb_head,0};/*循环双向链表，node插入尾部*/
static pb_node *pb_ptr = NULL;      
/************************************************************
 * 仅仅检查是否是数字，可实现其他的判断
 * TODO: ......
 *************************************************************/
static eat_bool is_phnum(u8 * str)
{
    u8 *s = str;
    eat_bool ret = EAT_TRUE;

    if( NULL == s)
        return ret = EAT_FALSE;
    
    do
    {
      if(*s>='0' && *s<='9')
        s++;
      else
        return ret = EAT_FALSE;
    }while(*s);
    
    eat_trace("%s return %d",__FUNCTION__,ret);
    return ret;
}
/************************************************************
 * 获取电话本AT的回调函数
 *************************************************************/
//循环双向链表，插入尾部
static void add_node(pb_node *head,pb_node *node)
{
    node->prev = head->prev;
    node->next = head;
    head->prev->next = node;
    head->prev = node;
}
static pb_node * get_ph_head(void)
{
    return &pb_head;
}
static pb_node *get_next_node(pb_node * cur)
{
    return cur->next;
}
#if !defined(__ABCDEF__)
AtCmdRsp pb_cb(const u8 *pRspStr)
{
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    u8   *rspStrTable[ ] = {"+CPBF: ","OK","ERROR"};
    s16   rspType = -1;
    const u8   *p = pRspStr;
    
    while(p) {
           /* ignore \r \n */
           while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
           {
               p++;
           }
           //有时号码与OK字符串会同时返回来，要同时处理这两个状态
          if (!strncmp(rspStrTable[0]/*+CPBF: */, p, strlen(rspStrTable[0])))
           {   
               pb_node *pb_p,* head = get_ph_head();
               u8 *s1,*s2;
               rspType = 0;          
               /*+CPBF: 5,"12324434343",129,"E"*/
               s1 = (u8*)strchr(p,'\"');            
               s2 = (u8*)strchr(s1+1,'\"'); 
/*TODO: 判断号码的有效性，无效的还要释放内存*/
               pb_p = (pb_node *)eat_mem_alloc(sizeof(pb_node));
               memset(pb_p->phone_num,0,PbLength);
               add_node(head,pb_p);
               
               strncpy(pb_p->phone_num,s1+1,(size_t)(s2-s1)-1);
               if (is_phnum(pb_p->phone_num))
                  {   /* if yes,cnt add 1;or do nothing  */
                   eat_trace("GET phone number:->[%s]",pb_p->phone_num); 
                   p = (u8*)strchr(p,0x0a); 
                   continue;/*处理回车后的下一个字符串*/
                  }          
           }
           if (!strncmp(rspStrTable[1], p, strlen(rspStrTable[1])))
              rspType = 1;
           else if(!strncmp(rspStrTable[2], p, strlen(rspStrTable[2])))
              rspType = 2;
           
           p = (u8*)strchr(p,0x0a); 
        }
    
    switch (rspType)
       {
          case 1: /*return OK*/
          case 2: //error
               rspValue = AT_RSP_CONTINUE ;/*next AT*/
               break;
          default:  /**/
               eat_trace("waiting rsp");
               rspValue = AT_RSP_WAIT;
               break;
       }
       return  rspValue;

}
eat_bool get_phone_num(pb_node * str,u8 * phone_ptr)
{
    eat_bool ret = EAT_FALSE;
    if(NULL == phone_ptr)
        return EAT_FALSE;/*NULL pointer*/
    
    strncpy(phone_ptr,str->phone_num,strlen(str->phone_num));
    return EAT_TRUE;
}
/************************************************************
 * 来电白名单
 *************************************************************/
eat_bool white_list(u8 *p)
{
   u8 i;
   pb_node *head = get_ph_head();
   pb_node *node = head->next;
   eat_bool ret = EAT_FALSE;
   if(NULL == p)
      return EAT_FALSE;/*NULL pointer*/
  
   while(node != get_ph_head()) {
      if(!strncmp(node->phone_num,p,strlen(p)))
         return EAT_TRUE;
     
      node = node->next;
    }
    return EAT_FALSE;
}


#else
AtCmdRsp pb_cb(const u8 *pRspStr)
{
    AtCmdRsp  rspValue = AT_RSP_WAIT;
    u8   *rspStrTable[ ] = {"+CPBF: ","OK"};
    s16   rspType = -1;
    u8    i = 0;
    const u8   *p = pRspStr;
    eat_trace("[function] %s()",__FUNCTION__);
        
    while(p) {
       /* ignore \r \n */
       while ( AT_CMD_CR == *p || AT_CMD_LF == *p)
       {
           p++;
       }
      if (!strncmp(rspStrTable[0], p, strlen(rspStrTable[0])))
       {   
           u8 *s1,*s2;
           rspType = 0;          
           /*+CPBF: 5,"12324434343",129,"E"*/
           s1 = (u8*)strchr(p,'\"');            
           s2 = (u8*)strchr(s1+1,'\"'); 
           strncpy(pbook.pb_list[pbook.cnt].phone_num,s1+1,(u32)(s2-s1-1));
           if (is_phnum(pbook.pb_list[pbook.cnt].phone_num))
              {   /* if yes,cnt add 1;or do nothing  */
               eat_trace("GET phone number:%d->[%s]",pbook.cnt,pbook.pb_list[pbook.cnt].phone_num);
               pbook.cnt++; 
               if (pbook.cnt > CNT_NUM)/*如果打印了，意味电话号码的buffer个数小了*/
                eat_trace("[PHONE BOOK ERROR]:CNT_NUM(%d) is too small,now cnt is(%d)",CNT_NUM,pbook.cnt);
              }          
           break;    /*找到CPBF就退出while*/
       }
       if (!strncmp(rspStrTable[1], p, strlen(rspStrTable[1])))
       {
          rspType = 1;
       }
       p = (u8*)strchr(p,0x0a); 
    }

    switch (rspType)
    {

       case 0:  /* get phone number [+CPBF: 1,"phone number",129,"A"   OK]*/
            eat_trace("get phone number: OK!!");
            rspValue = AT_RSP_CONTINUE;      
            break;
            
       case 1: /*only return OK,not found*/
            eat_trace("+CPBF: not found");
            rspValue = AT_RSP_STEP+1;/*next AT*/
            break;
            
       default:  /**/
            eat_trace("waiting rsp");
            rspValue = AT_RSP_WAIT;
            break;
    }
    return  rspValue;
}
eat_bool get_phone_num(u8 n,u8 * str)
{
    eat_bool ret = EAT_FALSE;
    if(NULL == str)
        return EAT_FALSE;/*NULL pointer*/
    
    if (n < pbook.cnt)
       {
        strncpy(str,pbook.pb_list[n].phone_num,strlen(pbook.pb_list[n].phone_num));
        ret=EAT_TRUE;
       }
    return ret;
}
/************************************************************
 * 来电白名单
 *************************************************************/
eat_bool white_list(u8 *p)
{
   u8 i;
   eat_bool ret = EAT_FALSE;
   if(NULL == p)
      return EAT_FALSE;/*NULL pointer*/
   
   for(i=0;i<pbook.cnt;i++)
      {
         if(!strncmp(pbook.pb_list[i].phone_num,p,strlen(p)))
            {
              return ret=EAT_TRUE;
            }
       }
    return EAT_FALSE;
}
#endif

/************************************************************
 * g_status的初始化
 *************************************************************/
void status_init(void)
{
    g_status.cal_status = WAIT_CALL;
    g_status.in_the_dialogue = EAT_FALSE;
    g_status.prompt = IDLE;
    g_status.pwrkey = EAT_FALSE;
    eat_trace("status initialization");
    return;
}
/************************************************************
 *
 *  
 *
 *************************************************************/

void set_in_the_dialogue(eat_bool bool)
{
   g_status.in_the_dialogue = bool;
}
eat_bool get_in_the_dialogue(void)
{
   return g_status.in_the_dialogue ;
}
void set_prompt(eat_bool stat)
{ /* stat:1)IDLE 2)BUSY */
   g_status.prompt = stat; // 1表示正在语音提示中,busy 0表示语音提示idle
}
eat_bool get_prompt(void)
{
   return g_status.prompt;
}
void set_cal_status(CALL_ENUM stat)
{
   g_status.cal_status = stat;
}
CALL_ENUM get_cal_status(void)
{
    return g_status.cal_status;
}
void set_pwrkey(eat_bool stat)
{
  g_status.pwrkey = stat;
}
eat_bool get_pwrkey(void)
{
   return g_status.pwrkey;
}
/***********************************
 * 目的:来电刚接通1s左右，硬件有个电平产生，
 * 导致挂断电话(非主动)，使用该变量来忽略这个挂断.
 * NOTE:来电时设置该标志，这个电平触发进入中断后就设置为无效，不要影响主动的挂断操作
 * ignore:EAT_TRUE；otherwise EAT_FALSE
 **********************************/
void ignore_ath_set(eat_bool stat)
{ 
   incoming_ignore_ath_flag = stat;
}
eat_bool  ignore_ath_get(void)
{ /* true : ignore ath; 
   * false: ath */
    return incoming_ignore_ath_flag;
}
/************************************************************
 *
 *定时器事件上报的处理函数
 *
 *************************************************************/
void  timer_hdlr(EatTimer_enum timer_id)
{
   EatTimer_enum id = timer_id;
   log_2("[function] %s() timer %d",__FUNCTION__,id);  
   switch (id)
   {
      case TIMER_8S:/*8's*/
          set_cal_status(CAN_NOT_STOP);  /*1.按键按下xs后开始拨号码,且不可终止,只接受挂电话按键*/
        //  g_status.n = 0;            
          log_1("[dialing ]start dialing");
                          /*
                           * Note:
                           * it must be AT_APEND rather than AT_END;
                           * calling behind crec;
                           */
         if(IDLE == get_prompt())
              crec(STR4,AT_APPEND);
          pb_ptr = get_next_node(&pb_head); 
          if(pb_ptr != &pb_head)//有号码
            {
            calling(pb_ptr);
            pb_ptr = get_next_node(pb_ptr);
            eat_timer_start(TIMER_25S,30000);/*此处时间将crec播放语音的大概时间考虑在内*/
            }
          else    //无号码
            pb_ptr = NULL;
         
          break;
          
      case TIMER_25S:/*25's*/

          if(EAT_FALSE == get_in_the_dialogue())
          {
             atcmd_queue_head_out();
             
             if(pb_ptr != &pb_head)
             {  
                hang_up(AT_APPEND);
                log_2("[dialing ] begin next dialing");
                calling(pb_ptr);
                pb_ptr = get_next_node(pb_ptr);
                eat_timer_start(TIMER_25S,25000);
             }
             else 
             { 
               pb_ptr = NULL;
               log_1("[dialing]All dialing faild!!stop dialing again");
               hang_up(AT_END);
               INCOMING_INVALID();
               log_2("pin 66 -timerhdlr");
              /*Do Not restart timer_25s */
               status_init();
             }
          }
          break;
       case TIMER_SOFT_RESET:
          eat_timer_start(TIMER_SOFT_RESET,600000);
          if( EAT_FALSE == get_in_the_dialogue())
            {
             incoming_ignore_ath_flag = 0;
             clear_66 = EAT_FALSE;
             at_error = EAT_FALSE;
             network_error = EAT_FALSE;
             atd_answer_flag = EAT_FALSE;//向外打电话对方接与否的标志
             cancel_delay = EAT_FALSE;
             status_init(); 
             eat_timer_stop(TIMER_25S);
             eat_timer_stop(TIMER_8S);
             INCOMING_INVALID();
             CHARGING_INVALID();
             atcmdovertimestop();
             simcom_atcmd_queue_init();
             log_0("soft reset");
            }
          break;
       case TIMER_CANCEL_DELAY:
          cancel_delay = EAT_FALSE;
          break;
          
       case TIMER_PWRKEY:
         if(EAT_TRUE == get_pwrkey())
            {
            simcom_atcmd_queue_init();
            crec(STR6,AT_END);
            eat_timer_start(TIMER_POWERDOWN,2000);//等待v_poweroff播完， 
            }
          break;
          
          case TIMER_POWERDOWN:
            INCOMING_VALID();
            log_2("pin 66 +powerdown");
            eat_power_down();            
            log_0("power down,waiting ");
          break;
          
        default:
           break;
    }
}

/************************************************************
 *
 *获取一次按键
 *
 *************************************************************/

u8 get_key(void)
{
   u8 down = 0;
   
   if(EAT_GPIO_LEVEL_LOW == READ_DIAL_2())
   {
     DIAL2_EINT_HIGH();
     //eat_trace("[interrupt] interrupt level high");
   }
   else
   {      //rising edge means key occur            
     DIAL2_EINT_LOW();
     if(POWER_ON  == get_cal_status())
        down = 0; //一键拨号初始化阶段还没完成，不接收按键
     else  
        {
        down = 1; /*  按键发生*/
        log_0("[interrupt] key down");
        }
   }
   return down;
}

/************************************************************
 *
 *按键事件的处理函数
 *
 *************************************************************/
void key_hdlr(void)
{
   if( EAT_FALSE == get_in_the_dialogue() )
    {
      if(WAIT_CALL == get_cal_status() && IDLE == get_prompt() && EAT_FALSE == cancel_delay)//WAIT_CALL && 确认前一个语音提示结束，再接受按键
      { 
      INCOMING_VALID();/*valid */
      log_1("[key ]A button to complete,start timer 3,valid pin66");
      set_cal_status(TRIGGER_CALL) ;/*进入呼叫状态*/
      eat_timer_start(TIMER_8S,10000);
      crec(STR2,AT_END);
      }
     // else if(TRIGGER_CALL == get_cal_status() && IDLE == get_prompt())//TRIGGER_CALL && 确认前一个语音提示结束，再接受按键
     else if(TRIGGER_CALL == get_cal_status()) //移除&& IDLE == get_prompt()，一键拨号开始,播放语音中也可以按键打断
      {            
        log_1("[key ]A button to complete,stop timer 3");
        eat_timer_stop(TIMER_8S);//cannel  
        cancel_delay = EAT_TRUE;
        g_status.cal_status = WAIT_CALL;//设置为了短时间多次取消按键不重复进入该else if语句
        if(BUSY == get_prompt())
            {
             atcmd_queue_head_out();
             cancel_crec_append();
             log_2("stop playback, next");
            }
        crec(STR3,AT_END);
        clear_66 = EAT_TRUE;//INCOMING_INVALID()此处不能直接拉低66脚
        eat_timer_start(TIMER_CANCEL_DELAY,8000);//一键拨号取消后有个延迟再接受按键
        status_init();
      }
      else 
      {
          log_0("ignore bottom,nothing to do"); ;//do nothing ignore interrupt                  
      }
   }
   else 
   {
       /*
        *此处的incoming_answer是为了屏蔽来电时，MCU会挂断电话的情况
        */
     if( EAT_FALSE == ignore_ath_get())
       {
         atcmd_queue_head_out();//simcom_atcmd_queue_init();
         hang_up(AT_END); 
        }
     else
         ignore_ath_set(EAT_FALSE);
   } 
}

/************************************************************
 *
 *power key 处理函数
 *
 *************************************************************/
void pwrkey_hdlr(eat_bool press)
{
  if (EAT_TRUE == press)
   {
     eat_timer_start(TIMER_PWRKEY,3000);
     set_pwrkey(EAT_TRUE);
     log_0("power key down ");
   }
   else
   {
     set_pwrkey(EAT_FALSE);
     log_0("power key up ");
     eat_timer_stop(TIMER_PWRKEY);
   }
}

/************************************************************
 *
 *设置不同时长的按键
 *
 *************************************************************/
void set_debounce(u8 data)
{
   u8 ch = data;
   log_2("data :%c",ch);
   if ('1' == ch)/*long time interval*/
    {
      DIAL2_EINT_HIGH_1S();
      log_2("1s high");
    }
   else
    {
      DIAL2_EINT_HIGH();
      log_2("200 ms high");
    }
 }

