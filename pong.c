#include "raylib.h"

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 450
#define PADDLE_WIDTH 15
#define PADDLE_HEIGHT 80
#define BALL_RADIUS 8

int main(void) {
    //Fenster initialisierung
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "PONG");
    SetTargetFPS(60);

    //Höhenlage der 
    float paddle1_y = SCREEN_HEIGHT / 2.0f - PADDLE_HEIGHT /2.0f;
    float paddle2_y = SCREEN_HEIGHT / 2.0f - PADDLE_HEIGHT /2.0f;
    float paddle_speed = 6.0f; //6pixel pro frame

    //Ball Position und Geschwindigkeit
    float ball_x = SCREEN_WIDTH / 2.0f;
    float ball_y = SCREEN_HEIGHT / 2.0f;
    float ball_dx = 5.0f;
    float ball_dy = 5.0f;

    //score
    int score1 = 0;
    int score2 = 0;

    //Spiel-Loop
    while (!WindowShouldClose()) 
    {

        //Input
        if (IsKeyDown(KEY_W) && paddle1_y > 0) paddle1_y -= paddle_speed;
        if (IsKeyDown(KEY_S) && paddle1_y < SCREEN_HEIGHT - PADDLE_HEIGHT) paddle1_y += paddle_speed;

        if (IsKeyDown(KEY_UP) && paddle2_y > 0) paddle2_y -= paddle_speed;
        if (IsKeyDown(KEY_DOWN) && paddle2_y < SCREEN_HEIGHT - PADDLE_HEIGHT) paddle2_y += paddle_speed;

        //Update
        ball_x += ball_dx;
        ball_y += ball_dy;
        
        //Kollision
        if (ball_y - BALL_RADIUS <= 0 || ball_y + BALL_RADIUS >= SCREEN_HEIGHT) 
        {   
            ball_dy *= -1.0f;
        }

        // Paddle-Kollision Spieler 1 (L)
        if (ball_x - BALL_RADIUS <= 30 + PADDLE_WIDTH && 
            ball_y >= paddle1_y && ball_y <= paddle1_y + PADDLE_HEIGHT) {
            ball_dx *= -1.1f; // Ball um 10% schneller
            ball_x = 30 + PADDLE_WIDTH + BALL_RADIUS; //Boundaries damit Ball nicht im Schläger bleibt
        }

        // Paddle-Kollision Spieler 2 (R)
        if (ball_x + BALL_RADIUS >= SCREEN_WIDTH - 30 - PADDLE_WIDTH && 
            ball_y >= paddle2_y && ball_y <= paddle2_y + PADDLE_HEIGHT) {
            ball_dx *= -1.1f;
            ball_x = SCREEN_WIDTH - 30 - PADDLE_WIDTH - BALL_RADIUS;
        }

        // Punktestand prüfen
        if (ball_x < 0) {
            score2++;
            ball_x = SCREEN_WIDTH / 2.0f; ball_y = SCREEN_HEIGHT / 2.0f; // Reset in die Mitte
            ball_dx = 5.0f; ball_dy = 5.0f; //Reset Geschwindigkeit
        }
        if (ball_x > SCREEN_WIDTH) {
            score1++;
            ball_x = SCREEN_WIDTH / 2.0f; ball_y = SCREEN_HEIGHT / 2.0f; 
            ball_dx = -5.0f; ball_dy = 5.0f;
        }


        //GUI erstellen
        BeginDrawing();
        ClearBackground(BLACK);

        //Netz in Mitte
        DrawLine(SCREEN_WIDTH / 2, 0, SCREEN_WIDTH / 2, SCREEN_HEIGHT, DARKGRAY);

        //Schläger und Ball 
        DrawRectangle(30, paddle1_y, PADDLE_WIDTH, PADDLE_HEIGHT, RAYWHITE);
        DrawRectangle(SCREEN_WIDTH - 30 - PADDLE_WIDTH, paddle2_y, PADDLE_WIDTH, PADDLE_HEIGHT, RAYWHITE);
        DrawCircle(ball_x, ball_y, BALL_RADIUS, RAYWHITE);

        // Punktestand zeichnen 
        DrawText(TextFormat("%d", score1), SCREEN_WIDTH / 4, 20, 60, LIGHTGRAY);
        DrawText(TextFormat("%d", score2), 3 * SCREEN_WIDTH / 4, 20, 60, LIGHTGRAY);

        EndDrawing();
    }

    // Aufräumen
    CloseWindow();
    return 0;
}