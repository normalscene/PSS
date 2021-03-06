#ifndef _SERVERMESSAGETASK_H
#define _SERVERMESSAGETASK_H

#include "ace/Synch.h"
#include "ace/Malloc_T.h"
#include "ace/Singleton.h"
#include "ace/Thread_Mutex.h"
#include "ace/Date_Time.h"

#include "IClientManager.h"
#include "MessageBlockManager.h"
#include "HashTable.h"
#include "ObjectArrayList.h"
#include "BaseTask.h"

//处理服务器间接收数据包过程代码
//如果服务器间线程处理挂起了，会尝试重启服务
//add by freeeyes

const uint32 MAX_SERVER_MESSAGE_QUEUE = 1000;    //允许最大队列长度
const uint32 MAX_DISPOSE_TIMEOUT      = 30;      //允许最大等待处理时间

const uint32 ADD_SERVER_CLIENT = ACE_Message_Block::MB_USER + 1;    //添加ClientMessage异步对象
const uint32 DEL_SERVER_CLIENT = ACE_Message_Block::MB_USER + 2;    //删除ClientMessage异步对象

//服务器间通讯的数据结构（接收包）
class _Server_Message_Info
{
public:
    int                m_nHashID        = 0;
    uint16             m_u2CommandID    = 0;
    IClientMessage*    m_pClientMessage = NULL;
    ACE_Message_Block* m_pRecvFinish    = NULL;
    ACE_Message_Block* m_pmbQueuePtr    = NULL;        //消息队列指针块
    _ClientIPInfo      m_objServerIPInfo;

    _Server_Message_Info()
    {
        //这里设置消息队列模块指针内容，这样就不必反复的new和delete，提升性能
        //指针关系也可以在这里直接指定，不必使用的使用再指定
        m_pmbQueuePtr = new ACE_Message_Block(sizeof(_Server_Message_Info*));

        _Server_Message_Info** ppMessage = (_Server_Message_Info**)m_pmbQueuePtr->base();
        *ppMessage = this;
    }

    void Close()
    {
        if (NULL != m_pmbQueuePtr)
        {
            m_pmbQueuePtr->release();
            m_pmbQueuePtr = NULL;
        }
    }

    ACE_Message_Block* GetQueueMessage()
    {
        return m_pmbQueuePtr;
    }

    void SetHashID(int nHashID)
    {
        m_nHashID = nHashID;
    }

    int GetHashID()
    {
        return m_nHashID;
    }

};

const uint32 MAX_SERVER_MESSAGE_INFO_COUNT = 100;

//_Server_Message_Info对象池
class CServerMessageInfoPool
{
public:
    CServerMessageInfoPool();

    void Init(uint32 u4PacketCount = MAX_SERVER_MESSAGE_INFO_COUNT);
    void Close();

    _Server_Message_Info* Create();
    bool Delete(_Server_Message_Info* pMakePacket);

    int GetUsedCount();
    int GetFreeCount();

private:
    CObjectArrayList<_Server_Message_Info> m_objArrayList;              //_Server_Message_Info内存对象列表
    CHashTable<_Server_Message_Info> m_objServerMessageList;           //Server Message缓冲池
    ACE_Recursive_Thread_Mutex       m_ThreadWriteLock;                //控制多线程锁
};

//服务器间数据包消息队列处理过程
class CServerMessageTask : public ACE_Task<ACE_MT_SYNCH>
{
public:
    CServerMessageTask();

    int open();
    virtual int svc (void);

    virtual int handle_signal (int signum,siginfo_t*   = 0,ucontext_t* = 0);

    bool Start();
    int  Close();

    uint32 GetThreadID();

    bool PutMessage(_Server_Message_Info* pMessage);

    bool PutMessage_Add_Client(IClientMessage* pClientMessage);

    bool PutMessage_Del_Client(IClientMessage* pClientMessage);

    bool CheckServerMessageThread(ACE_Time_Value const& tvNow);

private:
    bool CheckValidClientMessage(const IClientMessage* pClientMessage);
    bool ProcessMessage(const _Server_Message_Info* pMessage, uint32 u4ThreadID);

    virtual int CloseMsgQueue();

    void Add_ValidIClientMessage(IClientMessage* pClientMessage);
    void Update_ValidIClientMessage(const IClientMessage* pClientMessage);

    //关闭消息队列条件变量
    ACE_Thread_Mutex m_mutex;
    ACE_Condition<ACE_Thread_Mutex> m_cond;
    uint32               m_u4ThreadID         = 0;  //当前线程ID
    bool                 m_blRun              = false;       //当前线程是否运行
    uint32               m_u4MaxQueue         = MAX_SERVER_MESSAGE_QUEUE;  //在队列中的数据最多个数
    EM_Server_Recv_State m_emState            = SERVER_RECV_INIT;     //处理状态
    ACE_Time_Value       m_tvDispose;   //接收数据包处理时间

    //记录当前有效的IClientMessage，因为是异步的关系。
    //这里必须保证回调的时候IClientMessage是合法的。
    typedef vector<IClientMessage*> vecValidIClientMessage;
    vecValidIClientMessage m_vecValidIClientMessage;
};

class CServerMessageManager
{
public:
    CServerMessageManager();

    void Init();

    bool Start();
    int  Close();
    bool PutMessage(_Server_Message_Info* pMessage);
    bool CheckServerMessageThread(ACE_Time_Value const& tvNow);

    bool AddClientMessage(IClientMessage* pClientMessage);
    bool DelClientMessage(IClientMessage* pClientMessage);

private:
    CServerMessageTask*         m_pServerMessageTask = NULL;
    ACE_Recursive_Thread_Mutex  m_ThreadWritrLock;
};

typedef ACE_Singleton<CServerMessageManager, ACE_Recursive_Thread_Mutex> App_ServerMessageTask;
typedef ACE_Singleton<CServerMessageInfoPool, ACE_Recursive_Thread_Mutex> App_ServerMessageInfoPool;
#endif
