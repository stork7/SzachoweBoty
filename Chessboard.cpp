#include "ChessBoard.h"
#include <iostream>
#include <cctype>
#include <sstream>
#include <algorithm>


constexpr int MATE_SCORE = 100000;
constexpr int DRAW_SCORE = 0;
uint64_t ChessBoard::zobristTable[64][12];
uint64_t ChessBoard::zobristWhiteToMove;
uint64_t ChessBoard::zobristCastling[4];
uint64_t ChessBoard::zobristEnPassant[8];

ChessBoard::ChessBoard() {
    // --- Inicjalizacja tablic Zobrista (tylko raz) ---
    static bool zobristInitialized = false;
    if (!zobristInitialized) {
        std::mt19937_64 rng(2025); // stały seed dla testów
        std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);

        for (int sq = 0; sq < 64; ++sq)
            for (int p = 0; p < 12; ++p)
                zobristTable[sq][p] = dist(rng);

        for (int i = 0; i < 8; ++i)
            zobristEnPassant[i] = dist(rng);

        for (int i = 0; i < 4; ++i)
            zobristCastling[i] = dist(rng);

        zobristWhiteToMove = dist(rng);
        zobristInitialized = true;
    }
    loadFEN("r2q1rk1/p4pp1/6np/1p2p3/3pP1b1/P2P1NP1/P1RBQP1P/2R3K1_w_-_-_0_1");
}

bool ChessBoard::loadFEN(const std::string& fen) {
    std::string cleaned = fen;
    std::replace(cleaned.begin(), cleaned.end(), '_', ' ');

    std::istringstream ss(cleaned);
    std::string placement, active, castling, enPassant;
    int halfmoveClock = 0;
    int fullmoveNumber = 1;

    if (!(ss >> placement)) return false;
    if (!(ss >> active)) active = "w";
    if (!(ss >> castling)) castling = "-";
    if (!(ss >> enPassant)) enPassant = "-";
    if (!(ss >> halfmoveClock)) halfmoveClock = 0;
    if (!(ss >> fullmoveNumber)) fullmoveNumber = 1;
    (void)halfmoveClock;
    (void)fullmoveNumber;

    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            board[r][c] = '.';

    int row = 0, col = 0;
    for (char c : placement) {
        if (c == '/') {
            if (++row >= 8) return false;
            col = 0;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            int empty = c - '0';
            if (empty <= 0 || col + empty > 8) return false;
            col += empty;
            continue;
        }

        if (col >= 8 || row >= 8) return false;
        if (!std::isalpha(static_cast<unsigned char>(c))) return false;
        board[row][col++] = c;
    }
    if (row != 7 || col != 8) {
        // jeśli ostatni rząd nie został wypełniony dokładnie 8 polami
        return false;
    }

    if (active == "w") whiteToMove = true;
    else if (active == "b") whiteToMove = false;
    else return false;

    whiteKingMoved = blackKingMoved = true;
    whiteKingsideRookMoved = whiteQueensideRookMoved = true;
    blackKingsideRookMoved = blackQueensideRookMoved = true;

    if (castling != "-") {
        for (char c : castling) {
            switch (c) {
                case 'K': whiteKingMoved = whiteKingsideRookMoved = false; break;
                case 'Q': whiteKingMoved = whiteQueensideRookMoved = false; break;
                case 'k': blackKingMoved = blackKingsideRookMoved = false; break;
                case 'q': blackKingMoved = blackQueensideRookMoved = false; break;
                default: return false;
            }
        }
    }

    enPassantTarget = {-1, -1};
    if (enPassant != "-") {
        if (enPassant.size() != 2) return false;
        char file = enPassant[0];
        char rank = enPassant[1];
        if (file < 'a' || file > 'h' || rank < '1' || rank > '8') return false;
        enPassantTarget.second = file - 'a';
        enPassantTarget.first = 8 - (rank - '0');
    }

    simulationMode = false;
    positionHistory.clear();
    transpositionTable.clear();

    zobristKey = computeZobristKey();
    recordPosition();
    return true;
}

bool ChessBoard::sideWhiteToMove() const {
    return whiteToMove;
}

void ChessBoard::display() const {
    std::cout << "\n";
    for (int row = 0; row < 8; row++) {
        std::cout << (8 - row) << " ";
        for (int col = 0; col < 8; col++) {
            std::cout << board[row][col] << " ";
        }
        std::cout << "\n";
    }
    std::cout << "  a b c d e f g h\n";
}

bool ChessBoard::makeMove(const std::string& move) {
    int fromRow, fromCol, toRow, toCol;
    char promotionChar;
    bool isSim = simulationMode;
    if (!parseMove(move, fromRow, fromCol, toRow, toCol, promotionChar)) return false;

    char piece = board[fromRow][fromCol];
    if (piece=='.') return false;
    bool isWhite = std::isupper(piece);
    if (isWhite != whiteToMove) return false;

    // roszada
    if ((piece=='K' || piece=='k') && (move=="e1g1"||move=="e1c1"||move=="e8g8"||move=="e8c8")) {
        return tryCastle(isWhite,(move[2]=='g'));
    }

    if (!isMoveValid(fromRow,fromCol,toRow,toCol)) return false;
    if (wouldLeaveKingInCheck(fromRow,fromCol,toRow,toCol,promotionChar)) return false;

    doMove(fromRow,fromCol,toRow,toCol,promotionChar);
    postMoveUpdates(piece,fromRow,fromCol,toRow,toCol);
    return true;
}

