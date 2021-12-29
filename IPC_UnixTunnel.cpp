#include "IPC_UnixTunnel.h"

// #define offsetof(TYPE, MEMBER)    ((int)&((TYPE *)0)->MEMBER)

/**
 * @brief Construct a new ipc unixtunnel object
 * @param nb_subprocess tunnel数量
 */
IPC_UnixTunnel::IPC_UnixTunnel(int nb_subprocess)
{
    IPCTunnel default_init_tunnel = {"NULL", -1, -1, -1, -1, -1, -1, false};
    // printf("ipc_tunnel.size: %ld\n", ipc_tunnel.size());
    ipc_tunnel.resize(nb_subprocess);
    busy_tunnels = 0;
    // printf("ipc_tunnel.size: %ld\n", ipc_tunnel.size());

    for(int i=0; i<ipc_tunnel.size(); i++){
        ipc_tunnel[i] = default_init_tunnel;
    }
}

IPC_UnixTunnel::~IPC_UnixTunnel()
{
    printf("Release...\n");
    std::vector<IPCTunnel>().swap(ipc_tunnel); 
    printf("ipc_tunnel.size: %ld\n", ipc_tunnel.size());
}

/**
 * @brief 配置进程间通信tunnel
 * @param tunnel_idx tunnel索引
 * @param tunnal_name tunnel名
 * @return int 
 */
int IPC_UnixTunnel::config_tunnel(int tunnel_idx, std::string tunnal_name)
{
    if(tunnel_idx >= ipc_tunnel.size()){
        return -1;
    }
    if(ipc_tunnel[tunnel_idx].tunnal_name != "NULL"){
        return -2;
    }
    ipc_tunnel[tunnel_idx].tunnal_name = tunnal_name;
    busy_tunnels++;
}

/**
 * @brief 创建服务端通道
 * @return int 
 */
int IPC_UnixTunnel::Srv_Socket()
{
    for(int i=0; i<ipc_tunnel.size(); i++){
        if(ipc_tunnel[i].tunnal_name != "NULL"){
            // printf("ipc_tunnel %d already configured\n", i);

            if( -1 == (ipc_tunnel[i].Server_MP_ListenFd = socket(AF_UNIX, SOCK_STREAM, 0))){  
                perror("MP create socket failed");
                return -1;
            }	
            if( -1 == (ipc_tunnel[i].Server_DP_ListenFd = socket(AF_UNIX, SOCK_STREAM, 0))){  
                perror("DP create socket failed");
                return -1;
            }	
            // printf("socket create success....\n");
            /// 重用地址
            int one = 1;
            setsockopt(ipc_tunnel[i].Server_MP_ListenFd, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(int));
            setsockopt(ipc_tunnel[i].Server_DP_ListenFd, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(int));
            /// 非阻塞
            int mpflag = ::fcntl(ipc_tunnel[i].Server_MP_ListenFd, F_GETFL, 0);
            int mperr = ::fcntl(ipc_tunnel[i].Server_MP_ListenFd, F_SETFL, mpflag | O_NONBLOCK);
            assert(mperr == 0);
            int dpflag = ::fcntl(ipc_tunnel[i].Server_DP_ListenFd, F_GETFL, 0);
            int dperr = ::fcntl(ipc_tunnel[i].Server_DP_ListenFd, F_SETFL, dpflag | O_NONBLOCK);
            assert(dperr == 0);

            // printf("Launch %s listen thread\n", ipc_tunnel[i].tunnal_name.c_str());
            void* tunnel_info = &(ipc_tunnel[i]);
            pthread_create(&(ipc_tunnel[i].ipc_thread_id), NULL, Srv_ListenandAccept, tunnel_info);
            pthread_detach(ipc_tunnel[i].ipc_thread_id);
        }
    }
}


