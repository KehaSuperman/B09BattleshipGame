#include "server.h"

void set_nonblock(int fd)
{
    int flag=fcntl(fd, F_GETFL)|O_NONBLOCK;
    fcntl(fd, F_SETFL, flag);
}

player *add_player(player *head, player *new_player)
{
    if (NULL==new_player) return head;
    new_player->next=head;
    return new_player;
}

player *delete_player(player *head, int target_fd)
{
    player *next_player, *current_player;

    if (NULL==head) return NULL;

    next_player=head->next;
    current_player=head;
    if (current_player->player_fd==target_fd) //if the head is the target
    {
        free(current_player);
        return next_player;
    }

    while(next_player!=NULL)
    {
        if (next_player->player_fd==target_fd)
        {
            current_player->next=next_player->next;
            free(next_player);
            break;
        }
        current_player=next_player;
        next_player=next_player->next;
    }

    return head;
}

int search_player(player *head, char target_name[NAME_LEN])
{
    if (NULL==head) return 0;
    
    while(NULL!=head)
    {
        if (strcmp(head->name, target_name)==0) return 1;
        head=head->next;
    }

    return 0;
}

player *search_player_fd(player *head, int target_fd)
{
    if (NULL==head) return NULL;
    
    while(NULL!=head)
    {
        if (head->player_fd==target_fd) return head;
        head=head->next;
    }

    return NULL;
}

void delete_player_list(player *head)
{
    player *next_player;
    if (NULL==head) return;

    while(head!=NULL)
    {
        next_player=head->next;
        free(head);
        head=next_player;
    }
}

int is_valid_name(char name[NAME_LEN])
{
    for (int i=0; i<(int)(strlen(name)); i++)
    {
        if (isalnum(name[i])==0 && (name[i]!='-')) return 0;
    }   
    return 1;
}

void get_ship_body(int center[2], char direction, int body[5][2])
{
    int x=center[0];
    int y=center[1];
    if (direction=='|')
    {
        for (int i=-2, j=0;i<3&&j<5;i++,j++)
        {
            body[j][0]=x;
            body[j][1]=y+i;
        }
    }
    else
    {
        for (int i=-2, j=0;i<3&&j<5;i++,j++)
        {
            body[j][0]=x+i;
            body[j][1]=y;
        }
    }
}

int is_valid_coordinate(int center[2], char direction)
{
    int ship_body[5][2];
    get_ship_body(center, direction, ship_body);
    for (int i=0; i<5; i++)
    {
        if (ship_body[i][0]<0 || ship_body[i][0]>9\
        || ship_body[i][1]<0 || ship_body[i][1]>9) return 0;
    }
    return 1;
}

int bomb_hit(player *victim, int coordinate[2]) //return 1 if bomb it a ship
{
    for (int i=0; i<5; i++)
    {
        if (victim->ship_body[i][0]==-1 && victim->ship_body[i][1]==-1) continue;
        else if (victim->ship_body[i][0]==coordinate[0] && victim->ship_body[i][1]==coordinate[1])
        {
            victim->ship_hp[i]=0;
            return 1;
        }
    }
    return 0;
}

int ship_dead(player *victim)
{
    for (int i=0; i<5; i++)
    {
        if (victim->ship_hp[i]!=0) return 0;
    }
    return 1;
}

player *disconnet_player(player *head, int target_fd, int epfd) //delete from epoll, player list and close its fd
{    
    head=delete_player(head, target_fd);
    epoll_ctl(epfd, EPOLL_CTL_DEL, target_fd, NULL);
    close(target_fd);
    return head;
}

int send_to_player(char message[MESSAGE_LEN], int target_fd)
{
    int total_sent=0;
    while (total_sent!=(int)strlen(message))
    {
        ssize_t current_sent=send(target_fd, &message[total_sent], strlen(&message[total_sent]), MSG_NOSIGNAL);
        if (current_sent==-1 && errno==EPIPE)
        {
            return 0;
        }
        total_sent+=(int)current_sent;
    }
    return 1;
}

player *report(player *head, char message[MESSAGE_LEN], int target_fd, int epfd)
{
    if (target_fd==-1)
    {
        player *discon_list=NULL;
        player *current_player=head;
        int list_len=0;
        while (current_player!=NULL)
        {
            int discont_fd=send_to_player(message, current_player->player_fd);
            if (discont_fd!=1)  //send failed
            {
                player *player_cpy=(player *)calloc(1, sizeof(player));
                player_cpy->player_fd=current_player->player_fd;
                player_cpy->next=NULL;
                strcpy(player_cpy->name, current_player->name);
                discon_list=add_player(discon_list, player_cpy);
                list_len++;
            }
            current_player=current_player->next;
        }
        if (list_len!=0)
        {
            char message_list[list_len][MESSAGE_LEN];
            int index=0;
            player *current_discon=discon_list;
            while (current_discon!=NULL)
            {
                snprintf(message_list[index], MESSAGE_LEN, "GG %s\n", current_discon->name);
                head=disconnet_player(head, discon_list->player_fd, epfd);
                current_discon=current_discon->next;
            }
            delete_player_list(discon_list);
            for (int i=0; i<list_len; i++)
            {
                head=report(head, message_list[i], -1, epfd);
            }
        }
    }
    else {
        if (send_to_player(message, target_fd)!=1)
        {
            int registered=0;
            char name[NAME_LEN];
            player *discon_player=search_player_fd(head, target_fd);
            memset(name, 0, sizeof(name));

            if (discon_player!=NULL) 
            {
                registered=1;
                strcpy(name, discon_player->name);
            }

            head=disconnet_player(head, target_fd, epfd);
            if (registered==1)
            {
                snprintf(message, MESSAGE_LEN, "GG %s\n", name);
                head=report(head, message, -1, epfd);
            }
        }
    }
    return head;
}

