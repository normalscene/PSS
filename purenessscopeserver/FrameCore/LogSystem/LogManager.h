#ifndef _LOGMANAGER_H
#define _LOGMANAGER_H

#include <stdio.h>
#include "ILogObject.h"
#include "ILogManager.h"
#include "BaseTask.h"

//管理日志块的池
//一个使用者提议，对日志采用分级管理。
//也就是日志里面包含了
//add by freeeyes

class CLogBlockPool
{
public:
    CLogBlockPool();

    void Init(uint32 u4BlockSize, uint32 u4PoolCount);
    void Close();

    _LogBlockInfo* GetLogBlockInfo();                       //得到一个空余的日志块
    void ReturnBlockInfo(_LogBlockInfo* pLogBlockInfo);     //归还一个用完的日志块

    uint32 GetBlockSize();

private:
    _LogBlockInfo* m_pLogBlockInfo;       //日志池
    uint32         m_u4MaxBlockSize;      //日志池单块最大上限
    uint32         m_u4PoolCount;         //日志池中的日志块个数
    uint32         m_u4CurrIndex;         //日志池中当前已用到的日志块ID
};

class CLogManager : public ACE_Task<ACE_MT_SYNCH>, public ILogManager
{
public:
    CLogManager(void);

    int open ();
    virtual int svc(void);
    int Close();

    void Init(int nThreadCount = 1, int nQueueMax = MAX_MSG_THREADQUEUE, uint32 u4MailID = 0);
    int Start();
    int Stop();
    bool IsRun();

    int PutLog(_LogBlockInfo* pLogBlockInfo);
    int RegisterLog(IServerLogger* pServerLogger);
    int UnRegisterLog();

    void SetReset(bool blReset);

    void ResetLogData(uint16 u2LogLevel);

    //对维护接口
    uint32 GetLogCount();
    uint32 GetCurrLevel();

    uint16 GetLogID(uint16 u2Index);
    const char*  GetLogInfoByServerName(uint16 u2LogID);
    const char*  GetLogInfoByLogName(uint16 u2LogID);
    int    GetLogInfoByLogDisplay(uint16 u2LogID);
    uint16 GetLogInfoByLogLevel(uint16 u2LogID);

    //对内写日志的接口
    template <class... Args>
    int WriteLog_i(int nLogType, const char* fmt, Args&& ... args)
    {
        //从日志块池里面找到一块空余的日志块
        int nRet = 0;

        m_Logger_Mutex.acquire();
        _LogBlockInfo* pLogBlockInfo = m_objLogBlockPool.GetLogBlockInfo();

        if (NULL != pLogBlockInfo)
        {
            ACE_OS::snprintf(pLogBlockInfo->m_pBlock, m_objLogBlockPool.GetBlockSize() - 1, fmt, convert(std::forward<Args>(args))...);
            nRet = Update_Log_Block(nLogType, NULL, NULL, pLogBlockInfo);
        }

        m_Logger_Mutex.release();
        return nRet;
    };

    template <class... Args>
    int WriteToMail_i(int nLogType, uint32 u4MailID, const char* pTitle, const char* fmt, Args&& ... args)
    {
        int nRet = 0;
        m_Logger_Mutex.acquire();
        _LogBlockInfo* pLogBlockInfo = m_objLogBlockPool.GetLogBlockInfo();

        if (NULL != pLogBlockInfo)
        {
            ACE_OS::snprintf(pLogBlockInfo->m_pBlock, m_objLogBlockPool.GetBlockSize() - 1, fmt, convert(std::forward<Args>(args))...);
            nRet = Update_Log_Block(nLogType, &u4MailID, pTitle, pLogBlockInfo);
        }

        return nRet;
    };

    //对外写日志的接口
    virtual int WriteLogBinary(int nLogType, const char* pData, int nLen);

    virtual int WriteLog_r(int nLogType, const char* fmt, uint32 u4Len);

    virtual int WriteToMail_r(int nLogType, uint32 u4MailID, const char* pTitle, const char* fmt, uint32 u4Len);

private:
    bool Dispose_Queue();
    int ProcessLog(_LogBlockInfo* pLogBlockInfo);
    virtual int CloseMsgQueue();
    int Update_Log_Block(int nLogType, uint32* pMailID, const char* pTitle, _LogBlockInfo* pLogBlockInfo);

    //关闭消息队列条件变量
    ACE_Thread_Mutex                  m_mutex;
    ACE_Condition<ACE_Thread_Mutex>   m_cond;
    bool                              m_blRun;                    //日志系统是否启动
    bool                              m_blIsNeedReset;            //日志模块等级升级重置标志
    bool                              m_blIsMail;                 //是否可以发送邮件
    int                               m_nThreadCount;             //记录日志线程个数，目前默认是1
    int                               m_nQueueMax;                //日志线程允许的最大队列个数
    CLogBlockPool                     m_objLogBlockPool;          //日志块池
    ACE_Recursive_Thread_Mutex        m_Logger_Mutex;             //线程锁
    IServerLogger*                    m_pServerLogger;            //日志模块指针
};

typedef ACE_Singleton<CLogManager, ACE_Recursive_Thread_Mutex> AppLogManager;

#endif
