#include "main.h"
#include <cstdint>
#include <string>
#include <iostream>
#include <cassert>
#include <limits>
#include <bitset>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <random>
#include <chrono>
#include <thread>
#include <future>

using namespace std;

bool doTrans;
bool isMultithreaded;

//zobrist uint64_t -> evaluation as double
unordered_map<BOARD, double> posToEval;

//contains a random value for each color in each position to be used for hashing
BOARD zobrist[7][6][2];

//initialize the Zobrist table with random 64-bit numbers
void initZobrist() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;

    for (int col = 0; col < 7; ++col) {
        for (int row = 0; row < 6; ++row) {
            for (int color = 0; color < 2; ++color) {
                zobrist[col][row][color] = dist(gen);
            }
        }
    }
}

int getBitIndex(int row, int col) {
    return col * 7 + row; //includes sentinel bit at the top of each column
}

BOARD setBitAtIndex(BOARD dest, int value, int index){
    dest &= ~(1 << index); //clear the bit at index
    dest |= value << index; //set the bit at index
    return dest;
}

bool getBit(uint64_t bitboard, int row, int col) {
    int index = getBitIndex(row, col);
    return (bitboard >> index) & 1ULL;
}

void setIndexTo1(BOARD &board, int index){
    board |= (1ULL << index);
}

unsigned char getColumn(BOARD board, int col) {
    //move the desired column to the lsb, then 
    BOARD col_bits = (board >> (col * 7)) & ((1ULL << 6) - 1);
    return static_cast<unsigned char>(col_bits);
}

void Position::placePieceAt(int row, int col, int color){
    int index = getBitIndex(row, col);
    if(color == RED)
        setIndexTo1(rboard, index);
    else if(color == YELLOW)
        setIndexTo1(yboard, index);
}

void Position::printBoard() {
    for (int row = 5; row >= 0; --row) {
        for (int col = 0; col < 7; ++col) {
            BOARD mask = 1ULL << getBitIndex(row, col);
            cout << ((rboard & mask) ? 'R' : ((yboard & mask) ? 'Y' : '0')) << ' ';
        }
        cout << '\n';
    }
    cout << '\n';
}

int Position::colorToMove(){
    //count the number of 1s in each
    int rcount = __builtin_popcountll(rboard);
    int ycount = __builtin_popcountll(yboard);
    //if they have the same number of pieces, it's red's move
    return (rcount==ycount)? RED : YELLOW;
}

int Position::rowOfNewPieceInCol(int col){
    BOARD combined = rboard | yboard; //combine the 2 boards
    //bit shift the col all the way to the least sig bits, then chop off everything above with bitwise &
    BOARD columnMask = (combined >> (col * 7)) & 0x7F; 
    //invert and count the trailing zeros to determine how tall the stack is in that col
    int rowOfFirstEmptySlot = __builtin_ctzll(~columnMask & 0x7F);
    //if the row is full, error
    if(rowOfFirstEmptySlot >= 6) return -1;
    //return the index
    return rowOfFirstEmptySlot;
}

void Position::playMove(int col){
    int toMove = colorToMove();
    int row = rowOfNewPieceInCol(col);
    assert(row != -1); //makes sure the row isnt full
    int indexToPlace = getBitIndex(row, col);
    if(toMove == RED)
        setIndexTo1(rboard, indexToPlace);
    else
        setIndexTo1(yboard, indexToPlace);
   
    hash ^= zobrist[col][row][toMove];
}

/*
Input String Format = sequence of moves to reach this position

ex. "14423" results in this board:

        0 0 0 0 0 0 0 
        0 0 0 0 0 0 0
        0 0 0 0 0 0 0
        0 0 0 0 0 0 0
        0 0 0 0 R 0 0
        0 R Y R Y 0 0
*/
void Position::putStringIntoBoard(string sequence){
    for(char c : sequence){
        int charVal = c - '0'; //subtracting '0' converts the char to its numerical value
        assert(charVal <= 6 && charVal >=0); //make sure this char represents a valid row
        playMove(charVal); 
    }
}

