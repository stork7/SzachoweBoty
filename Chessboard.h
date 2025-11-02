#ifndef CHESSBOARD_H
#define CHESSBOARD_H

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <random>
#include <cstdint>

class ChessBoard {
    char board[8][8];
    bool whiteToMove;
    std::pair<int,int> enPassantTarget = {-1, -1};
    // Roszada – czy król / wieże ruszały się
    bool whiteKingMoved, blackKingMoved;
    bool whiteKingsideRookMoved, whiteQueensideRookMoved;
    bool blackKingsideRookMoved, blackQueensideRookMoved;
    std::unordered_map<std::string,int> positionHistory;
    bool simulationMode = false;
    static uint64_t zobristTable[64][12];
    static uint64_t zobristWhiteToMove;
    static uint64_t zobristCastling[4];
    static uint64_t zobristEnPassant[8];
    uint64_t zobristKey = 0;


public:
    ChessBoard();
    bool loadFEN(const std::string& fen);
    void display() const;
    bool makeMove(const std::string& move);
    bool isMoveValid(int fromRow, int fromCol, int toRow, int toCol) const;
    void recordPosition();
    bool isThreefoldRepetition();
    bool sideWhiteToMove() const;
    std::vector<std::string> generateLegalMoves(bool forWhite);
    std::vector<std::string> generateMovesForPiece(int row, int col) const;
    char getPiece(int row, int col) const;
    int evaluate(int mode, bool whitePerspective) const;
    struct MoveEval {
        std::string move;
        int eval;
    };
    MoveEval findBestMove(int depth, int heuristicMode);
    int minimax(int depth, int alpha, int beta, bool maximizingPlayer, int heuristicMode, int plyFromRoot);
private:
    bool isPathClear(int fromRow, int fromCol, int toRow, int toCol) const;
    bool isSquareAttacked(int row, int col, bool byWhite) const;
    bool canCastleKingside(bool isWhite) const;
    bool canCastleQueenside(bool isWhite) const;
    bool isInCheck(bool white) const;
    bool hasAnyLegalMove(bool white);
    std::string getPositionKey() const;
    bool tryCastle(bool isWhite, bool kingside);
    bool wouldLeaveKingInCheck(int fromRow, int fromCol, int toRow, int toCol, char promotionChar);
    void doMove(int fromRow, int fromCol, int toRow, int toCol, char promotionChar);
    void postMoveUpdates(char movedPiece, int fromRow, int fromCol, int toRow, int toCol);
    bool parseMove(const std::string& move, int& fromRow, int& fromCol, int& toRow, int& toCol, char& promotionChar);
    static int evaluateMaterialist(const ChessBoard& board, bool whitePerspective);
    static int evaluateStrateg(const ChessBoard& board, bool whitePerspective);
    static int evaluateAggressor(const ChessBoard& board, bool whitePerspective);
    std::vector<std::pair<std::string, int>> scoreMoves(
    const std::vector<std::string>& moves, bool forWhite) const;
    int pieceValue(char piece) const;
    uint64_t computeZobristKey() const;
    std::vector<std::string> generatePseudoLegalMoves(bool forWhite) const;


struct TTEntry {
    int depth;
    int eval;
};

    std::unordered_map<uint64_t, TTEntry> transpositionTable;




};

#endif