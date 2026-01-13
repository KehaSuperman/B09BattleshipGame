#ifndef SERVER_H

#define SERVER_H
#include<stdio.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<netinet/in.h>
#include<fcntl.h>
#include<errno.h>
#include<string.h>
#include<ctype.h>
#include<unistd.h>

#define BACKLOG 24
#define EVS_LEN 24
#define MESSAGE_LEN 120
#define NAME_LEN 25

typedef struct game_player player;
typedef struct client_request request;

struct game_player
{
    int ship_body[5][2];
    int ship_hp[5];
    char name[NAME_LEN];
    int player_fd;
    player *next;
};

struct client_request
{
    enum
    {
        BOMB,
        REGISTER,
        NAME_TAKEN,
        INVALID,
        DISCONNECT
    } type;

    char name[NAME_LEN];
    int coordinate[2];
    char direction;
};

void set_nonblock(int fd);
int is_valid_name(char name[NAME_LEN]);
int is_valid_coordinate(int center[2], char direction);
void get_ship_body(int center[2], char direction, int body[5][2]);
int consec_space(char *message, int len);

player *add_player(player *head, player *new_player);
player *delete_player(player *head, int target_fd);
int search_player(player *head, char target_name[NAME_LEN]);
player *search_player_fd(player *head, int target_fd);
void delete_player_list(player *head);
int bomb_hit(player *victim, int coordinate[2]);
int ship_dead(player *victim);
player *disconnet_player(player *head, int target_fd, int epfd);
int send_to_player(char message[MESSAGE_LEN], int target_fd);
player *report(player *head, char message[MESSAGE_LEN], int target_fd, int epfd);
player *report_lose(player* head, int epfd);
void handle_message(request *new_request, char message[MESSAGE_LEN], player* head, int player_fd);
player *handle_request(request *current_request, player *head, int player_fd, int epfd);

#endif