bool ChessBoard::parseMove(const std::string& move, int& fromRow, int& fromCol,int& toRow, int& toCol, char& promotionChar) {
    if (move.size() < 4 || move.size() > 5) return false;
    fromCol = move[0] - 'a';
    fromRow = 8 - (move[1] - '0');
    toCol   = move[2] - 'a';
    toRow   = 8 - (move[3] - '0');
    if (fromRow < 0 || fromRow > 7 || fromCol < 0 || fromCol > 7) return false;
    if (toRow   < 0 || toRow   > 7 || toCol   < 0 || toCol   > 7) return false;

    promotionChar = 0;
    if (move.size() == 5) {
        char pc = std::tolower(move[4]);
        if (pc == 'q' || pc == 'r' || pc == 'b' || pc == 'n') promotionChar = pc;
        else return false;
    }
    return true;
}

bool ChessBoard::tryCastle(bool isWhite, bool kingside) {
    // sprawdź, czy roszada możliwa
    if (isWhite) {
        if (kingside && canCastleKingside(true)) {
            if (isInCheck(true) || isSquareAttacked(7,5,false) || isSquareAttacked(7,6,false)) return false;
            board[7][4] = '.'; board[7][6] = 'K';
            board[7][7] = '.'; board[7][5] = 'R';
            whiteKingMoved = whiteKingsideRookMoved = true;
        } else if (!kingside && canCastleQueenside(true)) {
            if (isInCheck(true) || isSquareAttacked(7,3,false) || isSquareAttacked(7,2,false)) return false;
            board[7][4] = '.'; board[7][2] = 'K';
            board[7][0] = '.'; board[7][3] = 'R';
            whiteKingMoved = whiteQueensideRookMoved = true;
        } else return false;
    } else {
        if (kingside && canCastleKingside(false)) {
            if (isInCheck(false) || isSquareAttacked(0,5,true) || isSquareAttacked(0,6,true)) return false;
            board[0][4] = '.'; board[0][6] = 'k';
            board[0][7] = '.'; board[0][5] = 'r';
            blackKingMoved = blackKingsideRookMoved = true;
        } else if (!kingside && canCastleQueenside(false)) {
            if (isInCheck(false) || isSquareAttacked(0,3,true) || isSquareAttacked(0,2,true)) return false;
            board[0][4] = '.'; board[0][2] = 'k';
            board[0][0] = '.'; board[0][3] = 'r';
            blackKingMoved = blackQueensideRookMoved = true;
        } else return false;
    }

    enPassantTarget = {-1,-1};
    whiteToMove = !whiteToMove;

    // Zapisz pozycję tylko gdy to nie jest symulacja
    if (!simulationMode) recordPosition();

    return true;
}

bool ChessBoard::wouldLeaveKingInCheck(int fromRow, int fromCol, int toRow, int toCol, char promotionChar) {
    char piece = board[fromRow][fromCol];
    bool isWhite = std::isupper(piece);

    char backupFrom = board[fromRow][fromCol];
    char backupTo   = board[toRow][toCol];

    // symulacja
    board[toRow][toCol] = piece;
    board[fromRow][fromCol] = '.';

    // promocja w symulacji
    if (std::tolower(piece) == 'p' && (toRow==0 || toRow==7)) {
        char promo = promotionChar ? promotionChar : 'q';
        board[toRow][toCol] = isWhite ? std::toupper(promo) : std::tolower(promo);
    }

    bool result = isInCheck(isWhite);

    // cofnięcie
    board[fromRow][fromCol] = backupFrom;
    board[toRow][toCol] = backupTo;
    return result;
}

void ChessBoard::doMove(int fromRow, int fromCol, int toRow, int toCol, char promotionChar) {
    char piece = board[fromRow][fromCol];
    bool isWhite = std::isupper(piece);

    // en passant bicie
    if (std::tolower(piece) == 'p' && fromCol != toCol && board[toRow][toCol]=='.') {
        int capturedRow = isWhite ? toRow+1 : toRow-1;
        board[capturedRow][toCol] = '.';
    }

    // przesunięcie
    board[toRow][toCol] = piece;
    board[fromRow][fromCol] = '.';

    // promocja
    if (std::tolower(piece) == 'p' && (toRow==0 || toRow==7)) {
        char promo = promotionChar ? promotionChar : 'q';
        board[toRow][toCol] = isWhite ? std::toupper(promo) : std::tolower(promo);
    }
}

