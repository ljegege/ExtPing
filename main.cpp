#include <iostream>
#include <sys/socket.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include
using namespace std;

#define TRANMIST_TIMES 10
#define PACKET_SIZE 1024
int recvPacket(int icmp_socket, const struct sockaddr_in& destAddr, int seq, char* recvBuf, int bufSzie)
{
//	调用系统API获取ICMP回答报文
//	获取ip报文头的长度，并跳IP头。获取ICMP报文的开始地址
//	校验ICMP报文
//
//	判断报文的ID，如果不与进程ID相等则返回0
//
//	判断报文的序号
//	如果为当前等待的报文，则返回1
//	如果为之前的报文则返回2
}

int sendPacket(int icmp_socket, const struct sockaddr_in& destAddr)
{
//	封装ICMP数据报的头部
//	{
//		ICMP报文类型ICMP_ECHO
//		ICMP代码类型0
//		ICMP报文序列号
//		ICMP报文ID
//	}
//	获取当前时间，并将当前时间填入ICMP报文的数据段中
//	计算报文校验和，并将其填入ICMP报文的校验和字段中
//	调用系统API发送封装好的ICMP报文
}

void main_loop(int icmp_socket, const struct sockaddr_in& destAddr)
{
//	依次向目标主机发送TRANSMIT_TIME次数据
    int icmpSecquence = 0;
    char recvBuf[PACKET_SIZE];
    for(int i = 1; i < TRANMIST_TIMES; ++i)
	{
//		调用sendPacket发送一次ICMP探测
        if(!sendPacket(icmp_socket, destAddr))
        {
            // 发送失败
        }
//		定时等待目标主机的回答报文
        while(1)
        {
            fd_set rdSet;
            FD_ZERO(&rdSet);
            FD_SET(icmp_socket, &rdSet);
            struct timeval timeOut;
            timeOut.tv_sec = 1;
            timeOut.tv_usec = 0l
            int ct = select(icmp_socket + 1, &rdSet, NULL, NULL, &timeOut);
//			如果等待超时，则输出超时提示并返回则返回上一步，继续发送下一个ICMP探测
            if(ct == 0)
            {
                printf("icmp secquence %d time out\n", i);
                break;
            }
            if(ct < 0)
            {
                printf("system error!\n");
                exit(2);
            }
//			如果在限定的时间内有数据返回，则调用recvPacket来获取返回的数据。
            if(FD_ISSET(icmp_socket, &rdSet))
			{
                int addrLen = sizeof(destAddr);
                int dataLen = recvfrom(icmp_socket, recvBuf, PACKET_SIZE, 0, (struct sockaddr *)&destAddr, &addrLen);
                if(dataLen == 0)
                {
                    printf("fail to recvive packet %d\n", i);
                    break;
                }
//				如果接收到的数据是当前等待的数据
//				{
//					输出往返时间和TTL
//					返回上一步继续下一次的探测报文发送
//				}
//				如果接收到的数据是之前的ICMP探测的回答报文则继续等待当前的回答报文
			}
        }
	}
}

int main(int argc, char*argv[])
{
//  socket接收缓冲区的大小
    int socketBufSize = 10 * 1024;
//  目的主机地址
    struct sockaddr_in destAddr;
//	创建ICMP的socket
    int icmp_socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
//	如果不成功
    if(icmp_socket < 0)
    {
//	　　 提示用户创建raw socket 需要超级权限
        perror("Ping:open icmp socket");
        return 2;
    }

//	配置socket的配置选项：缓冲区的大小
    if(setsockopt(icmp_socket, SOL_SOCKET, SO_RCVBUF, &socketBufSize, sizeof(socketBufSize)))
    {
        perror("set socket buf");
        return 2;
    }
//	回收超级权限
    setuid(getuid());
//	检测参数个数是否正确，如果不正确则提示用户本程序的使用方法
    if(argc != 2)
    {
        printf("usage:%s hostname/IP address\n",argv[0]);
        return 2;
    }
//	设置目的IP地址
//	如果输入的是点分十进制的IP地址，则直接转换成二进制IP地址
    bzero(&destAddr, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    if(inet_pton(AF_INET, argv[1], &destAddr) <= 0)
    {
//      如果输入的是域名地址，则先获取主机的信息，之后再转换成二进制IP地址
        struct hostent* pHost;
        pHost = gethostbyname(argv[1]);
        if(!pHost)
        {
            fprintf(stderr, "ping: unknown host %s\n", argv[1]);
            return 2;
        }
        memccpy(destAddr.sin_addr, pHost->h_addr_list[0], 4);
    }
//	调用main_loop循环发送和接收ICMP消息
    main_loop(icmp_socket, destAddr);
}
