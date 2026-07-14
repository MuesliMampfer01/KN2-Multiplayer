#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "raylib.h"
#include "protocol.h"

int main() 
{
    //Verbinden mit Server
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {AF_INET, htons(PORT)};
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
    {
        printf("Could not connect to Server\n");
        return 1;
    }

    // Fenster aufbauen
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "PONG");
    SetTargetFPS(60);

    GameState state = {0};
    ClientIn input = {0};

    while (!WindowShouldClose())
    {
       //Input an Server
        if (state.status == 1) 
        {
            input.move_dir = 0;
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) input.move_dir = -1;
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) input.move_dir = 1;
            
            send(sock, &input, sizeof(input), MSG_NOSIGNAL);
        }

        
        GameState temp_state;
        int bytes;
        
        //Die while-Schleife zieht ALLE Pakete aus dem Buffer, bis er leer ist
        while ((bytes = recv(sock, &temp_state, sizeof(temp_state), MSG_DONTWAIT)) > 0) 
        {
            state = temp_state; // Überschreibt den alten Zustand immer mit dem neuesten
        }

        //Wenn recv 0 zurückgibt, hat der Server die Verbindung getrennt (gekickt!)
        if (bytes == 0) 
        {
            printf("Connection lost (Server closed the connection).\n");
            break; 
        }


        //Zeichnen der GUI
        BeginDrawing();
        ClearBackground(BLACK);

        if (state.status == 0)
        {
            DrawText("Waiting for 2nd Player...", SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2, 30, RAYWHITE);
        }
        else
        {
            //Netzlinie
            DrawLine(SCREEN_WIDTH / 2, 0, SCREEN_WIDTH / 2, SCREEN_HEIGHT, DARKGRAY);

            //Paddel Links
            DrawRectangle(30, state.paddle1_y, PADDLE_WIDTH, PADDLE_HEIGHT, RAYWHITE);

            //Paddel Rechts
            DrawRectangle(SCREEN_WIDTH - 30 - PADDLE_WIDTH, state.paddle2_y, PADDLE_WIDTH, PADDLE_HEIGHT, RAYWHITE);

            //Ball
            DrawRectangle(state.ball_x - BALL_RADIUS, state.ball_y - BALL_RADIUS, BALL_RADIUS * 2, BALL_RADIUS * 2, RAYWHITE);

            //Punktestand Spieler 1
            DrawText(TextFormat("%d", state.score1), SCREEN_WIDTH / 4, 20, 60, LIGHTGRAY);

            //Punktestand Spieler 2
            DrawText(TextFormat("%d", state.score2), 3 * SCREEN_WIDTH / 4, 20, 60, LIGHTGRAY);   
        }
        //Beenden
        EndDrawing();
    }
    
    CloseWindow();
    close(sock);
    return 0;  
}