void ChessBoard::postMoveUpdates(char piece, int fromRow, int fromCol, int toRow, int toCol) {
    if (piece == 'K') whiteKingMoved = true;
    if (piece == 'k') blackKingMoved = true;
    if (piece == 'R' && fromRow==7 && fromCol==0) whiteQueensideRookMoved = true;
    if (piece == 'R' && fromRow==7 && fromCol==7) whiteKingsideRookMoved = true;
    if (piece == 'r' && fromRow==0 && fromCol==0) blackQueensideRookMoved = true;
    if (piece == 'r' && fromRow==0 && fromCol==7) blackKingsideRookMoved = true;

    // en passant
    enPassantTarget = {-1,-1};
    if (std::tolower(piece)=='p' && abs(toRow-fromRow)==2) {
        enPassantTarget = { (fromRow+toRow)/2, fromCol };
    }

    // zmiana tury
    whiteToMove = !whiteToMove;

    zobristKey = computeZobristKey();

    // Zapisz historię i obsłuż koniec tylko gdy nie jesteśmy w trybie symulacji
    if (!simulationMode) {
        recordPosition();

        if (isThreefoldRepetition()) {
            display();
            std::cout << "Remis przez trzykrotne powtorzenie pozycji.\n";
            exit(0);
        }

        bool nowWhite = whiteToMove;
        if (isInCheck(nowWhite)) {
            if (!hasAnyLegalMove(nowWhite)) {
                std::cout << "Mat! " << (nowWhite ? "Biale" : "Czarne") << " przegrywaja.\n";
                display();
                exit(0);
            } else {
                std::cout << "Szach!\n";
            }
        } else {
            if (!hasAnyLegalMove(nowWhite)) {
                std::cout << "Pat! Remis.\n";
                display();
                exit(0);
            }
        }
    }
}


bool ChessBoard::isMoveValid(int fromRow, int fromCol, int toRow, int toCol) const {
    char piece = board[fromRow][fromCol];
    char target = board[toRow][toCol];
    bool isWhite = isupper(piece);

    if (target != '.' && (isupper(target) == isWhite)) return false;

    int dr = toRow - fromRow;
    int dc = toCol - fromCol;

    switch (tolower(piece)) {
        case 'p': {
            int dir = isWhite ? -1 : 1;

            // Zwykły ruch do przodu
            if (dc == 0 && target == '.') {
                if (dr == dir) return true;
                if ((isWhite && fromRow == 6) || (!isWhite && fromRow == 1)) {
                    if (dr == 2 * dir && board[fromRow + dir][fromCol] == '.' && board[toRow][toCol] == '.')
                        return true;
                }
            }

            // Normalne bicie
            if (abs(dc) == 1 && dr == dir && target != '.' && isupper(target) != isWhite)
                return true;

            // Bicie w przelocie
            if (abs(dc) == 1 && dr == dir && target == '.') {
                if (enPassantTarget.first == toRow && enPassantTarget.second == toCol) {
                    return true;
                }
            }

            return false;
        }
        case 'r': return (dr == 0 || dc == 0) && isPathClear(fromRow, fromCol, toRow, toCol);
        case 'b': return (abs(dr) == abs(dc)) && isPathClear(fromRow, fromCol, toRow, toCol);
        case 'q': return ((dr == 0 || dc == 0) || (abs(dr) == abs(dc))) && isPathClear(fromRow, fromCol, toRow, toCol);
        case 'n': return (abs(dr) == 2 && abs(dc) == 1) || (abs(dr) == 1 && abs(dc) == 2);
        case 'k': return abs(dr) <= 1 && abs(dc) <= 1;
    }
    return false;
}


bool ChessBoard::isPathClear(int fromRow, int fromCol, int toRow, int toCol) const {
    int dr = (toRow > fromRow) ? 1 : (toRow < fromRow ? -1 : 0);
    int dc = (toCol > fromCol) ? 1 : (toCol < fromCol ? -1 : 0);

    int r = fromRow + dr, c = fromCol + dc;
    while (r != toRow || c != toCol) {
        if (board[r][c] != '.') return false;
        r += dr;
        c += dc;
    }
    return true;
}

