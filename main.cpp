#include "ChessBoard.h"
#include <iostream>
using namespace std;

int main() {
    ChessBoard game;

    while (true) {
        game.display();
        cout << (game.sideWhiteToMove() ? "Biale" : "Czarne") << " na ruch.\n";

        // Jeśli białe – gracz, jeśli czarne – bot
        if (game.sideWhiteToMove()) {
            string move;
            cout << "Podaj ruch: ";
            cin >> move;
            if (!game.makeMove(move))
                cout << "Niepoprawny ruch!\n";
        } else {
            cout << "Bot mysli...\n";
            auto best = game.findBestMove(5,2); // głębokość 3, heurystyka "Agresor"
            cout << "Bot wybiera: " << best.move << " (ocena: " << best.eval << ")\n";
            game.makeMove(best.move);
        }
    }
}
