#include "fingerprint.h"
#include "usart.h"

#define FG_SEND_BUF_LEN			40// 发送缓冲区大小
#define FG_REC_BUF_LEN			40// 接收缓冲区大小

//与指纹头通讯状态定义
#define FG_STEP_NOP				0//无动作(空闲状态)
#define FG_STEP_SEND			1//向指纹头发送数据（命令）
#define FG_STEP_WAIT			2//等待应答包
#define FG_STEP_END				3//收到应答包
#define TEST					4//测试用

typedef struct {
    uint8_t	step;//与指纹头通讯所处状态
    uint8_t	currentCmd;//当前命令
    uint8_t	timeCnt; //等待计时（发送当前命令后等待应答包的最长时间10ms）
    uint8_t	result;	 //结果
} FG_t;
FG_t xdata FG_status;

uint8_t xdata FG_send_buf[FG_SEND_BUF_LEN]= {0};
uint8_t xdata FG_rec_buf[FG_REC_BUF_LEN]= {0}; //指纹头收包缓存区
uint8_t FG_rec_sta = 0; // 指纹头收包处理，bit7为接收完成标志


//常用命令定义
code uint8_t FG_HeadAndAddr[6] = {0xef,0x01,0xff,0xff,0xff,0xff}; //包头及包地址
#define FG_GetImage_Buf_Len	      6
code uint8_t FG_GetImage_Buf[FG_GetImage_Buf_Len] = {0x01,0x00,0x03,0x01,0X00,0x05};
#define FG_GenChar_Buf_Len	      7
code uint8_t FG_GenChar_Buf[FG_GenChar_Buf_Len] = {0x01,0x00,0x04,0x02,0x01,0x00,0x08};


static void FG_Send(uint8_t *buf,uint16_t len)
{   //向指纹头发送数据
    Uart0_send(buf,len);
}

void FG_Init(void)
{   //管脚配置,数据初始化
    P00_PushPull_Mode;
	FINGER_power_on();
    FG_DataReset();	
}

void FG_DataReset(void)
{   //数据初始化
    FG_status.currentCmd = 0;
    FG_status.step = FG_STEP_NOP;
    FG_status.timeCnt = 0;
    FG_status.result = 0;
}



bool FG_CheckPack(uint8_t *pack, uint32_t addr)
{   //包校验
    uint16_t i;
    uint16_t len;
    uint16_t sum;
    bool ret = TRUE;

    if( ((*(pack + 0)) != 0xEF) || ((*(pack + 1))!= 0x01) )
    {   //包头不对
        ret = FALSE;
    }
    else if( (*(pack + 2)) != (uint8_t)(addr>>24)  ||
             (*(pack + 3)) != (uint8_t)(addr>>16)  ||
             (*(pack + 4)) != (uint8_t)(addr>>8)   ||
             (*(pack + 5)) != (uint8_t)(addr) )
    {   //地址不对
        ret = FALSE;
    }
    else//校验和不对
    {
        sum = (*((uint8_t *)pack + 6)) + (*((uint8_t *)pack + 7)) + (*((uint8_t *)pack + 8));
        len =  ((uint16_t)(*((uint8_t *)pack + 7)) << 8 ) + (*((uint8_t *)pack + 8));//包长
        i = len;
        i -= 2;//去掉校验和字节数
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
{   //重新开始接收数据
    FG_rec_sta = 0;
}


bool FG_IsRecPack(void)
{   //是否收到包
    return 	(((FG_rec_sta & 0x80) == 0x80)?TRUE:FALSE);
}


void FG_Rec_Pack(uint8_t buf)
{   //收包处理,buf为串口收到的新数据，此函数应该在串口接收函数中调用。
    static xdata uint16_t len;//包中的包长度字段
    if(FG_rec_sta <FG_REC_BUF_LEN)
    {
        FG_rec_buf[FG_rec_sta] = buf;
        FG_rec_sta ++;
        if(FG_rec_sta == 1)
        {
            if(FG_rec_buf[0] != 0XEF)//包头错误
            {
                FG_rec_sta = 0;
            }
        }
        else if(FG_rec_sta == 2)
        {
            if(FG_rec_buf[1] != 0X01)//包头错误
            {
                FG_rec_sta = 0;
            }
        }
        else if(FG_rec_sta == 9)//接收应答包
        {
            len = FG_rec_buf[8] + 9;
            if(len > FG_REC_BUF_LEN)
            {
                len = FG_REC_BUF_LEN;
            }
        }
        else if(FG_rec_sta > 9)//接收应答包
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
    {   //空闲状态
        //获取图像
        FG_status.step = FG_STEP_SEND;
        FG_status.currentCmd = PS_GetImage;
        FG_Send(FG_HeadAndAddr,6);
        FG_Send(FG_GetImage_Buf,FG_GetImage_Buf_Len);
        FG_ClearRecBuf();
        FG_status.timeCnt = 20;
        FG_status.step = FG_STEP_WAIT;

    }
    else if(FG_status.step == FG_STEP_END && FG_status.currentCmd == PS_GetImage)
    {   // 上一个命令为获取图像
        //生成特征点
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
{   //时间处理，需要在定时器中断中调用
    if(FG_status.step == FG_STEP_WAIT)//等待状态
    {
        if(FG_IsRecPack() == TRUE) //收到包
        {
            FG_status.step = FG_STEP_END;
        }
        else if(FG_status.timeCnt > 0)
        {
            FG_status.timeCnt --;
        }
        else//超时未接到指纹应答包
        {
            FG_status.step = FG_STEP_END;
            //FG_status.result = 1;
        }
    }
}

void FG_InitPack()
{

}