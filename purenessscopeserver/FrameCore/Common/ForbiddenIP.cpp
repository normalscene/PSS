#include "ForbiddenIP.h"

CForbiddenIP::CForbiddenIP()
{

}

bool CForbiddenIP::Init(const char* szConfigPath)
{
    OUR_DEBUG((LM_INFO, "[CForbiddenIP::Init]Filename = %s.\n", szConfigPath));

    if(!m_ForbiddenData.Init(szConfigPath))
    {
        OUR_DEBUG((LM_INFO, "[CForbiddenIP::Init]Read Filename = %s error.\n", szConfigPath));
        return false;
    }

    m_VecForeverForbiddenIP.clear();
    m_VecTempForbiddenIP.clear();

    _ForbiddenIP ForbiddenIP;

    TiXmlElement* pNextTiXmlElementIP   = NULL;
    TiXmlElement* pNextTiXmlElementType = NULL;

    while(true)
    {
        const char* pIpData   = m_ForbiddenData.GetData("ForbiddenIP", "ip", pNextTiXmlElementIP);
        const char* pTypeData = m_ForbiddenData.GetData("ForbiddenIP", "type", pNextTiXmlElementType);

        if(NULL == pIpData || NULL == pTypeData)
        {
            break;
        }

        sprintf_safe(ForbiddenIP.m_szClientIP, MAX_BUFF_20, "%s", pIpData);

        if (ACE_OS::strcmp(pTypeData, "TCP") == 0)
        {
            ForbiddenIP.m_u1Type = CONNECT_IO_TCP;
        }
        else
        {
            ForbiddenIP.m_u1Type = CONNECT_IO_UDP;
        }

        m_VecForeverForbiddenIP.push_back(ForbiddenIP);
    }

    return true;
}

bool CForbiddenIP::CheckIP(const char* pIP, uint8 u1ConnectType)
{
    for(const _ForbiddenIP& objForbiddenIP : m_VecForeverForbiddenIP)
    {
        if(objForbiddenIP.m_u1ConnectType == u1ConnectType
           && CompareIP(objForbiddenIP.m_szClientIP, pIP) == true)
        {
            return false;
        }
    }

    for(VecForbiddenIP::iterator b = m_VecTempForbiddenIP.begin(); b != m_VecTempForbiddenIP.end(); ++b)
    {
        if((*b).m_u1ConnectType == u1ConnectType && CompareIP((*b).m_szClientIP, pIP) == true)
        {
            //如果是禁止时间段内，则返回false，否则删除定时信息。
            if ((*b).m_tvBegin + ACE_Time_Value((*b).m_u4Second, 0) > ACE_OS::gettimeofday())
            {
                return false;
            }
            else
            {
                m_VecTempForbiddenIP.erase(b);
                return true;
            }
        }
    }

    return true;
}

bool CForbiddenIP::AddForeverIP(const char* pIP, uint8 u1ConnectType)
{
    _ForbiddenIP ForbiddenIP;
    sprintf_safe(ForbiddenIP.m_szClientIP, MAX_IP_SIZE, "%s", pIP);
    ForbiddenIP.m_u1ConnectType = u1ConnectType;
    m_VecForeverForbiddenIP.push_back(ForbiddenIP);

    if (false == SaveConfig())
    {
        OUR_DEBUG((LM_INFO, "[CForbiddenIP::AddForeverIP]SaveConfig is error.\n"));
    }

    return true;
}

bool CForbiddenIP::AddTempIP(const char* pIP, uint32 u4Second, uint8 u1ConnectType)
{
    _ForbiddenIP ForbiddenIP;
    sprintf_safe(ForbiddenIP.m_szClientIP, MAX_IP_SIZE, "%s", pIP);
    ForbiddenIP.m_u1Type        = 1;
    ForbiddenIP.m_tvBegin       = ACE_OS::gettimeofday();
    ForbiddenIP.m_u4Second      = u4Second;
    ForbiddenIP.m_u1ConnectType = u1ConnectType;
    m_VecTempForbiddenIP.push_back(ForbiddenIP);

    return true;
}

bool CForbiddenIP::DelForeverIP(const char* pIP, uint8 u1ConnectType)
{
    for(VecForbiddenIP::iterator b = m_VecForeverForbiddenIP.begin(); b != m_VecForeverForbiddenIP.end(); ++b)
    {
        if(ACE_OS::strcmp(pIP, (*b).m_szClientIP) == 0 && (*b).m_u1ConnectType == u1ConnectType)
        {
            m_VecForeverForbiddenIP.erase(b);

            if (false == SaveConfig())
            {
                OUR_DEBUG((LM_INFO, "[CForbiddenIP::DelForeverIP]SaveConfig is error.\n"));
            }

            return true;
        }
    }

    return true;
}

bool CForbiddenIP::DelTempIP(const char* pIP, uint8 u1ConnectType)
{
    for(VecForbiddenIP::iterator b = m_VecTempForbiddenIP.begin(); b !=  m_VecTempForbiddenIP.end(); ++b)
    {
        if(ACE_OS::strcmp(pIP, (*b).m_szClientIP) == 0 && (*b).m_u1ConnectType == u1ConnectType)
        {
            m_VecTempForbiddenIP.erase(b);
            return true;
        }
    }

    return true;
}

