#include <iostream>
#include <sys/socket.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/time.h>
using namespace std;

#define TRANMIST_TIMES 10
#define PACKET_SIZE 1024
char sendBuf[PACKET_SIZE];
char recvBuf[PACKET_SIZE];
u_short in_cksum(const u_short *addr, register int len, u_short csum)
{
    register int nleft = len;
    const u_short *w = addr;
    register u_short answer;
    register int sum = csum;

    /*
     *  Our algorithm is simple, using a 32 bit accumulator (sum),
     *  we add sequential 16 bit words to it, and at the end, fold
     *  back all the carry bits from the top 16 bits into the lower
     *  16 bits.
     */
    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (nleft == 1)
        sum += htons(*(u_char *)w << 8);

    /*
     * add back carry outs from top 16 bits to low 16 bits
     */
    sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
    sum += (sum >> 16);			/* add carry */
    answer = ~sum;				/* truncate to 16 bits */
    return (answer);
}

double calcualteRTT(const struct timeval* curTime, const struct timeval* lastTime )
{
    double ms = (curTime->tv_sec - lastTime->tv_sec) * 1000;
    ms = ms + (curTime->tv_usec - lastTime->tv_usec) / 1000;
    return ms;
}
// return 0 means succed
// return 1 means time out
// return 2 means receive last packet
// return 3 means receive other process's icmp echo
// return 4 means system error
int recvPacket(int icmp_socket, const struct sockaddr_in& destAddr, int seq)
{
    //printf("%d:%s\n", icmp_socket, inet_ntoa(destAddr.sin_addr));
    fd_set rdSet;
    FD_ZERO(&rdSet);
    FD_SET(icmp_socket, &rdSet);
    struct timeval timeOut;
    timeOut.tv_sec = 5;
    timeOut.tv_usec = 0;
    int ct = select(icmp_socket + 1, &rdSet, NULL, NULL, &timeOut);
//			如果等待超时，则输出超时提示并返回则返回上一步，继续发送下一个ICMP探测
    if(ct == 0)
    {
        printf("icmp secquence %d time out\n", seq);
        return 1;
    }
    if(ct < 0)
    {
        printf("system error!\n");
        return 4;
    }
//			如果在限定的时间内有数据返回，则调用recvPacket来获取返回的数据。
    if(FD_ISSET(icmp_socket, &rdSet))
    {
        socklen_t addrLen = sizeof(destAddr);
        //	调用系统API获取ICMP回答报文
        int dataLen = recvfrom(icmp_socket, recvBuf, PACKET_SIZE, 0, (struct sockaddr *)&destAddr, &addrLen);
        if(dataLen == 0)
        {
            printf("fail to recvive packet %d\n", seq);
            return 4;
        }
//	获取ip报文头的长度，并跳IP头。获取ICMP报文的开始地址
        struct iphdr* pIP = (struct iphdr*)recvBuf;
        int ipHdrLth = (pIP->ihl) * 4;
        // icmp part
        //int icmpLength = dataLen - ipHdrLth;
        struct icmphdr* pIcmp = (struct icmphdr*)(recvBuf + ipHdrLth);
        if(pIcmp->un.echo.id == getpid())
        {
            //	如果接收到的数据是当前等待的数据
            if(pIcmp->un.echo.sequence == seq)
            {
                //  输出往返时间和TTL
                struct timeval* pLastTime, curTime;
                gettimeofday(&curTime, NULL);
                pLastTime = (struct timeval*)(pIcmp + 1);
                double rtt = calcualteRTT(&curTime, pLastTime);
                printf("%d byte from %s: icmp_seq=%u ttl=%d rtt=%.3f ms\n",
                       dataLen, inet_ntoa(destAddr.sin_addr), seq, pIP->ttl, rtt);
                return 0;
            }
            // if receiv the last packet, return 2
            else if(pIcmp->un.echo.sequence < seq)
            {
                return 2;
            }
        }
        else  //	判断报文的ID，如果不与进程ID相等则返回3
            return 3;
    }
    return 4;
}

int sendPacket(int icmp_socket, const struct sockaddr_in& destAddr, int seq)
{
    bzero(sendBuf, PACKET_SIZE);
//	封装ICMP数据报的头部
    struct icmphdr* pIcmp = (icmphdr*)sendBuf;
//		ICMP报文类型ICMP_ECHO
    pIcmp->type = ICMP_ECHO;
//		ICMP代码类型0
    pIcmp->code = 0;
//		ICMP报文序列号
    pIcmp->un.echo.sequence = seq;
//		ICMP报文ID
    pIcmp->un.echo.id = getpid();
//	}
//	获取当前时间，并将当前时间填入ICMP报文的数据段中
    struct timeval curTime;
    gettimeofday(&curTime, NULL);
    memcpy(pIcmp + 1, &curTime, sizeof(curTime));
//	计算报文校验和，并将其填入ICMP报文的校验和字段中
    pIcmp->checksum = in_cksum((u_short*)pIcmp, 64, 0);
//	调用系统API发送封装好的ICMP报文
    if(sendto(icmp_socket, sendBuf, 64, 0, (struct sockaddr*)&destAddr, sizeof(destAddr)) != 64)
    {
        // fail
        return 0;
    }
    else
        return 1;
}

void main_loop(int icmp_socket, const struct sockaddr_in& destAddr)
{
//	依次向目标主机发送TRANSMIT_TIME次数据
    int timeOutCt = 0;
    int succedCt = 0;
    int failCt = 0;
    for(int i = 1; i <= TRANMIST_TIMES; ++i)
    {
//		调用sendPacket发送一次ICMP探测
        if(!sendPacket(icmp_socket, destAddr, i))
        {
            // 发送失败
            printf("destination host can't receive icmp packet\n");
            exit(1);
        }
//		定时等待目标主机的回答报文
//         return 0 means succed
//         return 1 means time out
//         return 2 means receive last packet
//         return 3 means receive other process's icmp echo
//         return 4 means system error
//        printf("Ping %s 64 bytes of data\n", inet_ntoa(destAddr.sin_addr));
        int rt;
        while((rt = recvPacket(icmp_socket, destAddr, i)) != 2)
        {
            if(rt == 0)
            {
                ++succedCt;
                break;
            }
            else if(rt == 1)
            {
                ++timeOutCt;
                break;
            }
            else if(rt == 3)
            {
                ++failCt;
            }
        }
    }
    printf("succed:%d\tsucced rate:%f\%%", succedCt, (double (succedCt * 100)) / TRANMIST_TIMES);
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
    if(inet_pton(AF_INET, argv[1], &destAddr.sin_addr) <= 0)
    {
//      如果输入的是域名地址，则先获取主机的信息，之后再转换成二进制IP地址
        struct hostent* pHost;
        pHost = gethostbyname(argv[1]);
        if(!pHost)
        {
            fprintf(stderr, "ping: unknown host %s\n", argv[1]);
            return 2;
        }
        memcpy((char*)&destAddr.sin_addr, pHost->h_addr_list[0], 4);
        char buf[10];
        bzero(buf, 10);
        printf("Ping %s(%s) 64 bytes of data\n", pHost->h_name, inet_ntoa(destAddr.sin_addr));
    }
    else
    {
        printf("Ping %s 64 bytes of data\n", argv[1]);
    }

//	调用main_loop循环发送和接收ICMP消息
    main_loop(icmp_socket, destAddr);
}