bool ChessBoard::isSquareAttacked(int row, int col, bool byWhite) const {
    // Skoczek
    int knightMoves[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
    for (auto &m : knightMoves) {
        int r = row + m[0], c = col + m[1];
        if (r>=0 && r<8 && c>=0 && c<8) {
            char p = board[r][c];
            if (p != '.' && (tolower(p) == 'n') && (isupper(p) == byWhite)) return true;
        }
    }

    // Pion
    int dir = byWhite ? -1 : 1;
    for (int dc : {-1,1}) {
        int r = row + dir, c = col + dc;
        if (r>=0 && r<8 && c>=0 && c<8) {
            char p = board[r][c];
            if (p != '.' && (tolower(p) == 'p') && (isupper(p) == byWhite)) return true;
        }
    }

    // Wieża / Hetman
    int rookDirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    for (auto &d : rookDirs) {
        int r=row+d[0], c=col+d[1];
        while (r>=0 && r<8 && c>=0 && c<8) {
            char p = board[r][c];
            if (p != '.') {
                if ((tolower(p)=='r' || tolower(p)=='q') && (isupper(p) == byWhite)) return true;
                break;
            }
            r+=d[0]; c+=d[1];
        }
    }

    // Goniec / Hetman
    int bishopDirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
    for (auto &d : bishopDirs) {
        int r=row+d[0], c=col+d[1];
        while (r>=0 && r<8 && c>=0 && c<8) {
            char p = board[r][c];
            if (p != '.') {
                if ((tolower(p)=='b' || tolower(p)=='q') && (isupper(p) == byWhite)) return true;
                break;
            }
            r+=d[0]; c+=d[1];
        }
    }

    // Król
    for (int dr=-1; dr<=1; dr++) {
        for (int dc=-1; dc<=1; dc++) {
            if (dr==0 && dc==0) continue;
            int r=row+dr, c=col+dc;
            if (r>=0 && r<8 && c>=0 && c<8) {
                char p = board[r][c];
                if (p != '.' && tolower(p)=='k' && (isupper(p)==byWhite)) return true;
            }
        }
    }

    return false;
}

bool ChessBoard::canCastleKingside(bool isWhite) const {
    if (isWhite) {
        if (whiteKingMoved || whiteKingsideRookMoved) return false;
        if (board[7][5] != '.' || board[7][6] != '.') return false;
        if (isSquareAttacked(7,4,false) || isSquareAttacked(7,5,false) || isSquareAttacked(7,6,false)) return false;
        return true;
    } else {
        if (blackKingMoved || blackKingsideRookMoved) return false;
        if (board[0][5] != '.' || board[0][6] != '.') return false;
        if (isSquareAttacked(0,4,true) || isSquareAttacked(0,5,true) || isSquareAttacked(0,6,true)) return false;
        return true;
    }
}

bool ChessBoard::canCastleQueenside(bool isWhite) const {
    if (isWhite) {
        if (whiteKingMoved || whiteQueensideRookMoved) return false;
        if (board[7][1] != '.' || board[7][2] != '.' || board[7][3] != '.') return false;
        if (isSquareAttacked(7,4,false) || isSquareAttacked(7,3,false) || isSquareAttacked(7,2,false)) return false;
        return true;
    } else {
        if (blackKingMoved || blackQueensideRookMoved) return false;
        if (board[0][1] != '.' || board[0][2] != '.' || board[0][3] != '.') return false;
        if (isSquareAttacked(0,4,true) || isSquareAttacked(0,3,true) || isSquareAttacked(0,2,true)) return false;
        return true;
    }
}

bool ChessBoard::isInCheck(bool white) const {
    int kingRow = -1, kingCol = -1;
    char kingChar = white ? 'K' : 'k';

    // znajdź króla
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (board[r][c] == kingChar) {
                kingRow = r;
                kingCol = c;
                break;
            }
        }
    }

    // sprawdź, czy jakakolwiek figura przeciwnika może zbić króla
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            char p = board[r][c];
            if (p == '.') continue;
            if (isupper(p) == white) continue; // tylko przeciwnik

            if (isMoveValid(r, c, kingRow, kingCol))
                return true;
        }
    }
    return false;
}

bool ChessBoard::hasAnyLegalMove(bool white) {
    for (int r1 = 0; r1 < 8; r1++) {
        for (int c1 = 0; c1 < 8; c1++) {
            char p = board[r1][c1];
            if (p == '.' || (isupper(p) != white)) continue;

            for (int r2 = 0; r2 < 8; r2++) {
                for (int c2 = 0; c2 < 8; c2++) {
                    if (!isMoveValid(r1, c1, r2, c2)) continue;

                    // wykonaj tymczasowy ruch
                    char backupFrom = board[r1][c1];
                    char backupTo   = board[r2][c2];
                    board[r2][c2] = backupFrom;
                    board[r1][c1] = '.';

                    bool inCheck = isInCheck(white);

                    // cofamy
                    board[r1][c1] = backupFrom;
                    board[r2][c2] = backupTo;

                    if (!inCheck) return true; // jest legalny ruch
                }
            }
        }
    }
    return false;
}

std::string ChessBoard::getPositionKey() const {
    std::string key;

    // 1. Układ figur
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            key += board[r][c];
        }
    }

    // 2. Strona ruchu
    key += (whiteToMove ? 'w' : 'b');

    // 3. Roszada
    key += (whiteKingMoved ? '1' : '0');
    key += (whiteKingsideRookMoved ? '1' : '0');
    key += (whiteQueensideRookMoved ? '1' : '0');
    key += (blackKingMoved ? '1' : '0');
    key += (blackKingsideRookMoved ? '1' : '0');
    key += (blackQueensideRookMoved ? '1' : '0');

    // 4. En passant
    key += std::to_string(enPassantTarget.first) + "," + std::to_string(enPassantTarget.second);

    return key;
}

void ChessBoard::recordPosition() {
    std::string key = getPositionKey();
    positionHistory[key]++;
}

bool ChessBoard::isThreefoldRepetition() {
    std::string key = getPositionKey();
    return positionHistory[key] >= 3;
}

static std::string coordToNotation(int row, int col) {
    return std::string(1, 'a' + col) + std::to_string(8 - row);
}

