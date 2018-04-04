#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/epoll.h>
#include <list>
#include <map>
#include <string>
#include <iostream>
#include <fcntl.h>

#define MAX_EVENTS  10000
#define BUF_SIZE    1024
#define PORT        3333

using namespace std;

list<int>  cl_list;
map<int, string> room;
map<int, string> login;
map<int, string>::iterator it;

int setnonblocking(int fd) 
{
   int fdflags;

   if ((fdflags = fcntl(fd, F_GETFL, 0)) == -1)
      return -1;
   fdflags |= O_NONBLOCK;
   if (fcntl(fd, F_SETFL, fdflags) == -1)
      return -1;
   return 0;
}

int sendall(int s, const char *buf, int len, int flags)
{
    int total = 0;
    int n;

    while(total < len)
    {
        n = send(s, buf+total, len-total, flags);
        if(n == -1) { break; }
        total += n;
    }

    return (n==-1 ? -1 : total);
}


void close_client(int client)
{

    if (close(client)  == -1 )
    {
        perror("close");
        exit(EXIT_FAILURE);                        
    }

    cl_list.remove(client);
    room.erase(client);
    login.erase(client);

    cout<<"closing client: " << client <<endl;    
}


int main()
{
    struct  sockaddr_in addr;
    struct  epoll_event ev,  events[MAX_EVENTS];

    int     epoll, client, slave;
    char    buf[BUF_SIZE]; 

    string  your_room  = "\nвведите название комнаты:\n";
    string  your_login = "\nвведите логин:\n";

    slave = socket(AF_INET, SOCK_STREAM, 0);
    if(slave < 0)
    {
        perror("slave socket");
        exit(EXIT_FAILURE);
    }
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(slave, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("slave bind");
        exit(EXIT_FAILURE);
    }

    listen(slave, 1);
    
    epoll = epoll_create(1);
    if (epoll == -1)
    {
        perror("epoll create");    
        exit(EXIT_FAILURE);
    }

    ev.events   = EPOLLIN | EPOLLET;
    ev.data.fd  = slave;
    if ( epoll_ctl(epoll, EPOLL_CTL_ADD, slave, &ev) == -1)
    {
        perror("epoll ctl");
        exit(EXIT_FAILURE);
    }

    for (;;)
    {
        int count = epoll_wait(epoll, events, MAX_EVENTS, -1);
        if (count == -1)
        {
            perror("epoll wait");
            exit(EXIT_FAILURE);
        }

        for (int i=0; i < count; i++)
        {
            if (events[i].data.fd == slave) 
            {
                client = accept(slave, NULL, NULL);
                if(client == -1)
                {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                setnonblocking(client);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client;

                if ( epoll_ctl(epoll, EPOLL_CTL_ADD, client, &ev) == -1)
                {
                    perror("epoll ctl");
                    exit(EXIT_FAILURE);
                }
               	cl_list.push_back(client);
                //send(client, ask_room, (int) sizeof(ask_room), 0);
                if (sendall(client, your_room.c_str(), (int) your_room.length()+1, 0) == -1)
                    close_client( client);
            }
            else
            {
                client = events[i].data.fd;
                if (events[i].events & EPOLLIN)
                {   
                	int len = recv(client, buf, BUF_SIZE, 0);
                	if (len == 0)
                	{
                        close_client( client);
                	}
                	else
                	{
                        string mes(buf);
                        mes = mes.substr(0,len);

                        if (room.count(client) == 0)    
                        {
                            room[client] = mes.substr(0, mes.length()-2);

                            if (sendall(client, your_login.c_str(), (int) your_login.length()+1, 0) == -1)
                                close_client( client);
                            cout <<"регистрация в комнате: " << room[client] << endl;
                        } 
                        else 
                        {
                            if (login.count(client) == 0)
                            {
                                login[client] = mes.substr(0, mes.length()-2);
                                cout << "регистрация пользователя: " << login[client] << endl;
                            }  
                            else
                            {
                                cout <<"сообщение от "<<login[client] << " в комнате " << room[client] << ">> " << mes;                                
                                string send = ">> " + login[client] + ": " + mes;
                                for (it = room.begin(); it != room.end(); ++it)
                                {
                                    if (it->second == room[client]  &&  it->first != client )
                                    {
                                      if (sendall(it->first, send.c_str(), send.length()+1, 0) == -1)
                                         close_client( it->first);
                                    }
                                }
                            }                       
                        }
                	}
                }
                else if (events[i].events & (EPOLLHUP | EPOLLERR) )
                {
                    close_client( client);
                    exit(0);
                }
            }

        }
    }

    return 0;
}