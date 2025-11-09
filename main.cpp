#include "main.h"
#include <cstdint>
#include <string>
#include <iostream>
#include <cassert>

using namespace std;

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

int getBitIndex(int row, int col) {
    return col * 7 + row;
}

void setIndexTo1(BOARD &board, int index){
    board |= (1ULL << index);
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
}

int Position::colorToMove(){
    //count the number of 1s in each
    int rcount = __builtin_popcountll(rboard);
    int ycount = __builtin_popcountll(yboard);
    //if they have the same number of pieces, it's red's move
    return (rcount==ycount)? RED : YELLOW;
}

int Position::indexOfNewPieceInCol(int col){
    BOARD combined = rboard | yboard; //combine the 2 boards
    //bit shift the col all the way to the least sig bits, then chop off everything above with bitwise &
    BOARD columnMask = (combined >> (col * 7)) & 0x7F; 
    //invert and count the trailing zeros to determine how tall the stack is in that col
    int rowOfFirstEmptySlot = __builtin_ctzll(~columnMask & 0x7F);
    //if the row is full, error
    if(rowOfFirstEmptySlot >= 6) return -1;
    //return the index
    return getBitIndex(rowOfFirstEmptySlot, col);
}

void Position::playMove(int col){
    int toMove = colorToMove();
    int indexToPlace = indexOfNewPieceInCol(col);
    assert(indexToPlace != -1); //makes sure the row isnt full
    if(toMove == RED)
        setIndexTo1(rboard, indexToPlace);
    else
        setIndexTo1(yboard, indexToPlace);
}

void Position::putStringIntoBoard(string sequence){
    for(char c : sequence){
        int charVal = c - '0'; //subtracting '0' converts the char to its numerical value
        assert(charVal <= 6 && charVal >=0); //make sure this char represents a valid row
        playMove(charVal); 
    }
}

int main(int argc, char* argv[]){
    Position pos;
    pos.putStringIntoBoard("14423");
    pos.printBoard();
}