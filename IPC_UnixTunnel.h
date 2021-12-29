#ifndef __IPC_LOOPBACK_H__
#define __IPC_LOOPBACK_H__
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <vector>
#include <string.h>
#include <errno.h>
#include <strings.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/un.h>
#include <assert.h>


typedef struct _IPCTunnel{
    std::string tunnal_name;
    // int media_port; ///< 图传端口号(图像，视频)
    // int data_port;  ///< 数传端口号(控制命令等)
    /// 服务端
    int Server_MP_ListenFd;
    int Server_MP_ConnFd;
    int Server_DP_ListenFd;
    int Server_DP_ConnFd;
    /// 客户端
    int Client_MP_SocketFd;
    int Client_DP_SocketFd;
    /// 就绪标志
    bool ready;
    /// 
    pthread_t ipc_thread_id;

}IPCTunnel;

class IPC_UnixTunnel
{
public:
    /**
     * @brief Construct a new ipc unixtunnel object
     * @param nb_subprocess tunnel数量
     */
    IPC_UnixTunnel(int nb_subprocess);
    ~IPC_UnixTunnel();

    /**
     * @brief 配置进程间通信tunnel
     * @param tunnel_idx tunnel索引
     * @param tunnal_name tunnel名
     * @return int 
     */
    int config_tunnel(int tunnel_idx, std::string tunnal_name);


    /**
     * 服务端 
     */

    /**
     * @brief 创建服务端通道
     * @return int 
     */
    int Srv_Socket();
    /**
     * @brief 获取服务端图传描述符
     * @param tunnel_name tunnel名
     * @return int 
     */
    int Srv_GetMPFd(std::string tunnel_name);
    /**
     * @brief 获取服务端数传描述符
     * @param tunnel_name 
     * @return int 
     */
    int Srv_GetDPFd(std::string tunnel_name);
    /**
     * @brief 服务端多媒体数据接收
     * @param fd 
     * @param buffer 
     * @param len 
     * @param flags 
     * @return int 
     */
    int Srv_MPRecv(int fd, char* buffer, int len, int flags=0); ///< MSG_WAITALL: 等待接收完所有数据后返回
    /**
     * @brief 服务端多媒体数据发送
     * @param fd 
     * @param buffer 
     * @param len 
     * @param flags 
     * @return int 
     */
    int Srv_MPSend(int fd, char* buffer, int len, int flags=MSG_NOSIGNAL);
    /**
     * @brief 服务端通用数据接收
     * @param fd 
     * @param buffer 
     * @param len 
     * @param flags 
     * @return int 
     */
    int Srv_DPRecv(int fd, char* buffer, int len, int flags=0);
    /**
     * @brief 服务端通用数据发送
     * @param fd 
     * @param buffer 
     * @param len 
     * @param flags 
     * @return int 
     */
    int Srv_DPSend(int fd, char* buffer, int len, int flags=MSG_NOSIGNAL);


    /**
     * 客户端 
     */

    /**
     * @brief 
     * @return int 
     */
    int Cli_Socket();
    /**
     * @brief 
     * @param tunnel_name 
     * @return int 
     */
    int Cli_GetMPFd(std::string tunnel_name);
    /**
     * @brief 
     * @param tunnel_name 
     * @return int 
     */
    int Cli_GetDPFd(std::string tunnel_name);
    /**
     * @brief 
     * @param fd 
     * @param buffer 
     * @param len 
     * @param flags 
     * @return int 
     */
    int Cli_MPRecv(int fd, char* buffer, int len, int flags=0);
    /**
     * @brief 
     * @param fd 
     * @param buffer 
     * @param len 
     * @param flags 
     * @return int 
     */
    int Cli_MPSend(int fd, char* buffer, int len, int flags=MSG_NOSIGNAL);
    /**
     * @brief 
     * @param fd 
     * @param buffer 
     * @param len 
     * @param flags 
     * @return int 
     */
    int Cli_DPRecv(int fd, char* buffer, int len, int flags=0);
    /**
     * @brief 
     * @param fd 
     * @param buffer 
     * @param len 
     * @param flags 
     * @return int 
     */
    int Cli_DPSend(int fd, char* buffer, int len, int flags=MSG_NOSIGNAL);

    /**
     * @brief Tunnel状态查询
     * @return true IPC-Unix-Tunnel创建就绪
     * @return false IPC-Unix-Tunnel创建未就绪
     */
    bool TunnelCheck();

    /**
     * @brief 关闭IUT
     * @param tunnel_name 
     * @return int 
     */
    int CloseTunnel(std::string tunnel_name);
    /**
     * @brief 关闭服务端IUT
     * @param tunnel_name 
     * @return int 
     */
    int CloseServer(std::string tunnel_name);
    /**
     * @brief 关闭客户端IUT
     * @param tunnel_name 
     * @return int 
     */
    int CloseClient(std::string tunnel_name);


    /**
     * @brief 评估打印(仅用于开发、测试)
     */
    void eval_display();
private:
    static void* Srv_ListenandAccept(void* _tunnel);

private:
    std::vector<IPCTunnel> ipc_tunnel;
    int busy_tunnels;

};


#endif ///< __IPC_LOOPBACK_H__