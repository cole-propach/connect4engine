#include <cstdint>
#include <string>
#include <iostream>
#include <vector>

#define BOARD uint64_t

#define INF 9999999 //almost 10 million

enum{
    RED, YELLOW
};

struct Position{
    BOARD rboard;
    BOARD yboard;
    int eval;
    BOARD hash;
    int mostRecentMove;

    Position(BOARD rboard = 0, BOARD yboard = 0){
        this->rboard = rboard;
        this->yboard = yboard;
        this->mostRecentMove = -1;
    }
    void printBoard();
    void placePieceAt(int row, int col, int color);
    int colorToMove();
    int rowOfNewPieceInCol(int col);
    void playMove(int col);
    void putStringIntoBoard(std::string sequence);
    void evaluate();
    bool isLegalMove(int col);
    void initHash();
    std::vector<Position*>* children(uint8_t firstMove = 255);
};

enum{
    EXACT, LOWERBOUND, UPPERBOUND
};

struct TTEntry {
    uint64_t key;      //zobrist hash to verify match
    int depth;         //depth of stored search
    int score;         //score from minimax
    uint8_t flag;      //EXACT, LOWERBOUND, UPPERBOUND
    uint8_t bestMove;  //best move for ordering
};