// down 1 row: board >> 1
// up 1 row: board << 1
// right 1 col: board << 7
// left 1 col: board >> 7

bool hasHorizontalWin(BOARD board){
    BOARD m = board & (board >> 7); //detect 2 in a row
    if (m & (m >> 14)) return true; //detect 2 of those in a row
    return false;
}

bool hasVerticalWin(BOARD board){
    BOARD m = board & (board >> 1); //detect 2 in a row
    if (m & (m >> 2)) return true; //detect 2 of those in a row
    return false;
}

bool hasDiagonalWin(BOARD board){
    //up right
    BOARD m = board & (board << (7 + 1)); //detect 2 in a row
    if (m & (m >> 16)) return true; //detect 2 of those in a row
    //down right
    m = board & (board << (7 - 1)); //detect 2 in a row
    if (m & (m >> 12)) return true; //detect 2 of those in a row
    return false;
}

bool detectWin(BOARD board){
    return hasHorizontalWin(board) || hasVerticalWin(board) || hasDiagonalWin(board);
}

mutex mtx;

void Position::evaluate(){
    //check if the position is already won
    //red win = +inf
    //yellow win = -inf
    if(detectWin(rboard)){
        eval = numeric_limits<double>::infinity();
        return;
    }
    else if(detectWin(yboard)){
        eval = -numeric_limits<double>::infinity();
        return;
    }

    if(doTrans){
        lock_guard<mutex> lock(mtx);
        //if we've encountered it before, return prev eval
        if(posToEval.count(hash) == 1){
            eval = posToEval[hash];
            return;
        }
    }

    eval = 0;

    const int score3 = 50;
    const int score2 = 10;
    const int scoreCenter = 10;

    //check for 2 or 3 in a 4 long segment without the other player involved
    //check every horizontal window 4 long
    for(int row = 0; row < 6; row++){
        for(int col = 0; col < 4; col++){
            char rwindow = 0; //first 4 bits are the window
            char ywindow = 0;
            for(int i = 0; i < 4; i++){
                rwindow = setBitAtIndex(rwindow, getBit(rboard, row, col+i), i);
                ywindow = setBitAtIndex(ywindow, getBit(yboard, row, col+i), i);
            }
            int rcount = __builtin_popcountll(rwindow);
            int ycount = __builtin_popcountll(ywindow);
            //if that window contains any red pieces and no yellow pieces
            if(rcount > 0 && ycount == 0){
                //if that window contains 3 red pieces, add to score
                eval += (rcount == 3) ? score3 : 0;
                //else if that window contains 2 red pieces, add less to score
                eval += (rcount == 2) ? score2 : 0;
            }
            //else if that window contains any yellow pieces and no red pieces
            else if(ycount > 0 && rcount == 0){
                //if that window contains 3 yellow pieces, subtract from score
                eval += (ycount == 3) ? 0-score3 : 0;
                //else if that window contains 2 yellow pieces, subtract less from score
                eval += (ycount == 2) ? 0-score2 : 0;
            }
        }
    }
    
    //do the exact same for vertical
    for(int row = 0; row < 3; row++){
        for(int col = 0; col < 7; col++){
            char rwindow = 0; //first 4 bits are the window
            char ywindow = 0;
            for(int i = 0; i < 4; i++){
                rwindow = setBitAtIndex(rwindow, getBit(rboard, row+i, col), i);
                ywindow = setBitAtIndex(ywindow, getBit(yboard, row+i, col), i);
            }
            int rcount = __builtin_popcountll(rwindow);
            int ycount = __builtin_popcountll(ywindow);
            //if that window contains any red pieces and no yellow pieces
            if(rcount > 0 && ycount == 0){
                //if that window contains 3 red pieces, add to score
                eval += (rcount == 3) ? score3 : 0;
                //else if that window contains 2 red pieces, add less to score
                eval += (rcount == 2) ? score2 : 0;
            }
            //else if that window contains any yellow pieces and no red pieces
            else if(ycount > 0 && rcount == 0){
                //if that window contains 3 yellow pieces, subtract from score
                eval += (ycount == 3) ? 0-score3 : 0;
                //else if that window contains 2 yellow pieces, subtract less from score
                eval += (ycount == 2) ? 0-score2 : 0;
            }
        }
    }

    //do the exact same for / diagonal
    for(int row = 0; row < 3; row++){
        for(int col = 0; col < 4; col++){
            char rwindow = 0; //first 4 bits are the window
            char ywindow = 0;
            for(int i = 0; i < 4; i++){
                rwindow = setBitAtIndex(rwindow, getBit(rboard, row+i, col+i), i);
                ywindow = setBitAtIndex(ywindow, getBit(yboard, row+i, col+i), i);
            }
            int rcount = __builtin_popcountll(rwindow);
            int ycount = __builtin_popcountll(ywindow);
            //if that window contains any red pieces and no yellow pieces
            if(rcount > 0 && ycount == 0){
                //if that window contains 3 red pieces, add to score
                eval += (rcount == 3) ? score3 : 0;
                //else if that window contains 2 red pieces, add less to score
                eval += (rcount == 2) ? score2 : 0;
            }
            //else if that window contains any yellow pieces and no red pieces
            else if(ycount > 0 && rcount == 0){
                //if that window contains 3 yellow pieces, subtract from score
                eval += (ycount == 3) ? 0-score3 : 0;
                //else if that window contains 2 yellow pieces, subtract less from score
                eval += (ycount == 2) ? 0-score2 : 0;
            }
        }
    }

    //do the exact same for \ diagonal
    for(int row = 0; row < 3; row++){
        for(int col = 3; col < 7; col++){
            char rwindow = 0; //first 4 bits are the window
            char ywindow = 0;
            for(int i = 0; i < 4; i++){
                rwindow = setBitAtIndex(rwindow, getBit(rboard, row+i, col-i), i);
                ywindow = setBitAtIndex(ywindow, getBit(yboard, row+i, col-i), i);
            }
            int rcount = __builtin_popcountll(rwindow);
            int ycount = __builtin_popcountll(ywindow);
            //if that window contains any red pieces and no yellow pieces
            if(rcount > 0 && ycount == 0){
                //if that window contains 3 red pieces, add to score
                eval += (rcount == 3) ? score3 : 0;
                //else if that window contains 2 red pieces, add less to score
                eval += (rcount == 2) ? score2 : 0;
            }
            //else if that window contains any yellow pieces and no red pieces
            else if(ycount > 0 && rcount == 0){
                //if that window contains 3 yellow pieces, subtract from score
                eval += (ycount == 3) ? 0-score3 : 0;
                //else if that window contains 2 yellow pieces, subtract less from score
                eval += (ycount == 2) ? 0-score2 : 0;
            }
        }
    }

    //give bonus points for every piece in the center column
    unsigned char rmid = getColumn(rboard, 3);
    unsigned char ymid = getColumn(yboard, 3);

    int rcount = __builtin_popcountll(rmid);
    int ycount = __builtin_popcountll(ymid);

    eval += rcount * scoreCenter;
    eval -= ycount * scoreCenter;

    if(doTrans){
        lock_guard<mutex> lock(mtx);
        //put this into trasposition table
        posToEval[hash] = eval;
    }
}