void* IPC_UnixTunnel::Srv_ListenandAccept(void* _tunnel)
{
    int ret = -1;
    // printf("Server: bind, listen and accept\n");
    struct sockaddr_un servaddr;
    IPCTunnel* tunnel = (IPCTunnel*)_tunnel;

    // printf("  Tunnel name: %s\n",               tunnel->tunnal_name.c_str());
    // printf("  Tunnel Server_MP_ListenFd: %d\n", tunnel->Server_MP_ListenFd);
    // printf("  Tunnel Server_MP_ConnFd: %d\n",   tunnel->Server_MP_ConnFd);
    // printf("  Tunnel Server_DP_ListenFd: %d\n", tunnel->Server_DP_ListenFd);
    // printf("  Tunnel Server_DP_ConnFd: %d\n",   tunnel->Server_DP_ConnFd);
    // printf("  Tunnel Client_MP_SocketFd: %d\n", tunnel->Client_MP_SocketFd);
    // printf("  Tunnel Client_DP_SocketFd: %d\n", tunnel->Client_DP_SocketFd);


    int mp_exist = access(("/tmp/"+tunnel->tunnal_name+"_MP").c_str(), F_OK);
    int dp_exist = access(("/tmp/"+tunnel->tunnal_name+"_DP").c_str(), F_OK);
    // printf("%s exist: %d\n", ("/tmp/"+tunnel->tunnal_name+"_MP").c_str(), mp_exist);
    // printf("%s exist: %d\n", ("/tmp/"+tunnel->tunnal_name+"_DP").c_str(), dp_exist);

    if(mp_exist == 0){
        while(1){
            int res = remove(("/tmp/"+tunnel->tunnal_name+"_MP").c_str());
            if(res < 0){
                continue;
            }else{
                break;
            }
        }
    }
    if(dp_exist == 0){
        while(1){
            int res = remove(("/tmp/"+tunnel->tunnal_name+"_DP").c_str());
            if(res < 0){
                continue;
            }else{
                break;
            }
        }
    }

    /// 填充结构体servaddr信息
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, ("/tmp/"+tunnel->tunnal_name+"_MP").c_str());
    int len = offsetof(struct sockaddr_un, sun_path) + strlen(("/tmp/"+tunnel->tunnal_name+"_MP").c_str());
	/// 2.绑定IP和端口信息
	// ret = bind(tunnel->Server_MP_ListenFd, (struct sockaddr*)&servaddr, sizeof(servaddr)); 
    ret = bind(tunnel->Server_MP_ListenFd, (struct sockaddr*)&servaddr, len); 
	if(ret==-1){ 

		perror("cannot bind Server_MP socket"); 
		close(tunnel->Server_MP_ListenFd); 
		// unlink(UNIX_DOMAIN); 
		return NULL; 
  	} 
	// printf("bind Server_MP success....\n");

	/// 3.设置端口监听  
	ret = listen(tunnel->Server_MP_ListenFd, 1); 
	if(ret==-1){ 
		perror("Server_MP cannot listen the client connect request"); 
		close(tunnel->Server_MP_ListenFd); 
		// unlink(UNIX_DOMAIN); 
		return NULL; 
	}
    // printf("listen Server_MP success....\n");

    /// 填充结构体servaddr信息
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, ("/tmp/"+tunnel->tunnal_name+"_DP").c_str());
    len = offsetof(struct sockaddr_un, sun_path) + strlen(("/tmp/"+tunnel->tunnal_name+"_DP").c_str());
	/// 2.绑定IP和端口信息
	// ret = bind(tunnel->Server_DP_ListenFd, (struct sockaddr*)&servaddr, sizeof(servaddr)); 
    ret = bind(tunnel->Server_DP_ListenFd, (struct sockaddr*)&servaddr, len); 
	if(ret==-1){ 
		perror("cannot bind Server_DP socket"); 
		close(tunnel->Server_DP_ListenFd); 
		// unlink(UNIX_DOMAIN); 
		return NULL; 
  	} 
	// printf("bind Server_DP success....\n");

	/// 3.设置端口监听  
	ret = listen(tunnel->Server_DP_ListenFd, 1); 
	if(ret==-1){ 
		perror("Server_DP cannot listen the client connect request"); 
		close(tunnel->Server_DP_ListenFd); 
		// unlink(UNIX_DOMAIN); 
		return NULL; 
	}
    // printf("listen Server_DP success....\n");

    struct sockaddr_un mp_cli;
    socklen_t peerlen;
    peerlen = sizeof(struct sockaddr_un);
    while(1){   
        // printf("MP accept... \n");
		/// 4.主线程中等待客户端连接
        tunnel->Server_MP_ConnFd = accept(tunnel->Server_MP_ListenFd, (struct sockaddr *)&mp_cli, &peerlen);
		if(tunnel->Server_MP_ConnFd < 0){ 
			// printf("TUNNEL %s: \n", tunnel->tunnal_name.c_str()); 
            // perror("MP cannot accept client connect request"); 
			// close(tunnel->Server_MP_ConnFd); 
			// unlink(UNIX_DOMAIN); 
			// return NULL; 
		}else{
            break;
        }
        usleep(100*1000); 
    }
    printf("MP got  one client >>>\n");
    // printf("  remote sun_path: %s\n", std::string(mp_cli.sun_path).c_str());
    // for(int i=0; i<20; i++){
    //     printf("%c\n", mp_cli.sun_path[i]);
    // }
    // printf("\n");

    struct sockaddr_un dp_cli;
    while(1){
        // printf("DP accept... \n");
		/// 4.主线程中等待客户端连接
        tunnel->Server_DP_ConnFd = accept(tunnel->Server_DP_ListenFd, (struct sockaddr *)&dp_cli, &peerlen);
		if(tunnel->Server_DP_ConnFd < 0){ 
			perror("DP cannot accept client connect request"); 
			// close(tunnel->Server_DP_ConnFd); 
			// unlink(UNIX_DOMAIN); 
			// return NULL; 
		}else{
            break;
        }
        usleep(100*1000);
    }
    printf("DP got  one client >>>\n");
    // printf("  remote sun_path: %s\n", std::string(dp_cli.sun_path).c_str());
    // for(int i=0; i<20; i++){
    //     printf("%c\n", dp_cli.sun_path[i]);
    // }
    // printf("\n");
    tunnel->ready = true;


    printf("  Tunnel name: %s\n",               tunnel->tunnal_name.c_str());
    printf("  Tunnel Server_MP_ListenFd: %d\n", tunnel->Server_MP_ListenFd);
    printf("  Tunnel Server_MP_ConnFd: %d\n",   tunnel->Server_MP_ConnFd);
    printf("  Tunnel Server_DP_ListenFd: %d\n", tunnel->Server_DP_ListenFd);
    printf("  Tunnel Server_DP_ConnFd: %d\n",   tunnel->Server_DP_ConnFd);
    printf("  Tunnel Client_MP_SocketFd: %d\n", tunnel->Client_MP_SocketFd);
    printf("  Tunnel Client_DP_SocketFd: %d\n", tunnel->Client_DP_SocketFd);
    printf("  Tunnel Ready: %d\n", tunnel->ready);
}

