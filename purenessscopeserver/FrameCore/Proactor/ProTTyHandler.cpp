#include "ProTTyHandler.h"

CProTTyHandler::CProTTyHandler() : m_blState(false), m_blPause(false), m_pTTyMessage(NULL), m_emDispose(CONNECT_IO_PLUGIN), m_u4PacketParseInfoID(0)
{
    m_szName[0] = 0;
    m_pmbReadBuff = new ACE_Message_Block(MAX_BUFF_1024);
}

CProTTyHandler::~CProTTyHandler()
{
    Close();

    //回收资源
    SAFE_DELETE(m_pTTyMessage);
    SAFE_DELETE(m_pmbReadBuff);
}

bool CProTTyHandler::ConnectTTy()
{
    if (true == m_blState)
    {
        //如果设备已经在连接状态则什么也不做
        return true;
    }

    //连接设备描述符
    if (m_Connector.connect(m_Ttyio, ACE_DEV_Addr(m_szName), 0, ACE_Addr::sap_any, 0, O_RDWR | FILE_FLAG_OVERLAPPED) == -1)
    {
        OUR_DEBUG((LM_INFO, "[CProTTyHandler::Init]m_Connector.connect(%s) fail.\n", m_szName));
        return false;
    }

    //关联设备本身
    if (m_Ttyio.control(ACE_TTY_IO::SETPARAMS, &m_ObjParams) == -1)
    {
        OUR_DEBUG((LM_INFO, "[CProTTyHandler::Init]m_Ttyio SETPARAMS(%s) fail.\n", m_szName));
        return false;
    }

    //将句柄绑定给反应器(读对象)
    if (-1 == m_ObjReadRequire.open(*this, m_Ttyio.get_handle(), 0, this->proactor()))
    {
        OUR_DEBUG((LM_INFO, "[CProTTyHandler::Init]m_Ttyio open(%s) read fail.\n", m_szName));
        return false;
    }

    //将句柄绑定给反应器（写对象）
    if (-1 == m_ObjWriteRequire.open(*this, m_Ttyio.get_handle(), 0, this->proactor()))
    {
        OUR_DEBUG((LM_INFO, "[CProTTyHandler::Init]m_Ttyio open(%s) write fail.\n", m_szName));
        return false;
    }

    m_blState = true;
    return true;
}

void CProTTyHandler::Close()
{
    if (true == m_blState)
    {
        m_ObjReadRequire.cancel();
        m_ObjWriteRequire.cancel();

        if (CONNECT_IO_FRAME == m_emDispose)
        {
            //发送packetParse断开消息
            App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->DisConnect(m_u4ConnectID);

            //发送框架消息
            ACE_INET_Addr m_addrRemote;
            Send_MakePacket_Queue(m_u4ConnectID, m_u4PacketParseInfoID, NULL, PACKET_TTY_DISCONNECT, m_addrRemote, "TTy", 0, CONNECT_IO_TTY);
        }

        //关闭转发接口
        App_ForwardManager::instance()->DisConnectRegedit(m_szName, ENUM_FORWARD_TCP_TTY);
        m_strDeviceName = "";

        m_Ttyio.close();
        m_blState = false;
    }
}

bool CProTTyHandler::Init(uint32 u4ConnectID, const char* pName, ACE_TTY_IO::Serial_Params inParams, ITTyMessage* pTTyMessage, EM_CONNECT_IO_DISPOSE emDispose, uint32 u4PacketParseInfoID)
{
    m_u4ConnectID = u4ConnectID;
    sprintf_safe(m_szName, MAX_BUFF_100, "%s", pName);
    m_pTTyMessage         = pTTyMessage;
    m_ObjParams           = inParams;
    m_emDispose           = emDispose;
    m_u4PacketParseInfoID = u4PacketParseInfoID;

    //初始化连接设备
    if (true == ConnectTTy())
    {
        if (CONNECT_IO_FRAME == m_emDispose)
        {
            //发送packetParse断开消息
            _ClientIPInfo objClientIPInfo;
            _ClientIPInfo objLocalIPInfo;
            App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID)->Connect(m_u4ConnectID,
                    objClientIPInfo,
                    objLocalIPInfo);

            //发送框架消息
            ACE_INET_Addr m_addrRemote;
            Send_MakePacket_Queue(m_u4ConnectID, m_u4PacketParseInfoID, NULL, PACKET_TTY_CONNECT, m_addrRemote, "TTy", 0, CONNECT_IO_TTY);
        }

        //查看是否存在转发接口
        m_strDeviceName = App_ForwardManager::instance()->ConnectRegedit(pName,
                          ENUM_FORWARD_TCP_TTY,
                          dynamic_cast<IDeviceHandler*>(this));

        //准备接受数据
        Ready_To_Read_Buff();
        return true;
    }
    else
    {
        return false;
    }
}

bool CProTTyHandler::GetConnectState()
{
    return m_blState;
}

ACE_TTY_IO::Serial_Params CProTTyHandler::GetParams()
{
    return m_ObjParams;
}

void CProTTyHandler::SetPause(bool blPause)
{
    m_blPause = blPause;
}

