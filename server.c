#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "protocol.h"

int clients[MAX_CLIENTS];
int client_count = 0;

//Client rauswerfen und Queue aufrücken
void rm_client(int idx) 
{
    close(clients[idx]);

    //Wenn aktiver Spieler (0 oder 1) verliert und ein Zuschauer da ist
    if (idx < 2 && client_count > 2) 
    {
        //Der erste Zuschauer (Index 2) kriegt den Platz des Verlierers
        clients[idx] = clients[2]; 
        
        //Die restlichen Zuschauer rutschen einen Platz nach vorne
        for (int i = 2; i < client_count - 1; i++) 
        {
            clients[i] = clients[i + 1];
        }
    } 
    //Wenn Zuschauer geht oder ein Spieler geht und keiner zuschaut
    else 
    {
        for (int i = idx; i < client_count - 1; i++) 
        {
            clients[i] = clients[i + 1]; 
        }
    }
    
    client_count--;
    printf("Client disconnected. Current Player/Spectator: %d\n", client_count);
}
int main() 
{
    //Socket
    int srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); //Erlaubt das direkte Nutzen des Ports, falls geschlossen wurde

    struct sockaddr_in addr = {AF_INET, htons(PORT), INADDR_ANY};
    bind(srv_fd, (struct sockaddr*)&addr, sizeof(addr));
    socklen_t cl_len = sizeof(addr);
    listen(srv_fd, MAX_CLIENTS);

    printf("Server started on Port %d. Waiting for Players...\n", PORT);

    //Startzustand
    GameState state = 
    {
        SCREEN_HEIGHT/2.0f - PADDLE_HEIGHT/2.0f,
        SCREEN_HEIGHT/2.0f - PADDLE_HEIGHT/2.0f,
        SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f,
        0, 0, 0
    };

    float ball_dx = 5.0f, ball_dy = -5.0f;
    float paddle_speed = 6.0f;

    while (1) 
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(srv_fd, &read_fds); //Eingang im Auge behalten
        int max_fd = srv_fd; //höchste ID-Nummer der Clients

        for (int i = 0; i < client_count; i++) 
        {
            FD_SET(clients[i], &read_fds); //Jeder Client kriegt ID
            if (clients[i] > max_fd) max_fd = clients[i]; //Aktualisiert max, damit der Server nur bis dahin ausgeben muss (die folgenden sind leer)
        }

        //60FPS Taktung
        struct timeval tv = {0, 16666};
        select(max_fd + 1, &read_fds, NULL, NULL, &tv); //select() blockt, mit tv wird gesichert, dass der Server auch bei keinem Input das Spiel weiter berechnet

        if (FD_ISSET(srv_fd, &read_fds) && client_count < MAX_CLIENTS) //Neuen Client nach Check reinlassen
        {
            clients[client_count++] = accept(srv_fd, NULL, NULL); //Client auf Liste setzen
        }

        //Inputs der Spieler verarbeiten
        for (int i = 0; i < client_count; i++) 
        {
            if (FD_ISSET(clients[i], &read_fds))
            {
                ClientIn input;
                int data = recv(clients[i], &input, sizeof(input), 0);
                
                if (data <= 0)
                {
                    rm_client(i); //Jemand hat das Fenster geschlossen oder wurde gekickt
                    break; 
                }

                //Position berechnen (für die aktiven spieler)
                if (i == 0) //Spieler 1 (Links)
                { 
                    state.paddle1_y += input.move_dir * paddle_speed; 
                    if (state.paddle1_y < 0) state.paddle1_y = 0; 
                    if (state.paddle1_y > SCREEN_HEIGHT - PADDLE_HEIGHT) state.paddle1_y = SCREEN_HEIGHT - PADDLE_HEIGHT; 
                } 
                else if (i == 1) //Spieler 2 (Rechts)
                {
                    state.paddle2_y += input.move_dir * paddle_speed;
                    if (state.paddle2_y < 0) state.paddle2_y = 0;
                    if (state.paddle2_y > SCREEN_HEIGHT - PADDLE_HEIGHT) state.paddle2_y = SCREEN_HEIGHT - PADDLE_HEIGHT;
                }
            }
        }

        //Spiellogik (ausgeführt bei 2 oder mehr Spieler)
        if (client_count >= 2) 
        {
            state.status = 1; //Spiel läuft
            state.ball_x += ball_dx; //Ball ins Bewegen bringen
            state.ball_y += ball_dy;

            //Wandkollision des Balls (oben/unten)
            if (state.ball_y - BALL_RADIUS <= 0 || state.ball_y + BALL_RADIUS >= SCREEN_HEIGHT) ball_dy *= -1.0f;

            //Paddle Kollision links
            if (state.ball_x - BALL_RADIUS <= 30 + PADDLE_WIDTH && state.ball_y >= state.paddle1_y && state.ball_y <= state.paddle1_y + PADDLE_HEIGHT)
            {
                ball_dx *= -1.1f; state.ball_x = 30 + PADDLE_WIDTH + BALL_RADIUS;
            }

            //Paddle Kollision rechts
            if (state.ball_x + BALL_RADIUS >= SCREEN_WIDTH - 30 - PADDLE_WIDTH && state.ball_y >= state.paddle2_y && state.ball_y <= state.paddle2_y + PADDLE_HEIGHT) 
            {
                ball_dx *= -1.1f; state.ball_x = SCREEN_WIDTH - 30 - PADDLE_WIDTH - BALL_RADIUS;
            }

            //Punkt erzielt (Ball fliegt links oder rechts raus)
            if (state.ball_x < 0) 
            {
                state.score2++; //Punkt für Spieler 2
                //Ball in die Mitte zurücksetzen
                state.ball_x = SCREEN_WIDTH / 2.0f; 
                state.ball_y = SCREEN_HEIGHT / 2.0f;
                ball_dx = 5.0f; 
                ball_dy = 5.0f;
            }
            else if (state.ball_x > SCREEN_WIDTH) 
            {
                state.score1++; //Punkt für Spieler 1
                //Ball in die Mitte zurücksetzen
                state.ball_x = SCREEN_WIDTH / 2.0f; 
                state.ball_y = SCREEN_HEIGHT / 2.0f;
                ball_dx = -5.0f; 
                ball_dy = 5.0f;
            }

            //Matchende prüfen (Wer zuerst 5 Punkte hat, gewinnt)
            if (state.score1 >= 5) 
            {
                printf("Player 1 won! Player 2 (right) will be kicked.\n");
                rm_client(1); //Spieler 2 kicken, Zuschauer rückt auf
                state.score1 = 0; //Punktestände für das neue Match zurücksetzen
                state.score2 = 0;
            } 
            else if (state.score2 >= 5) 
            {
                printf("Player 2 won! Player 1 (left) will be kicked.\n");
                rm_client(0); //Spieler 1 kicken, P2 wird P1, Zuschauer rückt auf
                state.score1 = 0; //Punktestände für das neue Match zurücksetzen
                state.score2 = 0;
            }
        } 
        else
        {
            state.status = 0;
        }

        for (int i = 0; i < client_count; i++)
        {
            send(clients[i], &state, sizeof(state), MSG_NOSIGNAL);
        }
    }
    return 0;
}