/**
 * @brief 获取服务端图传描述符
 * @param tunnel_name tunnel名
 * @return int 
 */
int IPC_UnixTunnel::Srv_GetMPFd(std::string tunnel_name)
{
    for(int i=0; i<ipc_tunnel.size(); i++){
        if(ipc_tunnel[i].tunnal_name == tunnel_name){
            return ipc_tunnel[i].Server_MP_ConnFd;
        }
    }
}

/**
 * @brief 获取服务端数传描述符
 * @param tunnel_name 
 * @return int 
 */
int IPC_UnixTunnel::Srv_GetDPFd(std::string tunnel_name)
{
    for(int i=0; i<ipc_tunnel.size(); i++){
        if(ipc_tunnel[i].tunnal_name == tunnel_name){
            return ipc_tunnel[i].Server_DP_ConnFd;
        }
    }
}

/**
 * @brief 服务端多媒体数据接收
 * @param fd 
 * @param buffer 
 * @param len 
 * @param flags 
 * @return int 
 */
int IPC_UnixTunnel::Srv_MPRecv(int fd, char* buffer, int len, int flags)
{
    // int RecvLen = recv(fd, buffer, len, flags);
    // return  RecvLen;

	int RecvLen;
	do {
		RecvLen = recv(fd, buffer, len, 0);
		// RecvLen = recv(connfd, (char*)buffer, 1, 0); ///< 一次接收一个字节，也立即返回
	} while ((RecvLen == -1) && (errno == EINTR)); ///< EINTR: 被中断的系统调用  重新读取即可
	
	// printf("RecvLen = %d\n", RecvLen);

	if (RecvLen == -1){
		if (errno != EAGAIN) ///< 处于连接状态，但系统并未指示EAGAIN，主动断开连接
		printf("Accidental disconnection...\n");
	}else if (RecvLen == 0){ ///< recv返回0，对端已关闭
		printf("Client closed...\n");
	}

    return RecvLen;
}