std::vector<std::string> ChessBoard::generateLegalMoves(bool forWhite) {
    std::vector<std::string> legalMoves;
    auto pseudoMoves = generatePseudoLegalMoves(forWhite);

    for (const auto& move : pseudoMoves) {
        int fr, fc, tr, tc;
        char promo;
        if (!parseMove(move, fr, fc, tr, tc, promo)) continue;

        char piece = board[fr][fc];
        if (piece == '.' || (std::isupper(piece) != forWhite)) continue;
        if (std::tolower(piece) == 'k') {
            if (forWhite && fr == 7 && fc == 4) {
                if (move == "e1g1") { // krótka roszada
                    if (canCastleKingside(true)) {
                        legalMoves.push_back(move);
                    }
                    continue; // nie sprawdzamy dalej tego ruchu
                } else if (move == "e1c1") { // długa roszada
                    if (canCastleQueenside(true)) {
                        legalMoves.push_back(move);
                    }
                    continue;
                }
            }
            if (!forWhite && fr == 0 && fc == 4) {
                if (move == "e8g8") { // krótka roszada
                    if (canCastleKingside(false)) {
                        legalMoves.push_back(move);
                    }
                    continue;
                } else if (move == "e8c8") { // długa roszada
                    if (canCastleQueenside(false)) {
                        legalMoves.push_back(move);
                    }
                    continue;
                }
            }
        }
        if (!isMoveValid(fr, fc, tr, tc)) continue;

        if (!wouldLeaveKingInCheck(fr, fc, tr, tc, promo))
            legalMoves.push_back(move);
    }

    return legalMoves;
}

std::vector<std::string> ChessBoard::generatePseudoLegalMoves(bool forWhite) const {
    std::vector<std::string> moves;
    const int dir = forWhite ? -1 : 1;

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            char piece = board[r][c];
            if (piece == '.') continue;
            if (std::isupper(piece) != forWhite) continue;

            // pomocnicze: kolumny/wiersze jako litery/liczby
            auto posToStr = [](int r1, int c1, int r2, int c2) {
                std::string s;
                s += char('a' + c1);
                s += char('8' - r1);
                s += char('a' + c2);
                s += char('8' - r2);
                return s;
            };

            switch (std::tolower(piece)) {
                case 'p': {
                    int dir = forWhite ? -1 : 1;   // kierunek ruchu
                    int startRow = forWhite ? 6 : 1;
                    int promoRow = forWhite ? 0 : 7;

                    // --- 1. RUCH DO PRZODU ---
                    int forward = r + dir;
                    if (forward >= 0 && forward < 8 && board[forward][c] == '.') {
                        if (forward == promoRow) {
                            // PROMOCJE – 4 typy
                            std::string base = posToStr(r, c, forward, c);
                            moves.push_back(base + "q");
                            moves.push_back(base + "r");
                            moves.push_back(base + "b");
                            moves.push_back(base + "n");
                        } else {
                            moves.push_back(posToStr(r, c, forward, c));
                        }

                        // --- 2. PODWÓJNY RUCH Z POZYCJI STARTOWEJ ---
                        if (r == startRow) {
                            int doubleForward = r + 2 * dir;
                            if (board[r + dir][c] == '.' && board[doubleForward][c] == '.')
                                moves.push_back(posToStr(r, c, doubleForward, c));
                        }
                    }

                    // --- 3. BICIA (normalne + promocja po biciu) ---
                    for (int dc : {-1, 1}) {
                        int nc = c + dc;
                        if (nc < 0 || nc > 7) continue;

                        if (forward >= 0 && forward < 8) {
                            char target = board[forward][nc];

                            if (target != '.' && std::isupper(target) != forWhite) {
                                if (forward == promoRow) {
                                    // PROMOCJE PO BICIU
                                    std::string base = posToStr(r, c, forward, nc);
                                    moves.push_back(base + "q");
                                    moves.push_back(base + "r");
                                    moves.push_back(base + "b");
                                    moves.push_back(base + "n");
                                } else {
                                    moves.push_back(posToStr(r, c, forward, nc));
                                }
                            }
                        }
                    }

                    // --- 4. BICIE W PRZELOCIE (en passant) ---
                    if (enPassantTarget.first != -1) {
                        int epRow = enPassantTarget.first;
                        int epCol = enPassantTarget.second;

                        // pion może iść tylko na pole dokładnie epRow/epCol
                        if (forward == epRow && std::abs(epCol - c) == 1) {
                            moves.push_back(posToStr(r, c, epRow, epCol));
                        }
                    }

                    break;
                }
                case 'n': {
                    static const int dr[8] = {-2,-1,1,2, 2,1,-1,-2};
                    static const int dc[8] = {1,2,2,1,-1,-2,-2,-1};
                    for (int i = 0; i < 8; ++i) {
                        int nr = r + dr[i], nc = c + dc[i];
                        if (nr < 0 || nr >= 8 || nc < 0 || nc >= 8) continue;
                        char target = board[nr][nc];
                        if (target == '.' || std::isupper(target) != forWhite)
                            moves.push_back(posToStr(r, c, nr, nc));
                    }
                    break;
                }
                case 'b': case 'r': case 'q': {
                    static const std::vector<std::pair<int,int>> dirsB = {
                        {-1,-1}, {-1,1}, {1,-1}, {1,1}
                    };
                    static const std::vector<std::pair<int,int>> dirsR = {
                        {-1,0}, {1,0}, {0,-1}, {0,1}
                    };
                    std::vector<std::pair<int,int>> dirs;
                    if (std::tolower(piece) == 'b') dirs = dirsB;
                    else if (std::tolower(piece) == 'r') dirs = dirsR;
                    else {
                        dirs = dirsB;
                        dirs.insert(dirs.end(), dirsR.begin(), dirsR.end());
                    }

                    for (auto [dr, dc] : dirs) {
                        int nr = r + dr, nc = c + dc;
                        while (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
                            char target = board[nr][nc];
                            if (target == '.') {
                                moves.push_back(posToStr(r, c, nr, nc));
                            } else {
                                if (std::isupper(target) != forWhite)
                                    moves.push_back(posToStr(r, c, nr, nc));
                                break;
                            }
                            nr += dr; nc += dc;
                        }
                    }
                    break;
                }
                case 'k': {
                    for (int dr = -1; dr <= 1; ++dr) {
                        for (int dc = -1; dc <= 1; ++dc) {
                            if (dr == 0 && dc == 0) continue;
                            int nr = r + dr, nc = c + dc;
                            if (nr < 0 || nr >= 8 || nc < 0 || nc >= 8) continue;
                            char target = board[nr][nc];
                            if (target == '.' || std::isupper(target) != forWhite)
                                moves.push_back(posToStr(r, c, nr, nc));
                        }
                    }
                    if (forWhite && piece == 'K' && r == 7 && c == 4) {
                        moves.push_back("e1g1");
                        moves.push_back("e1c1");
                    } else if (!forWhite && piece == 'k' && r == 0 && c == 4) {
                        moves.push_back("e8g8");
                        moves.push_back("e8c8");
                    }
                    break;
                }
            }
        }
    }

    return moves;
}

