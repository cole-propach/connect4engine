#include <cstdint>
#include <string>
#include <iostream>
#include <vector>

#define BOARD uint64_t

enum{
    RED, YELLOW
};

struct Position{
    BOARD rboard;
    BOARD yboard;
    double eval;
    BOARD hash;

    Position(BOARD rboard = 0, BOARD yboard = 0){
        this->rboard = rboard;
        this->yboard = yboard;
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
    std::vector<Position> children();
};