/**
 * @brief 服务端多媒体数据发送
 * @param fd 
 * @param buffer 
 * @param len 
 * @param flags 
 * @return int 
 */
int IPC_UnixTunnel::Srv_MPSend(int fd, char* buffer, int len, int flags)
{
    int SendLen = send(fd, buffer, len, flags);
    return SendLen;
}

/**
 * @brief 服务端通用数据接收
 * @param fd 
 * @param buffer 
 * @param len 
 * @param flags 
 * @return int 
 */
int IPC_UnixTunnel::Srv_DPRecv(int fd, char* buffer, int len, int flags)
{
	int RecvLen;
	do {
		RecvLen = recv(fd, buffer, len, flags);
		// RecvLen = recv(connfd, (char*)buffer, 1, 0); ///< 一次接收一个字节，也立即返回
	} while ((RecvLen == -1) && (errno == EINTR)); ///< EINTR: 被中断的系统调用  重新读取即可
	
	// printf("RecvLen = %d\n", RecvLen);

	if (RecvLen == -1){
		if (errno != EAGAIN) ///< 处于连接状态，但系统并未指示EAGAIN，主动断开连接
		printf("Accidental disconnection...\n");
	}else if (RecvLen == 0){ ///< recv返回0，对端已关闭
		printf("Client closed...\n");
	}

    return RecvLen;
}

/**
 * @brief 服务端通用数据发送
 * @param fd 
 * @param buffer 
 * @param len 
 * @param flags 
 * @return int 
 */
int IPC_UnixTunnel::Srv_DPSend(int fd, char* buffer, int len, int flags)
{
    int SendLen = send(fd, buffer, len, flags);
    return SendLen;
}





/**
 * @brief 
 * @return int 
 */
int IPC_UnixTunnel::Cli_Socket()
{
    /// 1.创建套接字
    struct sockaddr_un sin;

    for(int i=0; i<ipc_tunnel.size(); i++){
        if(ipc_tunnel[i].tunnal_name != "NULL"){
            // printf("ipc_tunnel %d already configured\n", i);

            if( -1 == (ipc_tunnel[i].Client_MP_SocketFd = socket(AF_UNIX, SOCK_STREAM, 0))){  
                perror("MP create socket failed");
                return -1;
            }
            bzero(&sin, sizeof(sin));
            sin.sun_family = AF_UNIX;
            strcpy(sin.sun_path, ("/tmp/"+ipc_tunnel[i].tunnal_name+"_MP").c_str());
            int len = offsetof(struct sockaddr_un, sun_path) + strlen(("/tmp/"+ipc_tunnel[i].tunnal_name+"_MP").c_str());
            /// 2.请求连接目标服务器端口
            if( -1 == connect(ipc_tunnel[i].Client_MP_SocketFd, (struct sockaddr*)&sin, len) ){
                perror("connect failed");
                return -1;
            }



            if( -1 == (ipc_tunnel[i].Client_DP_SocketFd = socket(AF_UNIX, SOCK_STREAM, 0))){  
                perror("DP create socket failed");
                return -1;
            }
            bzero(&sin, sizeof(sin));
            sin.sun_family = AF_UNIX;
            strcpy(sin.sun_path, ("/tmp/"+ipc_tunnel[i].tunnal_name+"_DP").c_str());
            len = offsetof(struct sockaddr_un, sun_path) + strlen(("/tmp/"+ipc_tunnel[i].tunnal_name+"_DP").c_str());
            /// 2.请求连接目标服务器端口
            if( -1 == connect(ipc_tunnel[i].Client_DP_SocketFd, (struct sockaddr*)&sin, len) ){
                perror("connect failed");
                return -1;
            }	
        }
    }
    // printf("Client socket create successful\n");



    return 0;
}