vector<Position> Position::children(){
    //every col
    vector<Position> output;
    for(int i = 0; i < 7; i++){
        int count = __builtin_popcountll( getColumn(yboard, i) | getColumn(rboard, i) );
        if(count < 6){
            Position newPos = Position(rboard, yboard);
            newPos.hash = hash;
            newPos.playMove(i);
            output.push_back(newPos);
        }
    }
    return output;
}

//alpha is best score possible so far for maximizing player (red) at this level
//beta is best score possible so far for minimizing player (yellow) at this level
double minimax(Position pos, int depth, bool isMaximizingPlayer, double alpha, double beta){
    if(depth == 0 || detectWin(pos.rboard) || detectWin(pos.yboard)){
        pos.evaluate();
        return pos.eval;
    }

    vector<Position> children = pos.children();
    if(children.size() == 0){
        return 0;
    }

    double currentBest;

    if(isMaximizingPlayer){
        currentBest = -numeric_limits<double>::infinity();
        //for every child
        for(Position child : children){
            //minimax it
            double childMinimax = minimax(child, depth-1, !isMaximizingPlayer, alpha, beta);
            //check the minimax against the current best and keep the highest
            currentBest = max(currentBest, childMinimax);
            alpha = max(alpha, childMinimax);
            if(beta <= alpha){
                break;
            }
        }
    }
    else{ //minimizing player
        currentBest = numeric_limits<double>::infinity();
        //for every child
        for(Position child : children){
            //minimax it
            double childMinimax = minimax(child, depth-1, !isMaximizingPlayer, alpha, beta);
            //check the minimax against the current best and keep the highest
            currentBest = min(currentBest, childMinimax);
            beta = min(beta, childMinimax);
            if(beta <= alpha){
                break;
            }
        }
    }

    return currentBest;
}

