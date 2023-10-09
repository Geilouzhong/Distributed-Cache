#ifndef WRAP_H
#define WRAP_H
#include <iostream>
#include <sys/time.h>
#include <fcntl.h>
#include "unistd.h"
#include "arpa/inet.h"
#include "sys/epoll.h"
#include "cerrno"
#include "pthread.h"

struct OneIPList
{
public:
    std::string ip;
    int port;
};

OneIPList GetIPList(std::string cacheip){
    int cut = cacheip.find_last_of(':');
    OneIPList iptest;
    iptest.ip = cacheip.substr(0,cut);
    iptest.port = std::stoi(cacheip.substr(cut+1));
    //std::cout << iptest.ip << " " << iptest.port << std::endl;
    return iptest;
}

std::string Randstr(const int len)
{
    struct timeval timeSeed;
    gettimeofday(&timeSeed,NULL);
    srand(1000000 * timeSeed.tv_sec + timeSeed.tv_usec);
    std::string ans;
    int i;
    for (i = 0; i < len; i++)
    {
        char c;
        switch ((rand() % 3))
        {
        case 1:
            c = 'A' + rand() % 26;
            break;
        case 2:
            c = 'a' + rand() % 26;
            break;
        default:
            c = '0' + rand() % 10;
        }
        ans += c;
    }
    ans[++i] = '\0';
    return ans;
}

void perr_exit(const char* s){
    perror(s);
    exit(-1);
}
int Socket(int family, int type, int protocol){
    int n;
    if((n=socket(family, type, protocol))<0)
        perr_exit("socket error");
    return n;
}
/*
int Connect(int fd, const struct sockaddr* sa, socklen_t salen){
    int n;
    n = connect(fd, sa, salen);
    if(n<0)
        perr_exit("connect error");
    return n;
}*/
int Accept(int fd, struct sockaddr *sa, socklen_t *salenptr){
    int n;
    do{
        n = accept(fd, sa, salenptr);
        if(n>=0)
            return n;
    }while(errno==ECONNABORTED || errno==EINTR);
    perr_exit("accept error");
    return -1;
}
int Bind(int fd, const struct sockaddr *sa, socklen_t salen){
    int n;
    if((n = bind(fd, sa, salen))<0)
        perr_exit("bind error");
    return n;
}
int Listen(int fd, int backlog){
    int n;
    if((n = listen(fd,backlog))<0)
        perr_exit("listen error");
    return n;
}
int Close(int fd){
    int n;
    for(int i=0; i<3; i++){
        n=close(fd);
        if(n>=0)
            break;
        //perr_exit("close error");
    }
    return n;
}
ssize_t Write(int fd, const void* ptr, size_t nbytes){
    ssize_t n;
    do{
        n = write(fd, ptr, nbytes);
        if(n>=0)
            return n;
    }while(errno==EINTR||errno==EAGAIN);
    return -1;
}
int Epoll_create(int size){
    int efd = epoll_create(size);
    if(0>efd)
        perr_exit("epoll_create error");
    return efd;
}
int Epoll_ctl(int epfd, int op, int fd, struct epoll_event *event){
    int res, count=0;
    while(count++<=3){
        res = epoll_ctl(epfd, op, fd, event);
        if(res>=0)
            break;
    }
    /*
    if(0>res)
        perr_exit("epoll_ctl error");*/
    return res;

}
int Epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout){
    int nready = epoll_wait(epfd, events, maxevents, timeout);
    if(0>nready)
        perr_exit("epoll_wait error");
    return nready;
}
void Pthread_create(pthread_t *tid, const pthread_attr_t *attr,\
			   void * (*func)(void *), void *arg){
	int n;
	if ((n=pthread_create(tid, attr, func, arg))==0)
		return;
	errno = n;
	perr_exit("pthread_create error");
}
void Sleep_ms(int n){
    struct timeval delay;
    delay.tv_sec = 0;
    delay.tv_usec = n * 1000;
    select(0, NULL, NULL, NULL, &delay);
}
struct info_conn{
    int cfd;
    int n;
};
info_conn Socket_connect(const struct sockaddr* sa, socklen_t salen){
    int n, fd;
    struct info_conn info_con;
    fd = Socket(AF_INET, SOCK_STREAM, 0);
    n = connect(fd, sa, salen);
    info_con.cfd = fd;
    info_con.n = n;
    return info_con;
}

/* Master中使用 */
int Socket_connect(bool first_call, const struct sockaddr* sa, socklen_t salen){
    int n, fd;
    do{
        if(!first_call){    
            close(fd);
            sleep(1);
        }
        fd = Socket(AF_INET, SOCK_STREAM, 0);
        n = connect(fd, sa, salen);
        first_call = false;
    }while(n<0);
    auto flags = fcntl(fd,F_GETFL,0);
    fcntl(fd,F_SETFL,flags|O_NONBLOCK);
    return fd;
}
ssize_t Read(int cfd, char *buf, int size){
    ssize_t n;
    do{
        n = read(cfd, buf, size);
        if(n>=0)
            return n;
    }while(errno==EINTR||errno==EAGAIN);
    return -1;
}

ssize_t unblock_read_net(int cfd, char *buf, int size){
    std::string res="";
    bool flag=false;
    ssize_t rtn = 0;
    int n=0;
    do{
        n = read(cfd, buf, size);
        if(n>0&&!flag){
            for(int i=0; i<n; i++){
                if(buf[i]=='\0'){
                    flag=true;
                    break;
                }
                if(!flag){
                    res += buf[i];
                    rtn++;
                }
            }
        }
    }while(n>0 || errno==EINTR);
    if(rtn==0)
        return n;
    int i=0;
    for(char c:res){
        buf[i++] = c;
    }
    buf[i] = '\0';
    return rtn;
}

#endif
