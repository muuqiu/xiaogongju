/**
* File Name :AgentManager/PCConn.c
* Author: rootkiter
* E-mail: rootkiter@rootkiter.com
* Created Time : Tue 05 Jan 2016 01:06:57 AM PST
*/
#include "PCConn.h"


//===========================================================
//   inline  function 
//      m_SendData_CmdTunnel( conn,data, data, len);
#define M_SENDDATA_CMDTUNNEL_ERROR   -1
#define M_SENDDATA_CMDTUNNEL_OK       1
int m_SendData_CmdTunnel(pPCConn conn,char *data,int datalen){
    int nsend = -1;
    int cmdsocket = -1;
    if(conn == NULL || conn->cmd_socket == -1){
        return M_SENDDATA_CMDTUNNEL_ERROR;
    }
    // get cmd_socket
    cmdsocket = conn->cmd_socket;
    // session busylock
    while(NET_SESSION_BUSY_NOW == conn->BusyType){
        MIC_USLEEP(10);
    }
    // session lock it
    conn->BusyType = NET_SESSION_BUSY_NOW;
    // send data
//Printf_DEBUG("send data len is %d ",datalen);
    nsend = API_socket_send(cmdsocket,data,datalen);
MIC_USLEEP(50);
    // unlock it 
    conn->BusyType = NET_SESSION_UNUSED_NOW;
    // check send state
    if(nsend != datalen){
        return M_SENDDATA_CMDTUNNEL_ERROR;
    }
    return M_SENDDATA_CMDTUNNEL_OK;
}

///=======================================================
///   PCConn Functions
///=======================================================
pPCConn PCCONN_CreatePCConnFromSocket(int sock){
    pPCConn conn = (pPCConn)malloc(sizeof(PCConn));
    if(conn == NULL){
        return PCCONN_CREATEPCCONNFROMSOCKET_ERROR;
    }
    (conn->IPaddr[0])='\0'; 
    conn->ConnType   = CONNTYPE_REVERSE_CONNECT;
    conn->cmd_socket = sock;
    conn->BusyType   = NET_SESSION_UNUSED_NOW;
    return conn;
}

pPCConn PCCONN_CreatePCConnFromIPPort(char *ip,int port,int sock){
    pPCConn conn = PCCONN_CreatePCConnFromSocket(sock);
    if( PCCONN_CREATEPCCONNFROMSOCKET_ERROR == 
        conn){
        return PCCONN_CREATEPCCONNFROMIPPORT_ERROR;
    }
    conn->ConnType   = CONNTYPE_DIRECT_CONNECT;
    strncpy(conn->IPaddr,ip,MAX_IP_ADDR_LEN);
    conn->port       = port;
    conn->BusyType   = NET_SESSION_UNUSED_NOW;
    return conn;
}

int PCCONN_Free(pPCConn conn){
    if(conn == NULL){
        return PCCONN_FREE_ERROR;
    }
    free(conn);
    conn = NULL;
    return PCCONN_FREE_OK;
}

pPCConn PCCONN_Copy(pPCConn conn){
    pPCConn conn1 = NULL;
    if(conn == NULL){
        return PCCONN_COPY_ERROR;
    }
    conn1 = (pPCConn)malloc(sizeof(PCConn));
    if(conn1 == NULL){
        return PCCONN_COPY_ERROR;
    }
    conn1->ConnType   = conn->ConnType;
    conn1->BusyType   = conn->BusyType;
    conn1->port       = conn->port;
    conn1->cmd_socket = conn->cmd_socket;
    memcpy(conn1->IPaddr,conn->IPaddr,MAX_IP_ADDR_LEN);
    return conn1;
}

int PCCONN_SendData(pPCConn conn,char *data,int datalen){
    int res = m_SendData_CmdTunnel(conn,data,datalen);
    if(res == M_SENDDATA_CMDTUNNEL_ERROR){
        return PCCONN_SENDDATA_ERROR;
    }
    return datalen;
}

