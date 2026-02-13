#include "lst_timer.h"
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>



sort_timer_lst::sort_timer_lst()
{
    head = NULL;
    tail = NULL;
}
sort_timer_lst::~sort_timer_lst()
{
    util_timer *tmp = head;
    while(tmp)
    {
        head=tmp->next;
        delete tmp;
        tmp=head;
    }
}

void sort_timer_lst::add_timer(util_timer *timer)
{
    if (!timer)
    {
        return;
    }
    if (!head) {
        head = timer;
        tail = timer;
        return;
    }
    if (timer->expire < head->expire) {
        head->prev = timer;
        timer->next = head;
        head = timer;
        return;
    }
    add_timer(timer,head);
}

void sort_timer_lst::add_timer(util_timer *timer,util_timer *lst_head)
{
    util_timer *prev = lst_head;
    util_timer *tmp = prev->next;
    while(tmp)
    {
        if (timer->expire < tmp->expire) 
        {
            tmp->prev->next=timer;
            timer->prev = tmp->prev;
            timer->next = tmp;
            tmp->prev = timer;
            break;
        }
        tmp=tmp->next;
    }
    if (!tmp) 
    {
        prev->next = timer;
        timer->prev = tmp;
        timer->next = NULL;
        tail = timer;
    }
}

void sort_timer_lst::adjust_timer(util_timer *timer)
{
    if (!timer) 
    {
        return;    
    }

    util_timer *tmp = timer->next;
    if (!tmp || (timer->expire < tmp->expire)) 
    {
        return;
    }
    if (timer ==  head) 
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer,head);
    }
    else {
        timer->prev->next=timer->next;
        timer->next->prev=timer->prev;
        add_timer(timer,timer->next);
    }
}
void sort_timer_lst::del_timer(util_timer *timer)
{
    if (!timer) {
        return;
    }

    if ((timer == head) && (timer == tail)) {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }

    if (timer == head) {
        head=head->next;
        head->prev = NULL;
        delete timer;
        return;
    }

    if (timer == tail) {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }

    timer->next->prev = timer->prev;
    timer->prev->next = timer->next;
    delete timer;
}

void sort_timer_lst::tick()
{
    if(!head)
    {
        return;
    }

    time_t cur = time(NULL);
    util_timer *tmp=head;
    while (tmp) {
        if (cur < tmp->expire) {
            break;
        }
        tmp->cb_func(tmp->user_data);
        head=head->next;
        if (head) {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

int Utils::setnoblocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    int new_optiom = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL,new_optiom);
    return old_option;
}

void Utils::addfd(int epollfd,int fd,bool one_shoot,int TRIMode)
{
    epoll_event event;
    event.data.fd =fd;
    if (1 == TRIMode) {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    }else {
        event.events = EPOLLET | EPOLLRDHUP;
    }
    if (one_shoot) {
        event.events = event.events | EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnoblocking(fd);
}

void Utils::sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1],(char *) &msg,1,0);
    errno =save_errno;
}

void Utils::addsig(int sig,void(handler)(int),bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) 
    {
        sa.sa_flags = sa.sa_flags | SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL)!= -1);
}

void Utils::time_handler()
{
    m_time_lst.tick();
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd,const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
void cd_func(client_data *user_data)
{
    epoll_ctl(Utils::u_epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
    assert(user_data);
    close(user_data -> sockfd);
    // todo http连接计数
}