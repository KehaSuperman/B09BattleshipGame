#include "server.h"

int main(int argc, char *argv[])
{
    if (argc<2) //check if there are enough arguments
    {
        fprintf(stderr, "NOT ENOUGH ARGUMENT\n");
        return 0;
    }

    int listen_fd, epfd, port, event_num;
    struct sockaddr_in serv_addr;    
    struct epoll_event listen_ev, evs_list[EVS_LEN];

    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    sscanf(argv[1], "%d", &port);
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(port);
    serv_addr.sin_addr.s_addr=INADDR_ANY;
    
    listen_fd=socket(AF_INET, SOCK_STREAM, 0);
    set_nonblock(listen_fd);
    bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));  //bind my server
    listen(listen_fd, BACKLOG); //change it to listen mode

    epfd=epoll_create1(0);
    listen_ev.data.fd=listen_fd;
    listen_ev.events=EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &listen_ev);

    player *player_list=NULL;
    while(1)
    {
        event_num=epoll_wait(epfd, evs_list, EVS_LEN, 0);
        for (int i=0; i<event_num; i++)
        {
            if (evs_list[i].data.fd==listen_fd) //listen fd event, new client coming
            {
                int client_fd;
                struct epoll_event client_ev;                
                while(1)
                {
                    client_fd=accept(listen_fd, NULL, NULL);
                    if (-1==client_fd)         //check if we have accepted all client
                    {
                        if (errno==EAGAIN||errno==EWOULDBLOCK) break;
                        else perror("Accept failed");
                    }

                    set_nonblock(client_fd);
                    client_ev.data.fd=client_fd;
                    client_ev.events=EPOLLIN|EPOLLHUP;
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_ev)==-1)
                    {
                        perror("epoll_ctl failed");
                        return 0;
                    }
                }
            }
            else
            {
                char message[MESSAGE_LEN];
                ssize_t count;
                request new_request;
                memset(&new_request, 0, sizeof(new_request));
                memset(message, 0, sizeof(message));
                count=read(evs_list[i].data.fd, message, MESSAGE_LEN);
                if (count==-1 || count==0) new_request.type=DISCONNECT; //read failed, disconnect the client
                else handle_message(&new_request, message, player_list, evs_list[i].data.fd);

                player_list=handle_request(&new_request, player_list, evs_list[i].data.fd, epfd);

            }
        }
    }
}