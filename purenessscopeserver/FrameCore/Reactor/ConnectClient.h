#ifndef _CONNECTCLIENT_H
#define _CONNECTCLIENT_H

//处理客户端消息
//Reactor处理模式，解析数据包并将数据放在逻辑类处理。
//添加完了Proactor下的客户端发送，开始添加reactor的系统。
//add by freeeyes
//2011-01-17

#include "define.h"

#include "ace/Reactor.h"
#include "ace/Synch.h"
#include "ace/SOCK_Stream.h"
#include "ace/Svc_Handler.h"
#include "ace/Reactor_Notification_Strategy.h"

#include "AceReactorManager.h"
#include "BaseConnectClient.h"
#include "LogManager.h"
#include "BaseHander.h"
#include "TcpRedirection.h"
#include "IDeviceHandler.h"

class CConnectClient : public ACE_Svc_Handler<ACE_SOCK_STREAM, ACE_MT_SYNCH>, public IDeviceHandler
{
public:
    CConnectClient(void);
    ~CConnectClient(void);

    virtual int open(void*);
    virtual int handle_input(ACE_HANDLE fd = ACE_INVALID_HANDLE);
    virtual int handle_close(ACE_HANDLE h, ACE_Reactor_Mask mask);
    virtual int handle_output(ACE_HANDLE fd = ACE_INVALID_HANDLE);
    virtual bool Device_Send_Data(const char* pData, ssize_t nLen);                                    //透传数据接口

    void SetClientMessage(IClientMessage* pClientMessage); //设置消息接收处理类
    void SetServerID(int nServerID);                       //设置当前的ServerID
    int  GetServerID();                                    //获得当前ServerID
    void SetPacketParseInfoID(uint32 u4PacketParseInfoID); //设置PacketParseID
    bool SendData(ACE_Message_Block* pmblk);
    void Close();

    bool GetTimeout(ACE_Time_Value const& tvNow);                 //获得当前数据处理是否超时

    void ClientClose();                                    //主动关闭
    _ClientConnectInfo GetClientConnectInfo();             //得到当前链接信息

    void Output_Debug_Data(ACE_Message_Block* pMbData, int nLogType, bool blLog = false);     //输出DEBUG信息

private:
    int RecvData();                                                       //接收数据，正常模式
    int SendMessageGroup(uint16 u2CommandID, ACE_Message_Block* pmblk);   //将接收的数据包发给框架消息处理模块

    int Dispose_Recv_Data(ACE_Message_Block* pCurrMessage);               //处理接收到的数据

    uint32                      m_u4SendSize;           //发送字节数
    uint32                      m_u4SendCount;          //发送数据包数
    uint32                      m_u4RecvSize;           //接受字节数
    uint32                      m_u4RecvCount;          //接受数据包数
    uint32                      m_u4CostTime;           //消息处理总时间
    uint32                      m_u4CurrSize;           //当前接收到的字节数
    uint32                      m_u4MaxPacketSize;      //最大接收包长
    int                         m_nIOCount;             //当前IO操作的个数
    int                         m_nServerID;            //服务器ID
    uint8                       m_u1ConnectState;       //连接状态
    char                        m_szError[MAX_BUFF_500];
    ACE_INET_Addr               m_addrRemote;

    ACE_Recursive_Thread_Mutex  m_ThreadLock;
    IClientMessage*             m_pClientMessage;            //消息处理类的指针
    ACE_Message_Block*          m_pCurrMessage;              //当前的MB对象
    ACE_Time_Value              m_atvBegin;                  //链接建立时间

    EM_s2s                      m_ems2s;                     //是否需要回调状态
    ACE_Time_Value              m_atvRecv;                   //数据接收时间
    EM_Server_Recv_State        m_emRecvState;               //0为未接收数据，1为接收数据完成，2为处理数据完成
    EM_CONNECT_IO_DISPOSE       m_emDispose;                 //处理模式，框架处理 or 业务处理
    uint32                      m_u4PacketParseInfoID;       //框架处理模块ID
    string                      m_strDeviceName;             //转发接口名称
};
#endif