bool Position::isLegalMove(int col){
    int count = __builtin_popcountll( getColumn(yboard, col) | getColumn(rboard, col));
    return count < 6;
}

int bestMove(Position pos, int depth){
    bool isMaximizingPlayer = pos.colorToMove() == RED;

    double moveToMinimax[7];

    for(int i = 0; i < 7; i++){
        if(pos.isLegalMove(i)){
            Position child = Position(pos.rboard, pos.yboard);
            child.playMove(i);
            if(isMultithreaded){
                auto fut = async(launch::async, minimax, child, depth-1, !isMaximizingPlayer, -numeric_limits<double>::infinity(), numeric_limits<double>::infinity());
                moveToMinimax[i] = fut.get();
            }
            else{
                moveToMinimax[i] = minimax(child, depth-1, !isMaximizingPlayer, -numeric_limits<double>::infinity(), numeric_limits<double>::infinity());
            }
        }
    }

    double currentBest;
    int bestMove = 0;

    if(isMaximizingPlayer){
        currentBest = -numeric_limits<double>::infinity();
        //for every child
        for(int i = 0; i < 7; i++){
            if(pos.isLegalMove(i)){
                if(moveToMinimax[i] > currentBest){
                    bestMove = i;
                    currentBest = moveToMinimax[i];
                }
            }
        }
    }
    else{ //minimizing player
        currentBest = numeric_limits<double>::infinity();
        //for every child
        for(int i = 0; i < 7; i++){
            if(pos.isLegalMove(i)){
                if(moveToMinimax[i] < currentBest){
                    bestMove = i;
                    currentBest = moveToMinimax[i];
                }
            }
        }
    }
    
    return bestMove;
}

void Position::initHash(){
    hash = 0;
    for(int col=0; col<7; col++){
        for(int row=0; row<6; row++){
            if(getBit(rboard, row, col))
                hash ^= zobrist[col][row][0];
            else if(getBit(yboard, row, col))
                hash ^= zobrist[col][row][1];
        }
    }
}

int main(int argc, char* argv[]){
    //settings
    doTrans = true;
    isMultithreaded = true;
    
    int depth = stoi(argv[2]);
    bool printRuntime = false;
    bool printBoard = false;

    //start time
    auto start = std::chrono::high_resolution_clock::now();

    Position pos;

    if(doTrans){
        initZobrist();
        pos.initHash();
    }

    pos.putStringIntoBoard(argv[1]);
    if(printBoard){
        pos.printBoard();
    }
    cout << bestMove(pos, depth) <<'\n';

    //end time
    auto end = std::chrono::high_resolution_clock::now();

    //duriation of main() in ms
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    if(printRuntime)
        cout << "Runtime: " << duration.count() << " ms\n";
}