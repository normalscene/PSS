#include "Unit_ConnectTcp.h"

#ifdef _CPPUNIT_TEST

CUnit_ConnectTcp::CUnit_ConnectTcp()
{
}

void CUnit_ConnectTcp::setUp(void)
{
    m_nServerID = 1;
}

void CUnit_ConnectTcp::tearDown(void)
{
    m_nServerID = 0;
}

void CUnit_ConnectTcp::Test_Connect_Tcp_Server(void)
{
    bool blRet                 = false;
    CPostServerData objPostServerData;
    objPostServerData.SetServerID(m_nServerID);

    //连接远程服务器
    if (false == App_ClientReConnectManager::instance()->Connect(m_nServerID,
            "127.0.0.1",
            10002,
            TYPE_IPV4,
            (IClientMessage*)&objPostServerData))
    {
        OUR_DEBUG((LM_INFO, "[Test_Connect_Tcp_Server]Connect[127.0.0.1:10002]Tcp connect is fail.\n"));
        CPPUNIT_ASSERT_MESSAGE("[Test_Connect_Tcp_Server]Connect[127.0.0.1:10002]Tcp connect is fail.", true == blRet);
        return;
    }

    ACE_Time_Value tvConnectSleep(0, 10000);
    ACE_OS::sleep(tvConnectSleep);

    //测试定时执行程序
    ACE_Time_Value tvNow = ACE_OS::gettimeofday();
    App_ClientReConnectManager::instance()->handle_timeout(tvNow, NULL);

    //获得当前连接信息
    vecClientConnectInfo VecClientConnectInfo;
    App_ClientReConnectManager::instance()->GetConnectInfo(VecClientConnectInfo);

    if (1 != VecClientConnectInfo.size())
    {
        OUR_DEBUG((LM_INFO, "[Test_Connect_Tcp_Server]App_ClientReConnectManager::instance()->GetConnectInfo() is fail.\n"));
        CPPUNIT_ASSERT_MESSAGE("[Test_Connect_Tcp_Server]App_ClientReConnectManager::instance()->GetConnectInfo() is fail.", true == blRet);
        return;
    }

    //获得ServerID的连接状态
    EM_Server_Connect_State emConnectState = App_ClientReConnectManager::instance()->GetConnectState(m_nServerID);

    if (emConnectState != SERVER_CONNECT_OK)
    {
        OUR_DEBUG((LM_INFO, "[Test_Connect_Tcp_Server]App_ClientReConnectManager::instance()->GetConnectState is fail.\n"));
        CPPUNIT_ASSERT_MESSAGE("[Test_Connect_Tcp_Server]App_ClientReConnectManager::instance()->GetConnectState is fail.", true == blRet);
        return;
    }

    //获得当前裂解端口信息
    App_ClientReConnectManager::instance()->GetServerAddr(m_nServerID);

    _ClientIPInfo objServerIPInfo;
    App_ClientReConnectManager::instance()->GetServerIPInfo(m_nServerID, objServerIPInfo);

    //设置OK
    if (false == App_ClientReConnectManager::instance()->SetServerConnectState(m_nServerID, SERVER_CONNECT_OK))
    {
        OUR_DEBUG((LM_INFO, "[Test_Connect_Tcp_Server]App_ClientReConnectManager::instance()->SetServerConnectState is fail.\n"));
        CPPUNIT_ASSERT_MESSAGE("[Test_Connect_Tcp_Server]App_ClientReConnectManager::instance()->SetServerConnectState is fail.", true == blRet);
        return;
    }

    //拼装测试发送数据
    char szSendBuffer[MAX_BUFF_200] = { '\0' };
    char szBuff[20]                 = { '\0' };
    char szSession[32]              = { '\0' };
    sprintf_safe(szBuff, 20, "freeeyes");
    sprintf_safe(szSession, 32, "FREEEYES");

    short sVersion = 1;
    short sCommand = (short)COMMAND_AUTOTEST_HEAD;
    int nPacketLen = ACE_OS::strlen(szBuff);

    memcpy(szSendBuffer, (char*)&sVersion, sizeof(short));
    memcpy((char*)&szSendBuffer[2], (char*)&sCommand, sizeof(short));
    memcpy((char*)&szSendBuffer[4], (char*)&nPacketLen, sizeof(int));
    memcpy((char*)&szSendBuffer[8], (char*)&szSession, sizeof(char) * 32);
    memcpy((char*)&szSendBuffer[40], (char*)szBuff, sizeof(char) * nPacketLen);
    int nSendLen = nPacketLen + 40;

    char* pData = (char* )szSendBuffer;

    if (false == App_ClientReConnectManager::instance()->SendData(m_nServerID, pData, nSendLen, false))
    {
        OUR_DEBUG((LM_INFO, "[Test_Connect_Tcp_Server]Send TCP [127.0.0.1:10002] is fail.\n"));
        CPPUNIT_ASSERT_MESSAGE("[Test_Connect_Tcp_Server]Send TCP [127.0.0.1:10002] is fail.\n", true == blRet);
        return;
    }

    //等待3秒处理接收数据的时间
    ACE_Time_Value tvSleep(0, 20*1000);
    ACE_OS::sleep(tvSleep);

    if (false == App_ClientReConnectManager::instance()->DeleteIClientMessage((IClientMessage*)&objPostServerData))
    {
        OUR_DEBUG((LM_INFO, "[Test_Connect_Tcp_Server]Close TCP [127.0.0.1:10002] is fail.\n"));
        CPPUNIT_ASSERT_MESSAGE("[Test_Connect_Tcp_Server]Close TCP [127.0.0.1:10002] is fail.\n", true == blRet);
    }
}

void CUnit_ConnectTcp::Test_Connect_Tcp_Server_With_Local(void)
{
    bool blRet = false;
    CPostServerData objPostServerData;
    objPostServerData.SetServerID(2);

    //连接远程服务器
    if (false == App_ClientReConnectManager::instance()->Connect(2,
            "127.0.0.1",
            10002,
            TYPE_IPV4,
            "127.0.0.1",
            20032,
            TYPE_IPV4,
            (IClientMessage*)&objPostServerData))
    {
        OUR_DEBUG((LM_INFO, "[Test_Connect_Tcp_Server_With_Local]Connect[127.0.0.1:10002]Tcp connect is fail.\n"));
        CPPUNIT_ASSERT_MESSAGE("[Test_Connect_Tcp_Server_With_Local]Connect[127.0.0.1:10002]Tcp connect is fail.", true == blRet);
        return;
    }

    ACE_Time_Value tvConnectSleep(0, 10000);
    ACE_OS::sleep(tvConnectSleep);

    if (false == App_ClientReConnectManager::instance()->Close(2))
    {
        OUR_DEBUG((LM_INFO, "[Test_Connect_Tcp_Server_With_Local]Close TCP [127.0.0.1:10002] is fail.\n"));
        CPPUNIT_ASSERT_MESSAGE("[Test_Connect_Tcp_Server_With_Local]Close TCP [127.0.0.1:10002] is fail.\n", true == blRet);
        return;
    }

    //测试关闭错误的连接
    if (false != App_ClientReConnectManager::instance()->ConnectErrorClose(3))
    {
        OUR_DEBUG((LM_INFO, "[Test_Connect_Tcp_Server_With_Local]ConnectErrorClose is fail.\n"));
        CPPUNIT_ASSERT_MESSAGE("[Test_Connect_Tcp_Server_With_Local]ConnectErrorClose is fail.\n", true == blRet);
    }
}

#endif


