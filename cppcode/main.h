#include <cstdint>
#include <string>
#include <iostream>
#include <vector>

#define BOARD uint64_t
#define U64 uint64_t
#define U8 uint8_t
#define S16 int16_t

#define INF 32767 //16 bit int limit

#define TTSIZE 1000000 //1 million

extern int nodeCount;

enum{
    RED, YELLOW
};

struct Position{
    BOARD rboard;
    BOARD yboard;
    int eval;
    uint64_t hash;
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
    int opponentCanWinNextMove();
    int canWinNextMove();
    std::vector<Position> children(uint8_t firstMove = 255);
};

enum{
    EXACT, LOWERBOUND, UPPERBOUND
};

struct TTEntry {
    U64 keyFrag;
    S16 score;       //score from minimax //32 bits
    U8 depth;        //depth of stored search //6 bits
    U8 flag;         //EXACT, LOWERBOUND, UPPERBOUND //2 bits
    U8 bestMove;     //best move for ordering //3 bits
    //void print();
};