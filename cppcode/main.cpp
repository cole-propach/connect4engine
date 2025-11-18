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
unordered_map<BOARD, TTEntry*>* tt = new unordered_map<BOARD, TTEntry*>();

mutex TTmtx;

pair<TTEntry, bool> readTT(BOARD key){
    lock_guard<mutex> lock(TTmtx);
    auto it = tt->find(key);
    if(it != tt->end()){
        return {*it->second, true};  //return the entry if found
    }
    else{
        //handle the case where the key is not found
        return {TTEntry() , false};
    }
}

void writeTT(BOARD key, TTEntry val){
    lock_guard<mutex> lock(TTmtx);
    TTEntry* newVal = new TTEntry();
    newVal->rboard = val.rboard;
    newVal->yboard = val.yboard;
    newVal->depth = val.depth;
    newVal->score = val.score;
    newVal->flag = val.flag;
    newVal->bestMove = val.bestMove;
    if((*tt).count(key) == 0)
        (*tt)[key] = newVal;
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
void TTEntry::print(){
    Position printPos = Position(rboard, yboard);
    printPos.printBoard();
    std::cout << "TTEntry {\n";
    std::cout << "  rboard   = 0x" << std::hex << rboard << std::dec << "\n";
    std::cout << "  yboard   = 0x" << std::hex << yboard << std::dec << "\n";
    std::cout << "  depth    = " << depth << "\n";
    std::cout << "  score    = " << score << "\n";
    std::cout << "  flag     = " << static_cast<int>(flag) << "\n";
    std::cout << "  bestMove = " << static_cast<int>(bestMove) << "\n";
    std::cout << "}\n";
}

void printTT(const unordered_map<BOARD, TTEntry*>* tt) {
    std::cout << "=== Transposition Table (" << tt->size() << " entries) ===\n";

    for (const auto& pair : *tt) {
        BOARD key = pair.first;
        TTEntry* entry = pair.second;

        std::cout << "Key (hash) = 0x" << std::hex << key << std::dec << "\n";
        if (entry) {
            entry->print();
        } else {
            std::cout << "  (null entry)\n";
        }

        std::cout << "---------------------------------------\n";
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

int Position::canWinNextMove(){
    int myColor = colorToMove();
    BOARD myBoard = (myColor == RED) ? rboard : yboard;

    for (int col = 0; col < 7; col++) {
        int row = rowOfNewPieceInCol(col);
        if (row == -1) continue; // column full

        // Temporarily add your piece
        myBoard = setBitAtIndex(myBoard, 1, getBitIndex(row, col));

        // Check if it creates a win
        if (detectWin(myBoard)) {
            return col; // you can win here
        }

        // Undo the move
        myBoard = setBitAtIndex(myBoard, 0, getBitIndex(row, col));
    }

    return -1; // no immediate winning move
}

int Position::opponentCanWinNextMove() {
    int myColor = colorToMove();
    int opColor = (myColor == RED) ? YELLOW : RED;
    BOARD oppBoard = (opColor == RED) ? rboard : yboard;

    for (int col = 0; col < 7; col++) {
        int row = rowOfNewPieceInCol(col);
        if (row == -1) continue; // column full

        // Temporarily add opponent's piece
        oppBoard = setBitAtIndex(oppBoard, 1, getBitIndex(row, col));

        // Check if it creates a win
        if (detectWin(oppBoard)) {
            return col; // opponent can win here
        }

        // Undo the move
        oppBoard = setBitAtIndex(oppBoard, 0, getBitIndex(row, col));
    }

    return -1; // no immediate winning move
}

#include <cstdint>
#include <limits>

void Position::evaluate(){
    // quick terminal checks
    int myColor = colorToMove();
    if (detectWin(rboard)) {
        eval = INF;
        return;
    }
    else if (detectWin(yboard)) {
        eval = -INF;
        return;
    }
    // else if (canWinNextMove() != -1){
    //     eval = (myColor == RED) ? 100000 : -100000;
    //     return;
    // }
    // else if (opponentCanWinNextMove() != -1){
    //     eval = (myColor == RED) ? -50000 : 50000;
    //     return;
    // }

    // base score
    int score = 0;

    const int score3 = 10000;
    const int score2 = 100;
    const int scoreCenter = 10;

    // helper to score a window (4 bits in a small integer)
    auto score_window = [&](unsigned rwin, unsigned ywin){
        unsigned rcount = __builtin_popcount(rwin);
        unsigned ycount = __builtin_popcount(ywin);

        if (rcount > 0 && ycount == 0) {
            if (rcount == 3) score += score3;
            else if (rcount == 2) score += score2;
        } else if (ycount > 0 && rcount == 0) {
            if (ycount == 3) score -= score3;
            else if (ycount == 2) score -= score2;
        }
    };

    // horizontal windows
    for (int row = 0; row < 6; ++row){
        for (int col = 0; col < 4; ++col){
            uint8_t rwindow = 0;
            uint8_t ywindow = 0;
            for (int i = 0; i < 4; ++i){
                // getBit should return 0 or 1
                rwindow = setBitAtIndex(rwindow, getBit(rboard, row, col+i), i);
                ywindow = setBitAtIndex(ywindow, getBit(yboard, row, col+i), i);
            }
            score_window((unsigned)rwindow, (unsigned)ywindow);
        }
    }

    // vertical windows
    for (int row = 0; row < 3; ++row){
        for (int col = 0; col < 7; ++col){
            uint8_t rwindow = 0;
            uint8_t ywindow = 0;
            for (int i = 0; i < 4; ++i){
                rwindow = setBitAtIndex(rwindow, getBit(rboard, row+i, col), i);
                ywindow = setBitAtIndex(ywindow, getBit(yboard, row+i, col), i);
            }
            score_window((unsigned)rwindow, (unsigned)ywindow);
        }
    }

    // / diagonal (down-right)
    for (int row = 0; row < 3; ++row){
        for (int col = 0; col < 4; ++col){
            uint8_t rwindow = 0;
            uint8_t ywindow = 0;
            for (int i = 0; i < 4; ++i){
                rwindow = setBitAtIndex(rwindow, getBit(rboard, row+i, col+i), i);
                ywindow = setBitAtIndex(ywindow, getBit(yboard, row+i, col+i), i);
            }
            score_window((unsigned)rwindow, (unsigned)ywindow);
        }
    }

    // \ diagonal (down-left)
    for (int row = 0; row < 3; ++row){
        for (int col = 3; col < 7; ++col){
            uint8_t rwindow = 0;
            uint8_t ywindow = 0;
            for (int i = 0; i < 4; ++i){
                rwindow = setBitAtIndex(rwindow, getBit(rboard, row+i, col-i), i);
                ywindow = setBitAtIndex(ywindow, getBit(yboard, row+i, col-i), i);
            }
            score_window((unsigned)rwindow, (unsigned)ywindow);
        }
    }

    // center column bonus
    uint8_t rmid = getColumn(rboard, 3);
    uint8_t ymid = getColumn(yboard, 3);
    score += __builtin_popcount((unsigned)rmid) * scoreCenter;
    score -= __builtin_popcount((unsigned)ymid) * scoreCenter;

    // clamp to INF bounds (avoid overflow)
    if (score > INF) score = INF;
    if (score < -INF) score = -INF;

    eval = static_cast<int>(score);
}


vector<Position*>* Position::children(uint8_t firstMove) {
    vector<Position*>* output = new vector<Position*>();

    //default order of columns: middle first, then alternate outwards
    int colOrder[7] = {3, 4, 2, 5, 1, 6, 0};

    //if firstMove is specified, create a new order
    vector<int> finalOrder;
    if (firstMove <= 6 && firstMove >= 0) {
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
            if (col < 0 || col >= 7) {
                std::cerr << "Invalid child: col=" << col << "\n";
                assert(false);
            }
            newPos->playMove(col);
            output->push_back(newPos);
        }
    }

    return output;
}

const BOARD COL_MASK[7] = {
    0x7FULL << (0*7), // column 0
    0x7FULL << (1*7), // column 1
    0x7FULL << (2*7), // column 2
    0x7FULL << (3*7), // column 3
    0x7FULL << (4*7), // column 4
    0x7FULL << (5*7), // column 5
    0x7FULL << (6*7)  // column 6
};

BOARD mirrorBoard(BOARD board) {
    BOARD mirrored = 0;
    for (int c = 0; c < 7; ++c) {
        int mirrorCol = 6 - c;
        BOARD colBits = board & COL_MASK[c];
        int shift = (mirrorCol - c) * 7;
        if (shift > 0)
            mirrored |= colBits << shift;
        else
            mirrored |= colBits >> -shift;
    }
    return mirrored;
}

Position* mirrorPos(Position* pos) {
    Position* mirrored = new Position();
    mirrored->rboard = mirrorBoard(pos->rboard);
    mirrored->yboard = mirrorBoard(pos->yboard);
    mirrored->initHash();
    return mirrored;
}

// struct TTEntry {
//     uint64_t key;      //zobrist hash to verify match
//     int depth;         //depth of stored search
//     int score;         //score from minimax
//     uint8_t flag;      //EXACT, LOWERBOUND, UPPERBOUND
//     uint8_t bestMove;  //best move for ordering
// };

int mirrorMove(int col) {
    if(col == 255) return 255;
    return 6 - col; //mirror across center column
}

pair<TTEntry, bool> readTTOrMirror(Position* pos, Position* mirPos){
    auto e = readTT(pos->hash);
    auto eMir = readTT(mirPos->hash);

    if(e.second){ //if we found the entry, return it
        return {e.first, true};
    }
    else if(eMir.second){ //if we did not find the entry, but did find its mirror, mirror the entry and return that
        //return a TTEntry that has been mirrored
        TTEntry* eCopy = new TTEntry(eMir.first);
        eCopy->bestMove = mirrorMove(eCopy->bestMove);
        eCopy->rboard = mirrorBoard(pos->rboard);
        eCopy->yboard = mirrorBoard(pos->yboard);
        return {*eCopy, true};
    }
    return {TTEntry(), false};
}

//alpha-beta pruning works by maintaining a search window [alpha, beta)
//alpha is best score possible so far for maximizing player (red) at this level
//beta is best score possible so far for minimizing player (yellow) at this level
//minimax returns the best possible score that can be achieved for a given player from this position
int minimax(Position* pos, int depth, int alpha, int beta){//, bool &printing){
    bool isMaximizingPlayer = pos->colorToMove() == RED;
    //check in TT for this position or its mirror
    Position* mirPos = mirrorPos(pos);
    pair<TTEntry, bool> readE = readTTOrMirror(pos, mirPos);
    TTEntry* e = readE.second ? &readE.first : nullptr; 

    //use this entry only if it is for the same position as me, and if its depth is not lower than mine
    //make sure depth is not lower than mine because if my depth is higher, the search that put this entry into the table did not go deep enough to ensure i will get the same score if i search for myself
    bool canUseThisEntry = e != nullptr && e->rboard == pos->rboard && e->yboard == pos->yboard && e->depth > depth;
    if (canUseThisEntry){
        //if we have already done exactly this, just stop the search down the tree and return the previously calculated score
        if (e->flag == EXACT) {
            return e->score;
        }
        //need to set alpha and not return because the search that put this entry in did not complete the search for this position, it was pruned
        if (e->flag == LOWERBOUND) {
            alpha = max(alpha, e->score);
        }
        //need to set beta, same explanation as alpha
        if (e->flag == UPPERBOUND) {
            beta = min(beta, e->score);
        }
        //prune condition
        if (alpha >= beta) {
            return e->score;
        }
    }
    
    //if we are at a leaf, return the static eval because we cant make any moves from here
    if(depth == 0 || detectWin(pos->rboard) || detectWin(pos->yboard)){
        pos->evaluate();
        //printing = true;
        return pos->eval;
    }

    int bestMove = 42;
    vector<Position*>* children;
    //if the table entry has a best move, check that first
    if(e!=nullptr && e->bestMove != 255){
        children = pos->children(e->bestMove);
        bestMove = e->bestMove;
    }
    else{ //otherwise go check center outwards
        children = pos->children();
    }

    //if the board is full, but there are no wins, return 0 for tie (cant be a win if the code reaches this point due to above return)
    if(children->size() == 0){
        return 0;
    }

    int currentBest;

    if(isMaximizingPlayer){
        currentBest = -INF-1;
        //for every child
        for(Position* child : *children){
            //minimax it
            int childMinimax = minimax(child, depth-1, alpha, beta);
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
        currentBest = INF+1;
        //for every child
        for(Position* child : *children){
            //minimax it
            int childMinimax = minimax(child, depth-1, alpha, beta);
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

    for (Position* child : *children) {
        delete child; //delete each Position*
    }
    delete children; //delete the vector itself

    //make table entry
    TTEntry newE;
    newE.rboard = pos->rboard;
    newE.yboard = pos->yboard;
    newE.depth = depth;
    newE.bestMove = bestMove;

    if(currentBest <= alpha){
        newE.flag = UPPERBOUND;
    }
    else if(currentBest >= beta){
        newE.flag = LOWERBOUND;
    }
    else{
        newE.flag = EXACT;
    }

    newE.score = currentBest;
    writeTT(pos->hash, newE); //write it to table

    //make mirrored table entry
    newE.rboard = mirrorBoard(pos->rboard);
    newE.yboard = mirrorBoard(pos->yboard);
    newE.depth = depth;
    newE.bestMove = mirrorMove(bestMove);

    if(currentBest <= alpha){
        newE.flag = UPPERBOUND;
    } 
    else if(currentBest >= beta){
        newE.flag = LOWERBOUND;
    }
    else{
        newE.flag = EXACT;
    }

    newE.score = currentBest;
    writeTT(mirPos->hash, newE); //write it to table

    delete mirPos;

    return currentBest;
}

int pickBestMoveFromRootTT(Position root) {
    TTEntry e = readTT(root.hash).first; //get TT entry for root
    if (e.rboard != root.rboard || e.yboard != root.yboard) {
        return -1; //TT might be empty
    }
    return e.bestMove; //move with best score
}

void threadWorker(int threadID, Position &root, int maxDepth) {
    //each thread runs minimax from the root at depths up to the desired depth
    for (int depth = 1; depth <= maxDepth; depth++) {
        minimax(&root, depth, -INF, INF);
    }
}

int bestMove(Position pos, int depth) {
    for (int d = depth; d <= depth; d++) {
        minimax(&pos, d, -INF, INF);
    }
    return pickBestMoveFromRootTT(pos);
}

// int bestMove(Position pos, int depth){
//     //create n new threads that will find the best move
//     int n = 8;
//     vector<thread> threads;
//     for(int i = 0; i < n; i++){
//         //need ref() for passing by ref to threads
//         threads.emplace_back(threadWorker, i, ref(pos), depth);
//     }

//     //must be reference since threads cant be copied
//     for(thread &t : threads){
//         t.join();
//     }

//     //return the best move found by the threads
//     //check TT for best move of hash of root position
//     return pickBestMoveFromRootTT(pos.hash);
// }

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
    bool printRuntime = false;
    bool printBoard = true;

    //start time
    auto start = std::chrono::high_resolution_clock::now();

    int depth = stoi(argv[2]);

    Position pos = Position(0, 0);

    initZobrist();
    pos.initHash();

    pos.putStringIntoBoard(argv[1]);
    if(printBoard){
        pos.printBoard();
    }
    cout << bestMove(pos, depth) <<'\n';

    //printTT(tt);

    //end time
    auto end = std::chrono::high_resolution_clock::now();

    //duriation of main() in ms
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    if(printRuntime)
        cout << "Runtime: " << duration.count() << " ms\n";
}

//254554334123543263652
//25455433412354326365
//25455433412354326365242
