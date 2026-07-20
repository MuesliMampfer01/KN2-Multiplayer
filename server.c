#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "protocol.h"

int clients[MAX_CLIENTS];
int client_count = 0;

int sim_loss   = 0; //Paketverlust in % (0-100)
int sim_delay  = 0; //Grundverzoegerung in ms
int sim_jitter = 0; //Zufaellige Extra-Verzoegerung in ms (0 bis jitter)

//Aktuelle Zeit in Millisekunden
long now_ms(void)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec * 1000L + t.tv_usec / 1000;
}

//Wuerfelt, ob eine Nachricht "verloren" geht
int sim_drop(void)
{
    return sim_loss > 0 && (rand() % 100) < sim_loss;
}

//Warteschlange fuer verzoegerte Nachrichten
#define SIM_QUEUE_SIZE 4096
typedef struct
{
    int used;          //Slot belegt?
    int fd;            //Ziel-Client
    long deliver_at;   //Zeitpunkt, zu dem gesendet werden soll
    GameState state;   //Der zwischengespeicherte Spielzustand
} DelayedMsg;

DelayedMsg sim_queue[SIM_QUEUE_SIZE];

//Ersatz fuer send(): verwirft oder verzoegert Nachrichten je nach Simulation
void sim_send(int fd, GameState *s)
{
    //Paketverlust: Nachricht wird einfach nie gesendet
    if (sim_drop()) return;

    long delay = sim_delay;
    if (sim_jitter > 0) delay += rand() % sim_jitter;

    //Keine Verzoegerung aktiv -> direkt senden wie vorher
    if (delay <= 0)
    {
        send(fd, s, sizeof(*s), MSG_NOSIGNAL);
        return;
    }

    //Nachricht in die Delay-Queue legen
    for (int i = 0; i < SIM_QUEUE_SIZE; i++)
    {
        if (!sim_queue[i].used)
        {
            sim_queue[i].used = 1;
            sim_queue[i].fd = fd;
            sim_queue[i].deliver_at = now_ms() + delay;
            sim_queue[i].state = *s;
            return;
        }
    }
    //Queue voll -> Nachricht verwerfen (zaehlt als Extremstau)
}

//Jede Runde aufrufen: sendet alle Nachrichten, deren Zeit gekommen ist
void sim_flush(void)
{
    long t = now_ms();
    for (int i = 0; i < SIM_QUEUE_SIZE; i++)
    {
        if (sim_queue[i].used && sim_queue[i].deliver_at <= t)
        {
            send(sim_queue[i].fd, &sim_queue[i].state, sizeof(GameState), MSG_NOSIGNAL);
            sim_queue[i].used = 0;
        }
    }
}

//Alle wartenden Nachrichten eines Clients loeschen (bevor sein fd geschlossen wird)
void sim_clear_fd(int fd)
{
    for (int i = 0; i < SIM_QUEUE_SIZE; i++)
    {
        if (sim_queue[i].used && sim_queue[i].fd == fd) sim_queue[i].used = 0;
    }
}

//Laufzeit-Kommandos aus dem Terminal lesen ("loss 30" etc.)
void sim_read_command(void)
{
    char line[64];
    if (fgets(line, sizeof(line), stdin) == NULL) return;

    int v;
    if (sscanf(line, "loss %d", &v) == 1)        { sim_loss = v;   printf("[SIM] Paketverlust: %d%%\n", v); }
    else if (sscanf(line, "delay %d", &v) == 1)  { sim_delay = v;  printf("[SIM] Delay: %d ms\n", v); }
    else if (sscanf(line, "jitter %d", &v) == 1) { sim_jitter = v; printf("[SIM] Jitter: %d ms\n", v); }
    else printf("[SIM] Befehle: loss <0-100> | delay <ms> | jitter <ms>\n");
}

