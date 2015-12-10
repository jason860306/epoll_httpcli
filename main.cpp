#include <stdio.h> // for printf
#include <errno.h> // for errno
#include <string.h> // for strerror
#include <assert.h> // for assert
#include <iostream>
#include <sstream>
#include <fcntl.h> // for fcntl
#include <sys/socket.h> //for socket
#include <sys/epoll.h> // for epoll_*
#include <netinet/in.h>// for sockaddr_in
#include <arpa/inet.h>
#include "uriparser.h"

#define HTTP_REQ_URL "http://182.18.58.2:8095/stream//output/AACA71E555DBCF947318D54F5D73FC7A_270174_480P_optimizateProcess.mp4"
//#define HTTP_REQ_URL "http://182.18.58.2:8095/stream//output/989C7F2B44312693A5530C7F02C04033_263947_1080P_optimizateProcess.bhd"
//#define HTTP_REQ_URL "http://192.168.200.15/mxupsrv.pcap"

const int MaxSize = 64000;
const int MaxEps = 1024;
const int ListenEnq = 512;
const int EventsSize = 1;

/* set file descriptor to non-block mode */
int SetNonBlock(int sock)
{
    // get sock fd flags;
    int flags = fcntl(sock, F_GETFL);
    if(flags == -1)
    {
        printf("fcntl get error(%d): %s", errno, strerror(errno));
        perror("fcntl get error/n");
        return -1;
    }
    //set fd's flag to non-block.
    flags |= O_NONBLOCK;
    flags = fcntl(sock, F_SETFL, flags);
    if(flags == -1)
    {
        printf("fcntl get error(%d): %s", errno, strerror(errno));
        perror("fcntl set error/n");
        return -2;
    }
    return 0;
}

/* main entrance */
int main()
{
    std::cout << "epoll client sample" << std::endl; // prints epoll sample
//    EnableDebugMode(true);

    FILE *down_file = fopen("down_file.bin", "wb+");
    assert(NULL != down_file);

    CUriParser uriParser(HTTP_REQ_URL);

    //create epoll fd
    int epfd = epoll_create(MaxEps);
    //add fd to epfd
    struct epoll_event ev, events[EventsSize];
    struct sockaddr_in clientAddr;
    bzero(&clientAddr, sizeof(clientAddr));
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(uriParser.getPort());
    clientAddr.sin_addr.s_addr = inet_addr(uriParser.getHost().c_str());
    for (int i = 0 ; i < EventsSize; i ++)
    {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if(sockfd == -1)
        {
            printf("Create socket failed.");
            continue;
        }
        //connect
        int ret = connect(sockfd, (sockaddr*)&clientAddr, sizeof(sockaddr_in));
        if(ret == 0)
        {
            events[i].data.fd = sockfd;
            events[i].events = EPOLLOUT | EPOLLERR/* | EPOLLET*/;
            epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &events[i]);
        }
        else
        {
            events[i].data.fd = -1;
        }
    }

    //begin to accept
    printf("begin to communication.../n");
    char buf[MaxSize];
    for(;;)
    {
//        sleep(5);
        int nfds = epoll_wait(epfd, events, EventsSize, 20000);
        if(nfds == -1)
        {
            if(errno == EINTR)
            {
                continue;
            }
        }
//        printf("How much ? %d", nfds);
        for(int i = 0; i < nfds; i++)
        {
            //accept
            if(events[i].events & EPOLLIN)
            {
                int sockfd = events[i].data.fd;
                if(sockfd < 0) continue;
                //read
                int n = read(sockfd, buf, MaxSize);
                if( n == 0)
                {
                    printf("close socket %d, read = 0", sockfd);
                    close(sockfd);
                    events[i].data.fd = -1;

                    break;
                }
                else if(n < 0)
                {
                    if(errno == ECONNRESET)
                    {
                        printf("close socket %d, read < 0", sockfd);
                        close(sockfd);
                        events[i].data.fd = -1;

                        break;
                    }
                    else
                        printf("read failed");
                }
                fwrite(buf, n, 1, down_file);

////                printf("Read Data(%d bytes) from fd(%d): %s", n, sockfd, buf);
//                // modify fd event.
//                ev.data.fd = events[i].data.fd;
//                ev.events = EPOLLIN | EPOLLERR/* | EPOLLET*/;
//                epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
            }
            //send data out.
            else if(events[i].events & EPOLLOUT)
            {
                int sockfd = events[i].data.fd;

                std::ostringstream ostrm;
                ostrm << "GET " << uriParser.getPathEtc() << " HTTP/1.1\r\n";
                ostrm << "HOST: " << uriParser.getHost() << "\r\n";
                ostrm << "Range: bytes=0-" << 4194304 << "\r\n";
                ostrm << "User-Agent: epollcli\r\n";
                ostrm << "Accept: */*\r\n\r\n";

                printf("send http request: \r\n%s", ostrm.str().c_str());

                write(sockfd, ostrm.str().data(), ostrm.str().size());
                printf("Send data to fd(%d)", sockfd);
                ev.data.fd = sockfd;
                ev.events = EPOLLIN | EPOLLERR/* | EPOLLET*/;
                epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
            }
        }
    }
    for( int i = 0; i < EventsSize; i++)
    {
        int sockfd = events[i].data.fd;
        if( sockfd > 0)
        {
            close(sockfd);
            events[i].data.fd = -1;
        }
    }
    fflush(down_file);
    fclose(down_file);
    return 0;
}
