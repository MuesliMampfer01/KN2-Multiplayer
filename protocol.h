#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PORT 6767
#define MAX_CLIENTS 10

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 450
#define PADDLE_WIDTH 15
#define PADDLE_HEIGHT 80
#define BALL_RADIUS 8

//Client sendet an Server
typedef struct {
    int move_dir; 
} ClientIn;

//Server sendet an alle
typedef struct {
    float p1_y;
    float p2_y;
    float ball_x;
    float ball_y;
    int score1;
    int score2;
    int status; //0 = Warten auf Spiele, 1 = Spiel läuft
} Gamestate;

#endif