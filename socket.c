/*************************************************************************
	> File Name: socket.c
	> Author: 
	> Mail: 
	> Created Time: 2016年09月10日 星期六 15时35分34秒
 ************************************************************************/

#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<fcntl.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<sys/time.h>
#include<string.h>
#include<unistd.h>
#include<stdlib.h>
#include<stdarg.h>

/*
 * 通过地址和端口建立网络连接
 */
int Socket(const char *host, int clientPort)
{
    int sock;
    unsigned long inaddr;
    struct sockaddr_in ad;
    struct hostent *hp;

    memset(&ad, 0, sizeof(ad));
    ad.sin_family = AF_INET;

    inaddr = inet_addr(host);  //将点分十进制的IP转为无符号长整数
    if(inaddr != INADDR_NONE)
        memcpy(&ad.sin_addr, &inaddr, sizeof(inaddr));
    else  //如果host是域名
    {
        hp = gethostbyname(host);  //用域名获取IP
        if(hp == NULL)
            return -1;
        memcpy(&ad.sin_addr, hp->h_addr, hp->h_length);
    }

    ad.sin_port = htons(clientPort);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
        return sock;
    if(connect(sock, (struct sockaddr*)&ad, sizeof(ad)) < 0)
        return -1;
    return sock;
}