std::vector<std::string> ChessBoard::generateMovesForPiece(int row, int col) const {
    std::vector<std::string> moves;
    char piece = board[row][col];
    if (piece == '.') return moves;

    auto posToStr = [](int r1, int c1, int r2, int c2) {
        std::string s;
        s += static_cast<char>('a' + c1);
        s += static_cast<char>('8' - r1);
        s += static_cast<char>('a' + c2);
        s += static_cast<char>('8' - r2);
        return s;
    };

    auto addIfLegal = [&](int targetRow, int targetCol) -> bool {
        if (targetRow < 0 || targetRow >= 8 || targetCol < 0 || targetCol >= 8)
            return false;
        if (!isMoveValid(row, col, targetRow, targetCol))
            return false;

        auto* mutableThis = const_cast<ChessBoard*>(this);
        if (!mutableThis->wouldLeaveKingInCheck(row, col, targetRow, targetCol, 0))
            moves.push_back(posToStr(row, col, targetRow, targetCol));

        return board[targetRow][targetCol] == '.';
    };

    switch (std::tolower(piece)) {
        case 'p': {
            int dir = std::isupper(piece) ? -1 : 1;
            int forward = row + dir;
            addIfLegal(forward, col);

            if ((std::isupper(piece) && row == 6) || (!std::isupper(piece) && row == 1)) {
                int doubleForward = row + 2 * dir;
                addIfLegal(doubleForward, col);
            }

            for (int dc : {-1, 1}) {
                int nr = row + dir;
                int nc = col + dc;
                addIfLegal(nr, nc);
            }
            break;
        }
        case 'n': {
            static const int dr[8] = {-2,-1,1,2, 2,1,-1,-2};
            static const int dc[8] = {1,2,2,1,-1,-2,-2,-1};
            for (int i = 0; i < 8; ++i)
                addIfLegal(row + dr[i], col + dc[i]);
            break;
        }
        case 'b': {
            const int dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
            for (auto& d : dirs) {
                int nr = row + d[0], nc = col + d[1];
                while (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
                    if (!addIfLegal(nr, nc))
                        break;
                    nr += d[0];
                    nc += d[1];
                }
            }
            break;
        }
        case 'r': {
            const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
            for (auto& d : dirs) {
                int nr = row + d[0], nc = col + d[1];
                while (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
                    if (!addIfLegal(nr, nc))
                        break;
                    nr += d[0];
                    nc += d[1];
                }
            }
            break;
        }
        case 'q': {
            const int dirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
            for (auto& d : dirs) {
                int nr = row + d[0], nc = col + d[1];
                while (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
                    if (!addIfLegal(nr, nc))
                        break;
                    nr += d[0];
                    nc += d[1];
                }
            }
            break;
        }
        case 'k': {
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    if (dr == 0 && dc == 0) continue;
                    addIfLegal(row + dr, col + dc);
                }
            }
            break;
        }
    }
    return moves;
}


char ChessBoard::getPiece(int row, int col) const {
    if (row < 0 || row >= 8 || col < 0 || col >= 8) return '.'; // bezpieczeństwo
    return board[row][col];
}