int PCCONN_RecvData(pPCConn conn,char *data,int maxlen){
    int nrecv;
//    char cmdbuff[MAX_PROTO_BUFLEN];
    if( conn == NULL ){
        return PCCONN_RECVDATA_ERROR;
    }
MIC_USLEEP(10);
    nrecv = API_socket_recv(conn->cmd_socket,data,maxlen);
    if(nrecv == 0){ return PCCONN_RECVDATA_ERROR; }
    if(nrecv > maxlen){
        Printf_Error("nrecv (%d) > maxlen (%d)",
            nrecv,maxlen);
        return PCCONN_RECVDATA_ERROR;
    }
//    memcpy(data,cmdbuff,nrecv);
    return nrecv;
}

pPCConn PCCONN_Connect(char *ip,int port){
    int socket = SOCKET_CONNECT_ERROR;
    pPCConn conn = NULL;
    if(ip == NULL || port == -1){
        return PCCONN_CONNECT_ERROR;
    }
    socket = API_socket_connect(ip,port);
    if(socket == SOCKET_CONNECT_ERROR){
        return PCCONN_CONNECT_ERROR;
    }
    conn = PCCONN_CreatePCConnFromIPPort
        (ip,port,socket);
    if(conn == PCCONN_CREATEPCCONNFROMSOCKET_ERROR){
        return PCCONN_CONNECT_ERROR;
    }
    return conn;
}

typedef struct cbf_data{
    cbf_listen_fun fun;
    char *funarg;
}CBF_DATA,*pCBF_DATA;

int m_fun_server_cbf(int sock,char *data){
    if(sock == -1 || data == NULL){
        Printf_Error("m_fun_server_cbf no CBF fun????");
        return 0;
    }
    pPCConn conn = PCCONN_CreatePCConnFromSocket(sock);
    if(conn == PCCONN_CREATEPCCONNFROMSOCKET_ERROR){
        return 0;
    }
    pCBF_DATA fun_and_arg = (pCBF_DATA)data;
    cbf_listen_fun fun = fun_and_arg->fun;
    char *      funarg = fun_and_arg->funarg;
    int res = fun(conn,funarg);
    if(res == 0){
        Printf_DEBUG("fun run error????");
        return 0;
    }
    return 1;
}

int PCCONN_Listen(int port,
            int maxlen,
            cbf_listen_fun fun,
            char *pValue
        ){
    if(fun == NULL || port == -1 || maxlen <=0 )
    {
        return PCCONN_LISTEN_ERROR;
    }
    int sockserver = API_socket_init_server
        (port,maxlen);
    if( SOCKET_SERVER_INIT_ERROR == sockserver )
    {
        return PCCONN_LISTEN_ERROR;
    }

    pCBF_DATA data = (pCBF_DATA)malloc(sizeof(CBF_DATA));
    if(data == NULL) return PCCONN_LISTEN_ERROR;
    data -> fun    = fun;
    data -> funarg = pValue;

    int res = API_socket_server_start(sockserver,
        m_fun_server_cbf,
        (char*)data);
    if(res == API_SOCKET_SERVER_START_ERROR){
        return PCCONN_LISTEN_ERROR;
    }
    return PCCONN_LISTEN_OK;
}

int PCCONN_Conn_2_Conn(pPCConn conn1,pPCConn conn2,int usec){
    int sock1,sock2;
    if(conn1 == NULL || conn2 == NULL){
        return PCCONN_CONN_2_CONN_ERROR;
    }
    sock1 = conn1->cmd_socket;
    sock2 = conn2->cmd_socket;
    int res = tunn_sock_to_sock(sock1,sock2,usec);
    if(TUNNEL_SOCK_TO_SOCK_ERROR == res){
        return PCCONN_CONN_2_CONN_ERROR;
    }
    return PCCONN_CONN_2_CONN_OK;
}