/**
 * @brief 
 * @param tunnel_name 
 * @return int 
 */
int IPC_UnixTunnel::Cli_GetMPFd(std::string tunnel_name)
{
    for(int i=0; i<ipc_tunnel.size(); i++){
        if(ipc_tunnel[i].tunnal_name == tunnel_name){
            return ipc_tunnel[i].Client_MP_SocketFd;
        }
    }
}

/**
 * @brief 
 * @param tunnel_name 
 * @return int 
 */
int IPC_UnixTunnel::Cli_GetDPFd(std::string tunnel_name)
{
    for(int i=0; i<ipc_tunnel.size(); i++){
        if(ipc_tunnel[i].tunnal_name == tunnel_name){
            return ipc_tunnel[i].Client_DP_SocketFd;
        }
    }
}

/**
 * @brief 
 * @param fd 
 * @param buffer 
 * @param len 
 * @param flags 
 * @return int 
 */
int IPC_UnixTunnel::Cli_MPRecv(int fd, char* buffer, int len, int flags)
{
	int RecvLen;
	do {
		RecvLen = recv(fd, buffer, len, 0);
		// RecvLen = recv(connfd, (char*)buffer, 1, 0); ///< 一次接收一个字节，也立即返回
	} while ((RecvLen == -1) && (errno == EINTR)); ///< EINTR: 被中断的系统调用  重新读取即可
	
	// printf("RecvLen = %d\n", RecvLen);

	if (RecvLen == -1){
		if (errno != EAGAIN) ///< 处于连接状态，但系统并未指示EAGAIN，主动断开连接
		printf("Accidental disconnection...\n");
	}else if (RecvLen == 0){ ///< recv返回0，对端已关闭
		printf("Client closed...\n");
	}

    return RecvLen;
}

/**
 * @brief 
 * @param fd 
 * @param buffer 
 * @param len 
 * @param flags 
 * @return int 
 */
int IPC_UnixTunnel::Cli_MPSend(int fd, char* buffer, int len, int flags)
{
    int SendLen = send(fd, buffer, len, flags);
    return SendLen;
}

/**
 * @brief 
 * @param fd 
 * @param buffer 
 * @param len 
 * @param flags 
 * @return int 
 */
int IPC_UnixTunnel::Cli_DPRecv(int fd, char* buffer, int len, int flags)
{
	int RecvLen;
	do {
		RecvLen = recv(fd, buffer, len, 0);
		// RecvLen = recv(connfd, (char*)buffer, 1, 0); ///< 一次接收一个字节，也立即返回
	} while ((RecvLen == -1) && (errno == EINTR)); ///< EINTR: 被中断的系统调用  重新读取即可
	
	// printf("RecvLen = %d\n", RecvLen);

	if (RecvLen == -1){
		if (errno != EAGAIN) ///< 处于连接状态，但系统并未指示EAGAIN，主动断开连接
		printf("Accidental disconnection...\n");
	}else if (RecvLen == 0){ ///< recv返回0，对端已关闭
		printf("Client closed...\n");
	}

    return RecvLen;
}

/**
 * @brief 
 * @param fd 
 * @param buffer 
 * @param len 
 * @param flags 
 * @return int 
 */
int IPC_UnixTunnel::Cli_DPSend(int fd, char* buffer, int len, int flags)
{
    int SendLen = send(fd, buffer, len, flags);
    return SendLen;
}