int ChessBoard::evaluateMaterialist(const ChessBoard& board, bool whitePerspective) {
    const std::unordered_map<char, int> values = {
        {'p', 100}, {'n', 320}, {'b', 330}, {'r', 500}, {'q', 900}, {'k', 20000}
    };

    int score = 0;
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            char p = board.getPiece(r, c);
            if (p == '.') continue;
            int val = values.at(std::tolower(p));
            score += std::isupper(p) ? val : -val;
        }
    }

    return score;
}
int ChessBoard::evaluateStrateg(const ChessBoard& board, bool whitePerspective) {
    int score = evaluateMaterialist(board, true); // bazowo materiał
    int positional = 0;

    // Kontrola centrum
    const std::vector<std::pair<int,int>> center = {{3,3},{3,4},{4,3},{4,4}};
    for (auto [r, c] : center) {
        char p = board.getPiece(r, c);
        if (p == 'P') positional += 50;
        if (p == 'p') positional -= 50;
    }

    // Rozwinięcie figur
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            char p = board.getPiece(r, c);
            if (p == '.' || std::tolower(p) == 'p' || std::tolower(p) == 'k') continue;
            if (std::isupper(p) && r < 6) positional += 10;
            if (std::islower(p) && r > 1) positional -= 10;
        }
    }

    // Bezpieczeństwo króla – uproszczone
    if (board.getPiece(7,4) == 'K' && board.getPiece(6,4) == '.') positional -= 40;
    if (board.getPiece(0,4) == 'k' && board.getPiece(1,4) == '.') positional += 40;

    return score + positional;
}
int ChessBoard::evaluateAggressor(const ChessBoard& board, bool whitePerspective) {
    int score = evaluateMaterialist(board, true); // ocena bazowa dla białych
    int attackBonus = 0;

    // Znajdź pozycje królów
    int whiteKingRow = -1, whiteKingCol = -1;
    int blackKingRow = -1, blackKingCol = -1;

    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            char p = board.getPiece(r, c);
            if (p == 'K') { whiteKingRow = r; whiteKingCol = c; }
            else if (p == 'k') { blackKingRow = r; blackKingCol = c; }
        }
    }

    // Przejdź po wszystkich figurach i oceniaj agresję
    for (int r = 0; r < 8; ++r) {
        for (int c = 0; c < 8; ++c) {
            char p = board.getPiece(r, c);
            if (p == '.') continue;

            bool isWhite = std::isupper(p);
            auto moves = board.generateMovesForPiece(r, c);// generuje wszystkie możliwe ruchy danej figury

            for (const auto& m : moves) {
                int tr = 8 - (m[3] - '0');
                int tc = m[2] - 'a';


                // Sprawdź, czy atakuje pola wokół króla przeciwnika
                int kRow = isWhite ? blackKingRow : whiteKingRow;
                int kCol = isWhite ? blackKingCol : whiteKingCol;

                if (abs(tr - kRow) <= 1 && abs(tc - kCol) <= 1) {
                    attackBonus += isWhite ? 25 : -25;
                }

                // Aktywność figur — środkowa część planszy
                if (tr >= 2 && tr <= 5 && tc >= 2 && tc <= 5) {
                    if (std::tolower(p) != 'p' && std::tolower(p) != 'k')
                        attackBonus += isWhite ? 5 : -5;
                }
            }

            // Kara za zablokowanego gońca przez własnego piona
            if (std::tolower(p) == 'b') {
                int dir = isWhite ? -1 : 1;
                int fwdRow = r + dir;
                if (fwdRow >= 0 && fwdRow < 8) {
                    if ((isWhite && board.getPiece(fwdRow, c) == 'P') ||
                        (!isWhite && board.getPiece(fwdRow, c) == 'p')) {
                        attackBonus += isWhite ? -10 : 10;
                    }
                }
            }
        }
    }

    // Dodatkowy bonus za królową blisko króla przeciwnika
    if (blackKingRow != -1 && whiteKingRow != -1) {
        for (int r = 0; r < 8; ++r) {
            for (int c = 0; c < 8; ++c) {
                char p = board.getPiece(r, c);
                if (p == 'Q') {
                    int dist = std::abs(r - blackKingRow) + std::abs(c - blackKingCol);
                    attackBonus += std::max(0, 20 - dist * 3);
                }
                if (p == 'q') {
                    int dist = std::abs(r - whiteKingRow) + std::abs(c - whiteKingCol);
                    attackBonus -= std::max(0, 20 - dist * 3);
                }
            }
        }
    }

    return score + attackBonus;
}


int ChessBoard::evaluate(int mode, bool whitePerspective) const {
    switch (mode) {
        case 0: return evaluateMaterialist(*this, true);
        case 1: return evaluateStrateg(*this, true);
        case 2: return evaluateAggressor(*this, true);
        default: return 0;
    }
}

int ChessBoard::minimax(int depth, int alpha, int beta, bool maximizingPlayer, int heuristicMode, int plyFromRoot) {
    // Sprawdzenie transposition table (Zobrist)
    auto it = transpositionTable.find(zobristKey);
    if (it != transpositionTable.end() && it->second.depth >= depth)
        return it->second.eval;

    auto moves = generateLegalMoves(maximizingPlayer);
    if (moves.empty()) {
        if (isInCheck(maximizingPlayer)) {
            int mateScore = MATE_SCORE - plyFromRoot;
            return maximizingPlayer ? -mateScore : mateScore;
        }
        return DRAW_SCORE;
    }

    // Warunek zakończenia
    if (depth == 0)
        return evaluate(heuristicMode, true);

    // --- MOVE ORDERING ---
    auto orderedMoves = scoreMoves(moves, maximizingPlayer);

    int bestEval = maximizingPlayer ? -10000000 : 10000000;

    for (const auto& [move, score] : orderedMoves) {
        auto saved = *this; // kopiujemy stan
        this->simulationMode = true;

        if (!makeMove(move)) {
            this->simulationMode = false;
            continue;
        }

        // aktualizacja zobrista po ruchu
        zobristKey = computeZobristKey();

        int eval = minimax(depth - 1, alpha, beta, !maximizingPlayer, heuristicMode, plyFromRoot + 1);

        *this = saved;
        this->simulationMode = false;

        if (maximizingPlayer) {
            bestEval = std::max(bestEval, eval);
            alpha = std::max(alpha, eval);
        } else {
            bestEval = std::min(bestEval, eval);
            beta = std::min(beta, eval);
        }

        if (beta <= alpha)
            break; // przycięcie alfa-beta
    }

    // Zapis do transposition table
    transpositionTable[zobristKey] = {depth, bestEval};
    return bestEval;
}

