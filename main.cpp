#include <chrono>
#include "IPC_UnixTunnel.h"

using namespace std::chrono;

#define BUFFER_LEN (1920*1080*3)
FILE* SamplePicFP = NULL;
std::string SamplePicFile = "/share/039_IPC_Loopback/1080p.rgb888";
FILE* SamplePicTransFPO = NULL;
std::string SamplePicTransDir = "/share/039_IPC_Loopback/samples/";

IPC_UnixTunnel* ipc_unixtunnel = NULL;
char* mainprocess_buffer = NULL;
char* subprocess_buffer = NULL;

/// 系统信号捕捉
pthread_t SecondMark_tid;
void* SecondMark(void* args);

/// 计时打标
void Sig_Handler(int sig){
    printf("\n\nRecv signal: %d\n", sig);
	ipc_unixtunnel->CloseTunnel("UV");
    delete ipc_unixtunnel;
    ipc_unixtunnel = NULL;

    free(mainprocess_buffer);
    mainprocess_buffer = NULL;
    free(subprocess_buffer);
    subprocess_buffer = NULL;

    exit(1); 
}

int main(int argc, char* argv[])
{
	signal(SIGINT, Sig_Handler);
    // pthread_create(&SecondMark_tid, NULL, SecondMark, NULL);

    /// 1. 创建tunnel
    int tunnels = 2;
    ipc_unixtunnel = new IPC_UnixTunnel(tunnels);
    
    /// 2. 配置tunnel名
    ipc_unixtunnel->config_tunnel(0, "UV");
    ipc_unixtunnel->config_tunnel(1, "IR");

    /// 3. 初始化通道
    ipc_unixtunnel->Srv_Socket();
    usleep(50*1000);
    ipc_unixtunnel->Cli_Socket();

    /// 4. 等待tunnel初始化完成
    while(1){
        bool tunnel_ready = ipc_unixtunnel->TunnelCheck();
        if(tunnel_ready == true){
            printf("All tunnels was ready\n");
            break;
        }else{
            printf("tunnel not ready\n");
            sleep(1);
        }
    }


    SamplePicFP = fopen(SamplePicFile.c_str(), "rb+");
    // char SamplePicBuffer[1920*1080*3];
    // fseek(SamplePicFP, 0, SEEK_END);
    // int SamplePicLen = ftell(SamplePicFP);
    // printf("File len: %d\n", SamplePicLen);
    // fseek(SamplePicFP, 0, SEEK_SET);
    // int rdsize = fread(SamplePicBuffer, 1, SamplePicLen, SamplePicFP);


    /**
     * 做好上述工作以后，创建进程，然后在每个进程里获取图传和数传的描述符，即可进行相互通信
     */
    pid_t pid = fork(); ///< 创建进程
    if (pid == 0){ ///< 子进程
        /// 5-1: 子进程获取MP和DP描述符
        int MPFd = ipc_unixtunnel->Srv_GetMPFd("IR");
        int DPFd = ipc_unixtunnel->Srv_GetDPFd("IR");
        /// 5-2: 数据传输
        subprocess_buffer = (char*)malloc(BUFFER_LEN);
        while(1){
            // memset(subprocess_buffer, i, BUFFER_LEN);
            fread(subprocess_buffer, 1, BUFFER_LEN, SamplePicFP);
            ipc_unixtunnel->Cli_DPSend(DPFd, subprocess_buffer, BUFFER_LEN);
            fseek(SamplePicFP, 0, SEEK_SET);
        }

    }else if (pid > 0){ ///< 父进程
        /// 6-1: 主进程获取MP和DP描述符
        int MPFd = ipc_unixtunnel->Cli_GetMPFd("IR");
        int DPFd = ipc_unixtunnel->Cli_GetDPFd("IR");
        /// 6-2: 数据传输
        mainprocess_buffer = (char*)malloc(BUFFER_LEN);
        int reseived_len = 0;
        int sample_cnt = 0;
        while(1){
            bzero(mainprocess_buffer, sizeof(mainprocess_buffer));
            reseived_len = ipc_unixtunnel->Srv_DPRecv(DPFd, mainprocess_buffer, BUFFER_LEN, MSG_WAITALL);
            // printf("Server received: %s\n", std::string(mainprocess_buffer).c_str());
            printf("Server received length: %d\n", reseived_len);
            printf("Server received: %d\n", mainprocess_buffer[0]);
            
            /// 本地存储测试
        #if 1
            std::string new_path = SamplePicTransDir + std::to_string(sample_cnt) + ".rgb888";
            SamplePicTransFPO = fopen(new_path.c_str(), "wb");
            if(!SamplePicTransFPO){
                printf("Open saving file failed\n");
            }
            fwrite(mainprocess_buffer, 1, reseived_len, SamplePicTransFPO);
            fclose(SamplePicTransFPO);
            sample_cnt ++;
            if(sample_cnt == 100){
                break;
            }
        #endif

            usleep(33*1000);
        }

    }



    /// Keep alive
    while(1){
        sleep(1);
    }

    return 0;
}



void* SecondMark(void* args)
{
	steady_clock::time_point time_start;
	steady_clock::time_point time_end;
	while(1){
		time_start = steady_clock::now();

		while(1){
			time_end = steady_clock::now();
			auto tt = duration_cast<microseconds>(time_end - time_start);
    		// std::cout << "程序用时=" << tt.count() << "微秒" << std::endl;
			if(tt.count() > 1000000){
				std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>time interval " << std::endl;
				break;
			}
		}
	}
}