//Client rauswerfen und Queue aufruecken
void rm_client(int idx) 
{
    sim_clear_fd(clients[idx]); //Wartende Sim-Nachrichten fuer diesen Client verwerfen
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

int main(int argc, char *argv[]) 
{
    srand(time(NULL));

    //Simulationswerte optional per Kommandozeile setzen
    if (argc > 1) sim_loss   = atoi(argv[1]);
    if (argc > 2) sim_delay  = atoi(argv[2]);
    if (argc > 3) sim_jitter = atoi(argv[3]);

    //Socket
    int srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); //Erlaubt das direkte Nutzen des Ports, falls geschlossen wurde

    struct sockaddr_in addr = {AF_INET, htons(PORT), INADDR_ANY};
    bind(srv_fd, (struct sockaddr*)&addr, sizeof(addr));
    socklen_t cl_len = sizeof(addr);
    listen(srv_fd, MAX_CLIENTS);

    printf("Server started on Port %d. Waiting for Players...\n", PORT);
    printf("[SIM] Loss: %d%% | Delay: %d ms | Jitter: %d ms\n", sim_loss, sim_delay, sim_jitter);
    printf("[SIM] Laufzeit-Befehle im Terminal: loss <0-100> | delay <ms> | jitter <ms>\n");

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
        FD_SET(STDIN_FILENO, &read_fds); //Terminal-Eingaben fuer Sim-Befehle
        int max_fd = srv_fd; //hoechste ID-Nummer der Clients

        for (int i = 0; i < client_count; i++) 
        {
            FD_SET(clients[i], &read_fds); //Jeder Client kriegt ID
            if (clients[i] > max_fd) max_fd = clients[i]; //Aktualisiert max, damit der Server nur bis dahin ausgeben muss (die folgenden sind leer)
        }

        //60FPS Taktung
        struct timeval tv = {0, 16666};
        select(max_fd + 1, &read_fds, NULL, NULL, &tv); //select() blockt, mit tv wird gesichert, dass der Server auch bei keinem Input das Spiel weiter berechnet

        //Sim-Befehl aus dem Terminal?
        if (FD_ISSET(STDIN_FILENO, &read_fds)) sim_read_command();

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

                //SIMULATION: Eingabe geht "verloren" -> Server ignoriert sie einfach
                //(fuehlt sich fuer den Spieler an, als wuerde die Taste haengen)
                if (sim_drop()) continue;

                //Position berechnen (fuer die aktiven Spieler)
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

        //Spiellogik (ausgefuehrt bei 2 oder mehr Spielern)
        if (client_count >= 2) 
        {
            state.status = 1; //Spiel laeuft
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
                state.score2++; //Punkt fuer Spieler 2
                //Ball in die Mitte zuruecksetzen
                state.ball_x = SCREEN_WIDTH / 2.0f; 
                state.ball_y = SCREEN_HEIGHT / 2.0f;
                ball_dx = 5.0f; 
                ball_dy = 5.0f;
            }
            else if (state.ball_x > SCREEN_WIDTH) 
            {
                state.score1++; //Punkt fuer Spieler 1
                //Ball in die Mitte zuruecksetzen
                state.ball_x = SCREEN_WIDTH / 2.0f; 
                state.ball_y = SCREEN_HEIGHT / 2.0f;
                ball_dx = -5.0f; 
                ball_dy = 5.0f;
            }

            //Matchende pruefen (Wer zuerst 5 Punkte hat, gewinnt)
            if (state.score1 >= 5) 
            {
                printf("Player 1 won! Player 2 (right) will be kicked.\n");
                rm_client(1); //Spieler 2 kicken, Zuschauer rueckt auf
                state.score1 = 0; //Punktestaende fuer das neue Match zuruecksetzen
                state.score2 = 0;
            } 
            else if (state.score2 >= 5) 
            {
                printf("Player 2 won! Player 1 (left) will be kicked.\n");
                rm_client(0); //Spieler 1 kicken, P2 wird P1, Zuschauer rueckt auf
                state.score1 = 0; //Punktestaende fuer das neue Match zuruecksetzen
                state.score2 = 0;
            }
        } 
        else
        {
            state.status = 0;
        }

        //Spielzustand an alle Clients senden (läuft durch die Simulation)
        for (int i = 0; i < client_count; i++)
        {
            sim_send(clients[i], &state);
        }

        //Verzoegerte Nachrichten rausschicken, deren Zeit gekommen ist
        sim_flush();
    }
    return 0;
}