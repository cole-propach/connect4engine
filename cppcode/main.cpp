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

//zobrist uint64_t -> to TTEntry (seen in h file)
unordered_map<BOARD, TTEntry>* tt = new unordered_map<BOARD, TTEntry>();

mutex TTmtx;

TTEntry readTT(BOARD key){
    lock_guard<mutex> lock(TTmtx);
    return (*tt)[key];
}

void writeTT(BOARD key, TTEntry& val){
    lock_guard<mutex> lock(TTmtx);
    if((*tt).count(key) > 0)
        (*tt)[key] = val;
}

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
    mostRecentMove = col;
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

bool Position::isLegalMove(int col){
    int count = __builtin_popcountll( getColumn(yboard, col) | getColumn(rboard, col));
    return count < 6;
}

void Position::evaluate(){
    //check if the position is already won
    //red win = +inf
    //yellow win = -inf
    if(detectWin(rboard)){
        eval = INF;
        return;
    }
    else if(detectWin(yboard)){
        eval = -INF;
        return;
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
}

vector<Position*>* Position::children(int firstMove) {
    vector<Position*>* output = new vector<Position*>();

    //default order of columns: middle first, then alternate outwards
    int colOrder[7] = {3, 4, 2, 5, 1, 6, 0};

    //if firstMove is specified, create a new order
    vector<int> finalOrder;
    if (firstMove != -1) {
        finalOrder.push_back(firstMove); //first move first
        for (int col : colOrder) {
            if (col != firstMove) {
                finalOrder.push_back(col);
            }
        }
    } else {
        finalOrder.assign(colOrder, colOrder + 7); //use default order
    }

    for (int col : finalOrder) {
        int count = __builtin_popcountll(getColumn(yboard, col) | getColumn(rboard, col));
        if (count < 6) {
            Position* newPos = new Position(rboard, yboard);
            newPos->hash = hash;
            newPos->playMove(col);
            output->push_back(newPos);
        }
    }

    return output;
}

//alpha-beta pruning works by maintaining a search window [alpha, beta)
//alpha is best score possible so far for maximizing player (red) at this level
//beta is best score possible so far for minimizing player (yellow) at this level
//minimax returns the best possible score that can be achieved for a given player from this position
int minimax(Position* pos, int depth, bool isMaximizingPlayer, double alpha, double beta){
    double alphaOrig = alpha;
    double betaOrig = beta;
    //calling children() auto updates the hash of each child
    uint64_t hash = pos->hash;

    //check in TT for this position
    TTEntry e = readTT(hash);
    //use this entry only if it is for the same position as me, and if its depth is not lower than mine
    //make sure depth is not lower than mine because if my depth is higher, the search that put this entry into the table did not go deep enough to ensure i will get the same score if i search for myself
    bool canUseThisEntry = e.key == hash && e.depth >= depth;
    if (canUseThisEntry){
        //if we have already done exactly this, just stop the search down the tree and return the previously calculated score
        if (e.flag == EXACT) {
            return e.score;
        }
        //need to set alpha and not return because the search that put this entry in did not complete the search for this position, it was pruned
        if (e.flag == LOWERBOUND) {
            alpha = max(alpha, e.score);
        }
        //need to set beta, same explanation as alpha
        if (e.flag == UPPERBOUND) {
            beta = min(beta, e.score);
        }
        //prune condition
        if (alpha >= beta) {
            return e.score; //cutoff
        }
    }
    
    //if we are at a leaf, return the static eval because we cant make any moves from here
    if(depth == 0 || detectWin(pos->rboard) || detectWin(pos->yboard)){
        pos->evaluate();
        return pos->eval;
    }

    int bestMove = -1;
    vector<Position*>* children;
    //if the table entry has a best move, check that first
    if(e.bestMove != -1){
        children = pos->children(e.bestMove);
    }
    else{ //otherwise go check center outwards
        children = pos->children();
    }

    //if the board is full, but there are no wins, return 0 for tie (cant be a win if the code reaches this point due to above return)
    if(children->size() == 0){
        return 0;
    }

    double currentBest;

    if(isMaximizingPlayer){
        currentBest = -INF;
        //for every child
        for(Position* child : *children){
            //minimax it
            double childMinimax = minimax(child, depth-1, !isMaximizingPlayer, alpha, beta);
            //check the minimax against the current best and keep the best
            if(childMinimax > currentBest){
                currentBest = childMinimax;
                bestMove = child->mostRecentMove;
            }
            
            alpha = max(alpha, childMinimax);
            if(beta <= alpha){ //prune the rest
                break;
            }
        }
    }
    else{ //minimizing player
        currentBest = INF;
        //for every child
        for(Position* child : *children){
            //minimax it
            double childMinimax = minimax(child, depth-1, !isMaximizingPlayer, alpha, beta);
            //check the minimax against the current best and keep the best
            if(childMinimax < currentBest){
                currentBest = childMinimax;
                bestMove = child->mostRecentMove;
            }
            
            beta = min(beta, childMinimax);
            if(beta <= alpha){ //prune the rest
                break;
            }
        }
    }

    TTEntry newE;
    newE.key = hash;
    newE.depth = depth;
    newE.bestMove = bestMove;

    if(currentBest <= alphaOrig){
        newE.flag = UPPERBOUND;
    } 
    else if(currentBest >= betaOrig){
        newE.flag = LOWERBOUND;
    }
    else{
        newE.flag = EXACT;
    }

    newE.score = currentBest;
    writeTT(hash, newE);

    return currentBest;
}

int pickBestMoveFromRootTT(uint64_t rootHash) {

    TTEntry e = readTT(rootHash); //get TT entry for root
    if (e.key != rootHash) {
        return -1; //TT might be empty
    }
    return e.bestMove; //move with best score
}

void threadWorker(int threadID, Position &root, int maxDepth, bool isMaximizingPlayer) {
    //each thread runs minimax from the root at depths up to the desired depth
    for (int depth = 1; depth <= maxDepth; depth++) {
        minimax(&root, depth, isMaximizingPlayer, -INF, INF);
    }
}

int bestMove(Position pos, int depth){
    bool isMaximizingPlayer = pos.colorToMove() == RED;

    //create n new threads that will find the best move
    int n = 8;
    vector<thread> threads;
    for(int i = 0; i < n; i++){
        //need ref() for passing by ref to threads
        threads.emplace_back(threadWorker, i, ref(pos), depth, isMaximizingPlayer);
    }

    //must be reference since threads cant be copied
    for(thread &t : threads){
        t.join();
    }

    //return the best move found by the threads
    //check TT for best move of hash of root position
    return readTT(pos.hash).bestMove;
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
    
    int depth = stoi(argv[2]);
    bool printRuntime = false;
    bool printBoard = false;

    //start time
    auto start = std::chrono::high_resolution_clock::now();

    Position pos;

    initZobrist();
    pos.initHash();

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