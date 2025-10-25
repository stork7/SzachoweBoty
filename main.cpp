#include "ChessBoard.h"
#include <iostream>
#include <string>

enum class Mode {
    HUMAN_VS_BOT = 1,
    BOT_VS_BOT   = 2
};

static int askInt(const std::string& prompt, int def, int lo, int hi) {
    std::cout << prompt << " [" << def << "]: ";
    std::string s;
    if (!std::getline(std::cin, s) || s.empty()) return def;
    try {
        int v = std::stoi(s);
        if (v < lo || v > hi) return def;
        return v;
    } catch (...) { return def; }
}

void playHumanVsBot(int depth, int heuristic) {
    ChessBoard game;

    while (true) {
        game.display();
        std::cout << (game.sideWhiteToMove() ? "Biale" : "Czarne") << " na ruch.\n";

        if (game.sideWhiteToMove()) {
            std::string move;
            std::cout << "Podaj ruch (UCI, np. e2e4): ";
            if (!(std::cin >> move)) return;
            if (!game.makeMove(move))
                std::cout << "Niepoprawny ruch!\n";
        } else {
            std::cout << "Bot mysli...\n";
            auto best = game.findBestMove(depth, heuristic);
            if (best.move.empty()) {
                std::cout << "Brak ruchow.\n";
                return;
            }
            std::cout << "Bot wybiera: " << best.move << " (ocena: " << best.eval << ")\n";
            game.makeMove(best.move);
        }
    }
}

void playBotVsBot(int depth, int heuristicWhite, int heuristicBlack) {
    ChessBoard game;
    game.display();
    while (true) {

        bool whiteToMove = game.sideWhiteToMove();
        int heur = whiteToMove ? heuristicWhite : heuristicBlack;

        std::cout << (whiteToMove ? "Bialy" : "Czarny") << " bot mysli...\n";
        auto best = game.findBestMove(depth, heur);
        if (best.move.empty()) {
            std::cout << "Brak ruchow.\n";
            return;
        }
        std::cout << (whiteToMove ? "Bialy" : "Czarny")
                  << " bot wybiera: " << best.move
                  << " (ocena: " << best.eval << ")\n";

        game.makeMove(best.move);
        game.display();
    }
}

int main() {
    const Mode DEFAULT_MODE = Mode::HUMAN_VS_BOT;

    std::cout << "Wybierz tryb:\n"
              << "  1) Gracz vs Bot\n"
              << "  2) Bot vs Bot\n> ";
    int modeIn = 0;
    std::cin >> modeIn;

    // Wyczyść ewentualne resztki z bufora
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    Mode mode = (modeIn == 1 || modeIn == 2)
        ? static_cast<Mode>(modeIn)
        : DEFAULT_MODE;

    // 🔹 Tutaj zwykłe >> zamiast getline — działa w CLion
    int depth;
    std::cout << "Glebokosc przeszukiwania: ";
    if (!(std::cin >> depth)) depth = 5;

    if (mode == Mode::HUMAN_VS_BOT) {
        int heurBot;
        std::cout << "Heurystyka bota (0=Materialista,1=Strateg,2=Agresor): ";
        if (!(std::cin >> heurBot)) heurBot = 1;
        playHumanVsBot(depth, heurBot);
    } else {
        int heurW, heurB;
        std::cout << "Heurystyka bialego bota (0/1/2) [1]: ";
        if (!(std::cin >> heurW)) heurW = 1;
        std::cout << "Heurystyka czarnego bota (0/1/2) [1]: ";
        if (!(std::cin >> heurB)) heurB = 1;
        playBotVsBot(depth, heurW, heurB);
    }

    return 0;
}