/**
 * @brief Tunnel状态查询
 * @return true IPC-Unix-Tunnel创建就绪
 * @return false IPC-Unix-Tunnel创建未就绪
 */
bool IPC_UnixTunnel::TunnelCheck()
{
    int tunnels = 0;
    for(int i=0; i<ipc_tunnel.size(); i++){
        if(ipc_tunnel[i].tunnal_name != "NULL"){
            if(ipc_tunnel[i].ready == true){
                tunnels++;
            }
        }   
    } 

    if(busy_tunnels == tunnels){
        return true;
    }else{
        return false;
    }
}

/**
 * @brief 关闭IUT
 * @param tunnel_name 
 * @return int 
 */
int IPC_UnixTunnel::CloseTunnel(std::string tunnel_name)
{
    int res = -1;
    for(int i=0; i<ipc_tunnel.size(); i++){
        if(ipc_tunnel[i].tunnal_name == tunnel_name){
            printf("Clsoe tunnel\n");
            res = CloseServer(tunnel_name);
            if(res<0){
                return res;
            }
            res = CloseClient(tunnel_name);
            if(res<0){
                return res;
            }
            return 0;
        }
    }    
}

/**
 * @brief 关闭服务端IUT
 * @param tunnel_name 
 * @return int 
 */
int IPC_UnixTunnel::CloseServer(std::string tunnel_name)
{
    int tunnel_idx = -1;
    for(int i=0; i<ipc_tunnel.size(); i++){
        if(ipc_tunnel[i].tunnal_name == tunnel_name){
            tunnel_idx = i;
        }
    }

    if(tunnel_idx == -1){ ///< 未找到相关的ipc tunnel
        return -1;
    }
    
    printf("Clsoe server\n");
    close(ipc_tunnel[tunnel_idx].Server_MP_ListenFd);
    close(ipc_tunnel[tunnel_idx].Server_MP_ConnFd);
    close(ipc_tunnel[tunnel_idx].Server_DP_ListenFd);
    close(ipc_tunnel[tunnel_idx].Server_DP_ConnFd);

    return 0;
}

/**
 * @brief 关闭客户端IUT
 * @param tunnel_name 
 * @return int 
 */
int IPC_UnixTunnel::CloseClient(std::string tunnel_name)
{
    int tunnel_idx = -1;
    for(int i=0; i<ipc_tunnel.size(); i++){
        if(ipc_tunnel[i].tunnal_name == tunnel_name){
            tunnel_idx = i;
        }
    }
    
    if(tunnel_idx == -1){ ///< 未找到相关的ipc tunnel
        return -1;
    }

    printf("Clsoe client\n");
    close(ipc_tunnel[tunnel_idx].Client_MP_SocketFd);
    close(ipc_tunnel[tunnel_idx].Client_DP_SocketFd);

    return 0;
}

/**
 * @brief 评估打印(仅用于开发、测试)
 */
void IPC_UnixTunnel::eval_display()
{
    for(int i=0; i<ipc_tunnel.size(); i++){
        printf("Tunnel %d\n", i);
        printf("  Tunnel name: %s\n",               ipc_tunnel[i].tunnal_name.c_str());
        printf("  Tunnel Server_MP_ListenFd: %d\n", ipc_tunnel[i].Server_MP_ListenFd);
        printf("  Tunnel Server_MP_ConnFd: %d\n",   ipc_tunnel[i].Server_MP_ConnFd);
        printf("  Tunnel Server_DP_ListenFd: %d\n", ipc_tunnel[i].Server_DP_ListenFd);
        printf("  Tunnel Server_DP_ConnFd: %d\n",   ipc_tunnel[i].Server_DP_ConnFd);
        printf("  Tunnel Client_MP_SocketFd: %d\n", ipc_tunnel[i].Client_MP_SocketFd);
        printf("  Tunnel Client_DP_SocketFd: %d\n", ipc_tunnel[i].Client_DP_SocketFd);
    }
}