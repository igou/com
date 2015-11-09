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
eat_bool atd_answer_flag = EAT_FALSE;//�����绰�Է������ı�־
static eat_bool cancel_delay = EAT_FALSE;

static pb_node pb_head = {&pb_head,&pb_head,0};/*ѭ��˫������node����β��*/
static pb_node *pb_ptr = NULL;      
/************************************************************
 * ��������Ƿ������֣���ʵ���������ж�
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
 * ��ȡ�绰��AT�Ļص�����
 *************************************************************/
//ѭ��˫����������β��
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
           //��ʱ������OK�ַ�����ͬʱ��������Ҫͬʱ����������״̬
          if (!strncmp(rspStrTable[0]/*+CPBF: */, p, strlen(rspStrTable[0])))
           {   
               pb_node *pb_p,* head = get_ph_head();
               u8 *s1,*s2;
               rspType = 0;          
               /*+CPBF: 5,"12324434343",129,"E"*/
               s1 = (u8*)strchr(p,'\"');            
               s2 = (u8*)strchr(s1+1,'\"'); 
/*TODO: �жϺ������Ч�ԣ���Ч�Ļ�Ҫ�ͷ��ڴ�*/
               pb_p = (pb_node *)eat_mem_alloc(sizeof(pb_node));
               memset(pb_p->phone_num,0,PbLength);
               add_node(head,pb_p);
               
               strncpy(pb_p->phone_num,s1+1,(size_t)(s2-s1)-1);
               if (is_phnum(pb_p->phone_num))
                  {   /* if yes,cnt add 1;or do nothing  */
                   eat_trace("GET phone number:->[%s]",pb_p->phone_num); 
                   p = (u8*)strchr(p,0x0a); 
                   continue;/*����س������һ���ַ���*/
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
 * ���������
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
               if (pbook.cnt > CNT_NUM)/*�����ӡ�ˣ���ζ�绰�����buffer����С��*/
                eat_trace("[PHONE BOOK ERROR]:CNT_NUM(%d) is too small,now cnt is(%d)",CNT_NUM,pbook.cnt);
              }          
           break;    /*�ҵ�CPBF���˳�while*/
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
 * ���������
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
 * g_status�ĳ�ʼ��
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
   g_status.prompt = stat; // 1��ʾ����������ʾ��,busy 0��ʾ������ʾidle
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
 * Ŀ��:����ս�ͨ1s���ң�Ӳ���и���ƽ������
 * ���¹Ҷϵ绰(������)��ʹ�øñ�������������Ҷ�.
 * NOTE:����ʱ���øñ�־�������ƽ���������жϺ������Ϊ��Ч����ҪӰ�������ĹҶϲ���
 * ignore:EAT_TRUE��otherwise EAT_FALSE
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
 *��ʱ���¼��ϱ��Ĵ�����
 *
 *************************************************************/
void  timer_hdlr(EatTimer_enum timer_id)
{
   EatTimer_enum id = timer_id;
   log_2("[function] %s() timer %d",__FUNCTION__,id);  
   switch (id)
   {
      case TIMER_8S:/*8's*/
          set_cal_status(CAN_NOT_STOP);  /*1.��������xs��ʼ������,�Ҳ�����ֹ,ֻ���ܹҵ绰����*/
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
          if(pb_ptr != &pb_head)//�к���
            {
            calling(pb_ptr);
            pb_ptr = get_next_node(pb_ptr);
            eat_timer_start(TIMER_25S,30000);/*�˴�ʱ�佫crec���������Ĵ��ʱ�俼������*/
            }
          else    //�޺���
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
             atd_answer_flag = EAT_FALSE;//�����绰�Է������ı�־
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
            eat_timer_start(TIMER_POWERDOWN,2000);//�ȴ�v_poweroff���꣬ 
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
 *��ȡһ�ΰ���
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
        down = 0; //һ�����ų�ʼ���׶λ�û��ɣ������հ���
     else  
        {
        down = 1; /*  ��������*/
        log_0("[interrupt] key down");
        }
   }
   return down;
}

/************************************************************
 *
 *�����¼��Ĵ�����
 *
 *************************************************************/
void key_hdlr(void)
{
   if( EAT_FALSE == get_in_the_dialogue() )
    {
      if(WAIT_CALL == get_cal_status() && IDLE == get_prompt() && EAT_FALSE == cancel_delay)//WAIT_CALL && ȷ��ǰһ��������ʾ�������ٽ��ܰ���
      { 
      INCOMING_VALID();/*valid */
      log_1("[key ]A button to complete,start timer 3,valid pin66");
      set_cal_status(TRIGGER_CALL) ;/*�������״̬*/
      eat_timer_start(TIMER_8S,10000);
      crec(STR2,AT_END);
      }
     // else if(TRIGGER_CALL == get_cal_status() && IDLE == get_prompt())//TRIGGER_CALL && ȷ��ǰһ��������ʾ�������ٽ��ܰ���
     else if(TRIGGER_CALL == get_cal_status()) //�Ƴ�&& IDLE == get_prompt()��һ�����ſ�ʼ,����������Ҳ���԰������
      {            
        log_1("[key ]A button to complete,stop timer 3");
        eat_timer_stop(TIMER_8S);//cannel  
        cancel_delay = EAT_TRUE;
        g_status.cal_status = WAIT_CALL;//����Ϊ�˶�ʱ����ȡ���������ظ������else if���
        if(BUSY == get_prompt())
            {
             atcmd_queue_head_out();
             cancel_crec_append();
             log_2("stop playback, next");
            }
        crec(STR3,AT_END);
        clear_66 = EAT_TRUE;//INCOMING_INVALID()�˴�����ֱ������66��
        eat_timer_start(TIMER_CANCEL_DELAY,8000);//һ������ȡ�����и��ӳ��ٽ��ܰ���
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
        *�˴���incoming_answer��Ϊ����������ʱ��MCU��Ҷϵ绰�����
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
 *power key ������
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
 *���ò�ͬʱ���İ���
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

