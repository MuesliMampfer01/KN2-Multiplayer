# KN2-Multiplayer
Eine kleine Multiplayer-Anwendung für Kommunikationsnetze 2

Ausführbare Datei erstellen:
gcc pong.c -o pong -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

Ausfürbare Datei ausführen:
./pong


Ausführen vom server:
./server 20 150 50        # 20% Loss, 150ms Delay, 50ms Jitter

Zusätzlich kann man während das Spiel läuft im Server-Terminal Befehle eintippen und Enter drücken:

loss 40      → setzt Paketverlust auf 40%
delay 300    → setzt Verzögerung auf 300ms
jitter 100   → setzt Jitter auf 100ms
loss 0       → schaltet Verlust wieder aus