player *report_lose(player* head, int epfd)
{
    player *current_player=head;
    player *lose_list=NULL;
    int list_len=0;
    while (current_player!=NULL)
    {
        if (ship_dead(current_player)==1)
        {
            player *player_cpy=(player *)calloc(1, sizeof(player));
            player_cpy->player_fd=current_player->player_fd;
            player_cpy->next=NULL;
            strcpy(player_cpy->name, current_player->name);
            lose_list=add_player(lose_list, player_cpy);
            list_len+=1;
        }
        current_player=current_player->next;
    }

    if (list_len!=0)
    {
        int index=0;
        player *current_lose=lose_list;
        char message_list[list_len][MESSAGE_LEN];
        while (current_lose!=NULL)
        {
            snprintf(message_list[index], MESSAGE_LEN, "GG %s\n", current_lose->name);
            current_lose=current_lose->next;
            index++;
        }
        for (int i=0; i<list_len; i++)
        {
            head=report(head, message_list[i], -1, epfd);
        }
        current_lose=lose_list;
        while (current_lose!=NULL)
        {
            head=disconnet_player(head, current_lose->player_fd, epfd);
            current_lose=current_lose->next;
        }
        delete_player_list(lose_list);
    }
    return head;
}

int consec_space(char *message, int len)
{
    for (int i=0; i<len-1; i++)
    {
        if (message[i]==' ' && message[i+1]==' ') return 1;
    }
    if (len>0 && message[len-2]==' ') return 1;
    return 0;
}

void handle_message(request *new_request, char message[MESSAGE_LEN], player* head, int player_fd)
{
    char space;
    if (strlen(message)>100) //message too long
    {
        memset(new_request, 0, sizeof(request));
        new_request->type=DISCONNECT;
    }
    else if (sscanf(message, "REG %21s %d %d%c%c\n", new_request->name,  \
        &new_request->coordinate[0], &new_request->coordinate[1], &space, &new_request->direction)==5)
    {
        if ((int)strlen(new_request->name)>20 \
        || is_valid_name(new_request->name)==0 \
        || (new_request->direction!='-' && new_request->direction!='|') \
        || is_valid_coordinate(new_request->coordinate, new_request->direction)==0 \
        || consec_space(message, strlen(message))==1 \
        || space!=' ')
        {
            memset(new_request, 0, sizeof(request));
            new_request->type=INVALID;
            return;
        }
        else if (search_player(head, new_request->name)==1)
        {
            memset(new_request, 0, sizeof(request));
            new_request->type=NAME_TAKEN;
            return;
        }
        new_request->type=REGISTER;
    }
    else if (sscanf(message, "BOMB%c%d %d\n", &space ,&new_request->coordinate[0], &new_request->coordinate[1])==3)
    {
        player *attacker=search_player_fd(head, player_fd);
        if (attacker==NULL || consec_space(message, strlen(message))==1 || space!=' ')
        {
            new_request->type=INVALID;
        }
        else
        {
            strcpy(new_request->name, attacker->name);
            new_request->type=BOMB;
        }
    }
    else
    {
        memset(new_request, 0, sizeof(request));
        new_request->type=INVALID;
    }
}

player *handle_request(request *current_request, player *head, int player_fd, int epfd)
{
    char message[MESSAGE_LEN];
    if (NULL==current_request) fprintf(stderr, "Request missing");
    else if (current_request->type==DISCONNECT)
    {
        player *discon_player=search_player_fd(head, player_fd);
        if (discon_player!=NULL)
        {
            snprintf(message, MESSAGE_LEN-1,"GG %s\n", discon_player->name);
            report(head, message, -1, epfd);
        }
        else
        {
            head=disconnet_player(head, player_fd, epfd);
            return head;
        }

        if (search_player_fd(head, player_fd)!=NULL)    //check if it has been removed by send_to
        {
            head=disconnet_player(head, player_fd, epfd);
        }
        
    }
    else if (current_request->type==REGISTER)
    {
        player *new_player=(player *)calloc(1, sizeof(player));
        strcpy(new_player->name, current_request->name);
        get_ship_body(current_request->coordinate, current_request->direction, new_player->ship_body);
        new_player->next=NULL;
        new_player->player_fd=player_fd;
        for (int i=0; i<5; i++) new_player->ship_hp[i]=1;
        head=add_player(head, new_player);

        snprintf(message, MESSAGE_LEN-1,"JOIN %s\n", current_request->name);
        head=report(head, "WELCOME\n", player_fd, epfd);
        head=report(head, message, -1, epfd);
    }
    else if (current_request->type==INVALID)
    {
        head=report(head, "INVALID\n", player_fd, epfd);
    }
    else if (current_request->type==NAME_TAKEN)
    {
        head=report(head, "TAKEN\n", player_fd, epfd);
    }
    else if (current_request->type==BOMB)
    {
        int miss=1;
        player *current_player=head;
        while (current_player!=NULL)
        {
            if (bomb_hit(current_player, current_request->coordinate)==1)
            {
                snprintf(message, MESSAGE_LEN, "HIT %s %d %d %s\n", current_request->name, \
                current_request->coordinate[0], current_request->coordinate[1], current_player->name);
                head=report(head, message, -1, epfd);
                head=report_lose(head, epfd);
                miss=0;
            }
            current_player=current_player->next;
        }
        if (miss==1)
        {
            snprintf(message, MESSAGE_LEN, "MISS %s %d %d\n", current_request->name, \
                current_request->coordinate[0], current_request->coordinate[1]);
            head=report(head, message, -1, epfd);
        }
    }

    return head;

}