// =========================
// Funkcja wybierająca najlepszy ruch
// =========================
ChessBoard::MoveEval ChessBoard::findBestMove(int depth, int heuristicMode) {
    bool forWhite = whiteToMove;
    auto moves = generateLegalMoves(forWhite);

    if (moves.empty())
        return {"", evaluate(heuristicMode, forWhite)};

    // --- MOVE ORDERING ---
    auto orderedMoves = scoreMoves(moves, forWhite);

    int bestEval = forWhite ? -10000000 : 10000000;
    std::string bestMove;

    int alpha = -10000000;
    int beta = 10000000;

    for (const auto& [move, score] : orderedMoves) {
        auto saved = *this;
        this->simulationMode = true;

        if (!makeMove(move)) {
            this->simulationMode = false;
            continue;
        }

        int eval = minimax(depth - 1, alpha, beta, !forWhite, heuristicMode, 1);

        *this = saved;
        this->simulationMode = false;

        if (forWhite && eval > bestEval) {
            bestEval = eval;
            bestMove = move;
            alpha = std::max(alpha, eval);
        } else if (!forWhite && eval < bestEval) {
            bestEval = eval;
            bestMove = move;
            beta = std::min(beta, eval);
        }

        if (beta <= alpha)
            break; // Przycięcie
    }

    return {bestMove, bestEval};
}


int ChessBoard::pieceValue(char piece) const {
    switch (piece) {
        case 'p': return 100;
        case 'n': return 325;
        case 'b': return 325;
        case 'r': return 500;
        case 'q': return 1050;
        case 'k': return 40000;
        default:  return 0;
    }
}
// --- Funkcja pomocnicza: nadaje priorytety ruchom ---
std::vector<std::pair<std::string, int>> ChessBoard::scoreMoves(
    const std::vector<std::string>& moves, bool forWhite) const
{
    std::vector<std::pair<std::string, int>> scored;

    for (const auto& move : moves) {
        int score = 0;

        if (move.size() < 4) continue;
        int fromCol = move[0] - 'a';
        int fromRow = 8 - (move[1] - '0');
        int toCol   = move[2] - 'a';
        int toRow   = 8 - (move[3] - '0');

        char movingPiece = board[fromRow][fromCol];
        char capturedPiece = board[toRow][toCol];

        // --- 1. Bicie (im więcej warty cel, tym większy priorytet) ---
        if (capturedPiece != '.') {
            int capturedVal = pieceValue(std::tolower(capturedPiece));
            int moverVal = pieceValue(std::tolower(movingPiece));
            score += 10 * capturedVal - moverVal;
        }

        // --- 2. Promocja ---
        if (std::tolower(movingPiece) == 'p' && (toRow == 0 || toRow == 7))
            score += 800;

        // --- 3. Ruchy figur centralnych (kontrola centrum) ---
        if ((toRow >= 2 && toRow <= 5) && (toCol >= 2 && toCol <= 5))
            score += 50;

        // --- 4. Losowe drobne różnicowanie, by uniknąć remisów deterministycznych ---
        score += rand() % 5;

        scored.push_back({move, score});
    }

    // --- Sortuj malejąco po priorytecie ---
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    return scored;
}

uint64_t ChessBoard::computeZobristKey() const {
    uint64_t key = 0;

    auto pieceIndex = [](char piece) -> int {
        switch (piece) {
            case 'P': return 0; case 'N': return 1; case 'B': return 2;
            case 'R': return 3; case 'Q': return 4; case 'K': return 5;
            case 'p': return 6; case 'n': return 7; case 'b': return 8;
            case 'r': return 9; case 'q': return 10; case 'k': return 11;
            default: return -1;
        }
    };

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            char piece = board[row][col];
            int idx = pieceIndex(piece);
            if (idx >= 0)
                key ^= zobristTable[row * 8 + col][idx];
        }
    }

    if (whiteToMove) key ^= zobristWhiteToMove;
    if (whiteKingsideRookMoved == false) key ^= zobristCastling[0];
    if (whiteQueensideRookMoved == false) key ^= zobristCastling[1];
    if (blackKingsideRookMoved == false) key ^= zobristCastling[2];
    if (blackQueensideRookMoved == false) key ^= zobristCastling[3];

    if (enPassantTarget.first != -1)
        key ^= zobristEnPassant[enPassantTarget.second];

    return key;
}