bool CProTTyHandler::GetPause()
{
    return m_blPause;
}

void CProTTyHandler::handle_read_file(const ACE_Asynch_Read_File::Result& result)
{
    if (!result.success())
    {
        //接收设备数据异常
        OUR_DEBUG((LM_ERROR, "[CProTTyHandler::handle_read_file]Error:%d.\n", (int)result.error()));

        if (CONNECT_IO_PLUGIN == m_emDispose && NULL != m_pTTyMessage)
        {
            //通知上层应用
            m_pTTyMessage->ReportMessage(m_u4ConnectID, (uint32)result.error(), EM_TTY_EVENT_RW_ERROR);
        }

        if (CONNECT_IO_FRAME == m_emDispose)
        {
            //通知框架消息
        }

        //断开当前设备
        Close();
        return;
    }

    if (0 == result.bytes_transferred())
    {
        Ready_To_Read_Buff();
        return;
    }

    ACE_Message_Block& mb = result.message_block();

    if ("" != m_strDeviceName)
    {
        App_ForwardManager::instance()->SendData(m_strDeviceName, &mb);
        return;
    }

    if (false == m_blPause)
    {
        if (CONNECT_IO_PLUGIN == m_emDispose && NULL != m_pTTyMessage)
        {
            //回调接收数据函数
            m_pTTyMessage->RecvData(m_u4ConnectID, mb.rd_ptr(), (uint32)result.bytes_transferred());
        }
        else
        {
            //调用框架的函数处理
            _Packet_Parse_Info* pPacketParse = App_PacketParseLoader::instance()->GetPacketParseInfo(m_u4PacketParseInfoID);

            if (NULL != pPacketParse)
            {
                _Packet_Info obj_Packet_Info;
                uint8 n1Ret = pPacketParse->Parse_Packet_Stream(m_u4ConnectID,
                              &mb,
                              dynamic_cast<IMessageBlockManager*>(App_MessageBlockManager::instance()),
                              &obj_Packet_Info,
                              CONNECT_IO_TTY);

                if (PACKET_GET_ENOUGH == n1Ret)
                {
                    //发送消息给消息框架
                    CPacketParse* pPacketParse = App_PacketParsePool::instance()->Create(__FILE__, __LINE__);
                    pPacketParse->SetPacket_Head_Message(obj_Packet_Info.m_pmbHead);
                    pPacketParse->SetPacket_Body_Message(obj_Packet_Info.m_pmbBody);
                    pPacketParse->SetPacket_CommandID(obj_Packet_Info.m_u2PacketCommandID);
                    pPacketParse->SetPacket_Head_Src_Length(obj_Packet_Info.m_u4HeadSrcLen);
                    pPacketParse->SetPacket_Head_Curr_Length(obj_Packet_Info.m_u4HeadCurrLen);
                    pPacketParse->SetPacket_Body_Src_Length(obj_Packet_Info.m_u4BodySrcLen);
                    pPacketParse->SetPacket_Body_Curr_Length(obj_Packet_Info.m_u4BodyCurrLen);

                    ACE_INET_Addr m_addrRemote;
                    Send_MakePacket_Queue(m_u4ConnectID, m_u4PacketParseInfoID, pPacketParse, PACKET_PARSE, m_addrRemote, "TTy", 0, CONNECT_IO_TTY);

                    //清理用完的m_pPacketParse
                    App_PacketParsePool::instance()->Delete(pPacketParse);
                }
            }
        }
    }

    Ready_To_Read_Buff();
}

void CProTTyHandler::handle_write_file(const ACE_Asynch_Write_File::Result& result)
{
    if (!result.success())
    {
        OUR_DEBUG((LM_ERROR, "[CProTTyHandler::handle_write_file]Error:%s.\n", result.error()));
        return;
    }

    OUR_DEBUG((LM_ERROR, "[CProTTyHandler::handle_write_file]Send OK:(%d).\n", result.message_block().length()));
    result.message_block().release();
}

bool CProTTyHandler::Send_Data(const char* pData, ssize_t nLen)
{
    //如果连接已断开，这里尝试重连
    ConnectTTy();

    if (true == m_blState && false == m_blPause)
    {
        ACE_Message_Block* m = new ACE_Message_Block(nLen);
        m->copy(pData, nLen);

        int nCurrLen = (int)m->length();

        if (0 != m_ObjWriteRequire.write(*m, m->length()))
        {
            //发送数据失败
            m_pTTyMessage->ReportMessage(m_u4ConnectID, (uint32)errno, EM_TTY_EVENT_RW_ERROR);

            //中断设备
            Close();
        }

        return true;
    }
    else
    {
        //当前连接中断，无法发送数据
        return false;
    }

}

bool CProTTyHandler::Device_Send_Data(const char* pData, ssize_t nLen)
{
    return Send_Data(pData, nLen);
}

void CProTTyHandler::Ready_To_Read_Buff()
{
    m_pmbReadBuff->reset();

    m_ObjReadRequire.read(*m_pmbReadBuff, m_pmbReadBuff->space());
}