bool CForbiddenIP::SaveConfig()
{

    //将修改的配置信息写入文件
    FILE* pFile = ACE_OS::fopen(FORBIDDENIP_FILE, "wb+");

    if(NULL == pFile)
    {
        OUR_DEBUG((LM_ERROR, "[CForbiddenIP::SaveConfig]Open file fail.\n"));
        return false;
    }

    char szTemp[MAX_BUFF_500] = {'\0'};
    sprintf_safe(szTemp, MAX_BUFF_500, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n<config>\r\n");

    size_t stSize = ACE_OS::fwrite(szTemp, sizeof(char), ACE_OS::strlen(szTemp), pFile);

    if(stSize !=  ACE_OS::strlen(szTemp))
    {
        OUR_DEBUG((LM_ERROR, "[CForbiddenIP::SaveConfig]Write file fail.\n"));
        ACE_OS::fclose(pFile);
        return false;
    }

    for(_ForbiddenIP& objForbiddenIP : m_VecForeverForbiddenIP)
    {
        if(objForbiddenIP.m_u1ConnectType == CONNECT_TCP)
        {
            sprintf_safe(szTemp, MAX_BUFF_500, "<ForbiddenIP ip=\"%s\" type=\"TCP\" desc=\"ForbiddenIP，type is 'TCP' or 'UDP'\" />\r\n", objForbiddenIP.m_szClientIP);
        }
        else
        {
            sprintf_safe(szTemp, MAX_BUFF_500, "<ForbiddenIP ip=\"%s\" type=\"UDP\" desc=\"ForbiddenIP，type is 'TCP' or 'UDP'\" />\r\n", objForbiddenIP.m_szClientIP);
        }

        stSize = ACE_OS::fwrite(szTemp, sizeof(char), ACE_OS::strlen(szTemp), pFile);

        if(stSize !=  ACE_OS::strlen(szTemp))
        {
            OUR_DEBUG((LM_ERROR, "[CForbiddenIP::SaveConfig]Write file fail.\n"));
            ACE_OS::fclose(pFile);
            return false;
        }
    }

    sprintf_safe(szTemp, MAX_BUFF_500, "</config>\r\n");

    stSize = ACE_OS::fwrite(szTemp, sizeof(char), ACE_OS::strlen(szTemp), pFile);

    if(stSize !=  ACE_OS::strlen(szTemp))
    {
        OUR_DEBUG((LM_ERROR, "[CForbiddenIP::SaveConfig]Write file fail.\n"));
        ACE_OS::fclose(pFile);
        return false;
    }

    ACE_OS::fflush(pFile);
    ACE_OS::fclose(pFile);
    return true;
}

bool CForbiddenIP::CompareIP(const char* pTargetIP, const char* pClientIP)
{
    char szTargetIP[MAX_IP_SIZE];
    char szClientIP[MAX_IP_SIZE];

    char szTarget[5];
    char szClient[5];

    memcpy_safe(pTargetIP, (uint32)ACE_OS::strlen(pTargetIP), szTargetIP, (uint32)MAX_IP_SIZE, true);

    memcpy_safe(pClientIP, (uint32)ACE_OS::strlen(pClientIP), szClientIP, (uint32)MAX_IP_SIZE, true);

    char* pTargetPos = (char* )szTargetIP;
    char* pClientPos = (char* )szClientIP;

    char* pTargetTPos = ACE_OS::strstr(pTargetPos, ".");
    char* pClientTPos = ACE_OS::strstr(pClientPos, ".");

    while(pTargetTPos)
    {
        if(pClientTPos == NULL)
        {
            return false;
        }

        memcpy_safe(pTargetPos, (uint32)(pTargetTPos - pTargetPos), szTarget, (uint32)MAX_IP_SIZE);
        szTarget[(int32)(pTargetTPos - pTargetPos)] = '\0';
        memcpy_safe(pClientPos, (uint32)(pClientTPos - pClientPos), szClient, (uint32)MAX_IP_SIZE);
        szClient[(int32)(pClientTPos - pClientPos)] = '\0';

        if(strcmp(szTarget, "*") != 0 && strcmp(szTarget, szClient) != 0)
        {
            return false;
        }

        pTargetPos = pTargetTPos + 1;
        pClientPos = pClientTPos + 1;

        pTargetTPos = ACE_OS::strstr(pTargetPos + 1, ".");
        pClientTPos = ACE_OS::strstr(pClientPos + 1, ".");
    }

    if(strcmp(pTargetPos, "*") != 0)
    {
        if(strcmp(pTargetPos, pClientPos) != 0)
        {
            return false;
        }
        else
        {
            return true;
        }
    }
    else
    {
        return true;
    }
}

VecForbiddenIP* CForbiddenIP::ShowForeverIP()
{
    return &m_VecForeverForbiddenIP;
}

VecForbiddenIP* CForbiddenIP::ShowTempIP()
{
    return &m_VecTempForbiddenIP;
}
