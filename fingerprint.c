#include "fingerprint.h"
#include "usart.h"

#define FG_SEND_BUF_LEN			40// ���ͻ�������С
#define FG_REC_BUF_LEN			40// ���ջ�������С

//��ָ��ͷͨѶ״̬����
#define FG_STEP_NOP				0//�޶���(����״̬)
#define FG_STEP_SEND			1//��ָ��ͷ�������ݣ����
#define FG_STEP_WAIT			2//�ȴ�Ӧ���
#define FG_STEP_END				3//�յ�Ӧ���
#define TEST					4//������

typedef struct {
    uint8_t	step;//��ָ��ͷͨѶ����״̬
    uint8_t	currentCmd;//��ǰ����
    uint8_t	timeCnt; //�ȴ���ʱ�����͵�ǰ�����ȴ�Ӧ������ʱ��10ms��
    uint8_t	result;	 //���
} FG_t;
FG_t xdata FG_status;

uint8_t xdata FG_send_buf[FG_SEND_BUF_LEN]= {0};
uint8_t xdata FG_rec_buf[FG_REC_BUF_LEN]= {0}; //ָ��ͷ�հ�������
uint8_t FG_rec_sta = 0; // ָ��ͷ�հ�����bit7Ϊ������ɱ�־


//���������
code uint8_t FG_HeadAndAddr[6] = {0xef,0x01,0xff,0xff,0xff,0xff}; //��ͷ������ַ
#define FG_GetImage_Buf_Len	      6
code uint8_t FG_GetImage_Buf[FG_GetImage_Buf_Len] = {0x01,0x00,0x03,0x01,0X00,0x05};
#define FG_GenChar_Buf_Len	      7
code uint8_t FG_GenChar_Buf[FG_GenChar_Buf_Len] = {0x01,0x00,0x04,0x02,0x01,0x00,0x08};


static void FG_Send(uint8_t *buf,uint16_t len)
{   //��ָ��ͷ��������
    Uart0_send(buf,len);
}

void FG_Init(void)
{   //�ܽ�����,���ݳ�ʼ��
    P00_PushPull_Mode;
	FINGER_power_on();
    FG_DataReset();	
}

void FG_DataReset(void)
{   //���ݳ�ʼ��
    FG_status.currentCmd = 0;
    FG_status.step = FG_STEP_NOP;
    FG_status.timeCnt = 0;
    FG_status.result = 0;
}



bool FG_CheckPack(uint8_t *pack, uint32_t addr)
{   //��У��
    uint16_t i;
    uint16_t len;
    uint16_t sum;
    bool ret = TRUE;

    if( ((*(pack + 0)) != 0xEF) || ((*(pack + 1))!= 0x01) )
    {   //��ͷ����
        ret = FALSE;
    }
    else if( (*(pack + 2)) != (uint8_t)(addr>>24)  ||
             (*(pack + 3)) != (uint8_t)(addr>>16)  ||
             (*(pack + 4)) != (uint8_t)(addr>>8)   ||
             (*(pack + 5)) != (uint8_t)(addr) )
    {   //��ַ����
        ret = FALSE;
    }
    else//У��Ͳ���
    {
        sum = (*((uint8_t *)pack + 6)) + (*((uint8_t *)pack + 7)) + (*((uint8_t *)pack + 8));
        len =  ((uint16_t)(*((uint8_t *)pack + 7)) << 8 ) + (*((uint8_t *)pack + 8));//����
        i = len;
        i -= 2;//ȥ��У����ֽ���
        for(; i > 0; i --)
        {
            sum += *(pack + 8 + i);
        }
        if(sum !=  (((uint16_t)(*((uint8_t *)pack + 7 + len)) << 8 ) + (*((uint8_t *)pack + 8 + len))))
        {
            ret = FALSE;
        }
    }
    return ret;
}


void FG_ClearRecBuf(void)
{   //���¿�ʼ��������
    FG_rec_sta = 0;
}


bool FG_IsRecPack(void)
{   //�Ƿ��յ���
    return 	(((FG_rec_sta & 0x80) == 0x80)?TRUE:FALSE);
}


void FG_Rec_Pack(uint8_t buf)
{   //�հ�����,bufΪ�����յ��������ݣ��˺���Ӧ���ڴ��ڽ��պ����е��á�
    static xdata uint16_t len;//���еİ������ֶ�
    if(FG_rec_sta <FG_REC_BUF_LEN)
    {
        FG_rec_buf[FG_rec_sta] = buf;
        FG_rec_sta ++;
        if(FG_rec_sta == 1)
        {
            if(FG_rec_buf[0] != 0XEF)//��ͷ����
            {
                FG_rec_sta = 0;
            }
        }
        else if(FG_rec_sta == 2)
        {
            if(FG_rec_buf[1] != 0X01)//��ͷ����
            {
                FG_rec_sta = 0;
            }
        }
        else if(FG_rec_sta == 9)//����Ӧ���
        {
            len = FG_rec_buf[8] + 9;
            if(len > FG_REC_BUF_LEN)
            {
                len = FG_REC_BUF_LEN;
            }
        }
        else if(FG_rec_sta > 9)//����Ӧ���
        {
            if(FG_rec_sta ==  len)
            {
                FG_rec_sta |= 0x80;
            }
        }
    }
}


void FG_Test(void)
{
    FG_Send(FG_HeadAndAddr,6);
    FG_Send(FG_GetImage_Buf,FG_GetImage_Buf_Len);
}

//void FG_SendCmd(uint8_t cmd,uint8_t *buf,uint8_t len)
//{
//
//}

void FG_GenChar(void)
{
    if(FG_status.step == FG_STEP_NOP)
    {   //����״̬
        //��ȡͼ��
        FG_status.step = FG_STEP_SEND;
        FG_status.currentCmd = PS_GetImage;
        FG_Send(FG_HeadAndAddr,6);
        FG_Send(FG_GetImage_Buf,FG_GetImage_Buf_Len);
        FG_ClearRecBuf();
        FG_status.timeCnt = 20;
        FG_status.step = FG_STEP_WAIT;

    }
    else if(FG_status.step == FG_STEP_END && FG_status.currentCmd == PS_GetImage)
    {   // ��һ������Ϊ��ȡͼ��
        //����������
        FG_status.step = FG_STEP_SEND;
        FG_status.currentCmd = PS_GenChar;
        FG_Send(FG_HeadAndAddr,6);
        FG_Send(FG_GenChar_Buf,FG_GenChar_Buf_Len);
        FG_ClearRecBuf();
        FG_status.timeCnt = 30;
        FG_status.step = FG_STEP_WAIT;
    }
    else if(FG_status.step == FG_STEP_END && FG_status.currentCmd == PS_GenChar)
    {
        FG_status.step = FG_STEP_NOP;
    }
}

void FG_TimeAnaly_10ms(void)
{   //ʱ�䴦����Ҫ�ڶ�ʱ���ж��е���
    if(FG_status.step == FG_STEP_WAIT)//�ȴ�״̬
    {
        if(FG_IsRecPack() == TRUE) //�յ���
        {
            FG_status.step = FG_STEP_END;
        }
        else if(FG_status.timeCnt > 0)
        {
            FG_status.timeCnt --;
        }
        else//��ʱδ�ӵ�ָ��Ӧ���
        {
            FG_status.step = FG_STEP_END;
            //FG_status.result = 1;
        }
    }
}

void FG_InitPack()
{

}