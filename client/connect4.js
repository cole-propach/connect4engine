const canvas = document.getElementById('connect4');
const ctx = canvas.getContext('2d');
const engineToggle = document.getElementById('engineToggle');

let deltaTime = 0;

async function runExe(arg1, arg2) {
  // Construct the URL (encode arguments to be URL-safe)
  const url = `http://localhost:3000/run?arg1=${encodeURIComponent(arg1)}&arg2=${encodeURIComponent(arg2)}`;

  try {
    const response = await fetch(url);  // Fetch from server
    if (!response.ok) {
      throw new Error(`HTTP error ${response.status}`);
    }
    const result = await response.text(); // Or .json() if server returns JSON
    return result;
  } catch (err) {
    console.error('Error fetching exe result:', err);
    throw err;
  }
}

function drawRect(x1, y1, x2, y2, color){
    ctx.fillStyle = color;
    ctx.fillRect(x1, y1, x2, y2);
}

function drawLine(x1, y1, x2, y2, color, width){
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.strokeStyle = color;
    ctx.lineWidth = width;
    ctx.stroke();
}

function drawCircle(x, y, r, color){
    ctx.beginPath();
    ctx.arc(x, y, r, 0, Math.PI * 2);
    ctx.fillStyle = color;
    ctx.fill();
}

function drawCircularHole(x, y, r){
    ctx.globalCompositeOperation = 'destination-out';
    ctx.beginPath();
    ctx.arc(x, y, r, 0, Math.PI * 2);
    ctx.fill();

    ctx.globalCompositeOperation = 'source-over';
}

canvas.addEventListener("click", function(event) {
    const rect = canvas.getBoundingClientRect();
    const x = event.clientX - rect.left;
    const y = event.clientY - rect.top;
    clickAt(x, y);
});

//create the board as a 2x2 array (6 rows, 7 cols)
//empty = false
//'red' = red piece
//'gold' = yellow piece
const board = Array.from({ length: 6 }, () => Array(7).fill(false));

function detectWin() {
    const rows = 6;
    const cols = 7;

    // Directions to check: [rowOffset, colOffset]
    const directions = [
        [0, 1],  // horizontal
        [1, 0],  // vertical
        [1, 1],  // diagonal down-right
        [1, -1]  // diagonal down-left
    ];

    for (let row = 0; row < rows; row++) {
        for (let col = 0; col < cols; col++) {
            const color = board[row][col];
            if (!color) continue; // skip empty cells

            for (const [dr, dc] of directions) {
                let count = 1;

                for (let i = 1; i < 4; i++) {
                    const r = row + dr * i;
                    const c = col + dc * i;

                    // Check bounds
                    if (r < 0 || r >= rows || c < 0 || c >= cols) break;

                    if (board[r][c] === color) {
                        count++;
                    } else {
                        break;
                    }
                }

                if (count === 4) {
                    return [true, color]; // win detected
                }
            }
        }
    }

    return [false, null]; // no win
}

function numPiecesInColumn(col) {
    let count = 0;
    for (let row = 0; row < board.length; row++) {
        if (board[row][col]) {
            count++;
        }
    }
    return count;
}

function xToColumn(x) {
    const colWidth = canvas.width / 7;
    let col = Math.floor(x / colWidth);

    //clamp to screen
    if (col < 0) col = 0;
    if (col >= 7) col = 7 - 1;

    return col;
}

function colToX(col){
    return col * 100 + 50;
}

function rowToY(row){
    return 600 - (row * 100 + 50);
}

function drawWinLines() {
    const rows = 6;
    const cols = 7;
    const directions = [
        [0, 1],  // horizontal
        [1, 0],  // vertical
        [1, 1],  // diagonal down-right
        [1, -1]  // diagonal down-left
    ];

    for (let row = 0; row < rows; row++) {
        for (let col = 0; col < cols; col++) {
            const color = board[row][col];
            if (!color) continue;

            for (const [dr, dc] of directions) {
                let count = 1;
                let lastRow = row;
                let lastCol = col;

                for (let i = 1; i < 4; i++) {
                    const r = row + dr * i;
                    const c = col + dc * i;

                    if (r < 0 || r >= rows || c < 0 || c >= cols) break;
                    if (board[r][c] === color) {
                        count++;
                        lastRow = r;
                        lastCol = c;
                    } else {
                        break;
                    }
                }

                if (count === 4) {
                    // Use your colToX and rowToY functions
                    const x1 = colToX(col);
                    const y1 = rowToY(row);
                    const x2 = colToX(lastCol);
                    const y2 = rowToY(lastRow);

                    drawLine(x1, y1, x2, y2, "black", 5);
                }
            }
        }
    }
}


function drawWhiteCircles() {
    const rows = 6;
    const cols = 7;
    const cellSize = 100;

    ctx.fillStyle = "white";

    for (let row = 0; row < rows; row++) {
        for (let col = 0; col < cols; col++) {
            const x = col * cellSize + cellSize / 2;
            const y = row * cellSize + cellSize / 2;

            ctx.beginPath();
            ctx.arc(x, y, cellSize / 2 - 5, 0, Math.PI * 2);
            ctx.fill();
        }
    }
}

function drawGridWithHolesAndPieces(){
    const rows = 6;
    const cols = 7;
    const cellSize = 100;

    //draw blue background
    drawRect(0, 0, canvas.width, canvas.height, 'blue');

    //create holes using clipping
    ctx.save();
    ctx.beginPath();
    for(let row=0; row<rows; row++){
        for(let col=0; col<cols; col++){
            const x = col*cellSize + cellSize/2;
            const y = row*cellSize + cellSize/2;
            ctx.moveTo(x + 35, y); //move to edge to avoid line
            ctx.arc(x, y, 35, 0, Math.PI*2);
        }
    }
    ctx.clip(); //now only the holes area is drawable

    drawWhiteCircles();

    // Draw the pieces behind grid holes
    for(const p of pieces){
        p.draw();
    }

    ctx.restore(); //reset clipping
}

class Piece {
    constructor(row, col, color) {
        this.x = colToX(col);
        this.y = -30;
        this.color = color;
        this.row = row;
        this.col = col;
        this.r = 40;
        this.acceleration = 100;
        this.velocity = 0;
        this.finalY = rowToY(row);
    }

    move(dx, dy) {
        this.x += dx;
        this.y += dy;
    }

    act(){
        this.velocity += this.acceleration*deltaTime;
        this.move(0, this.velocity);

        if(this.y >= this.finalY){
            this.acceleration = 0;
            this.y = this.finalY;
        }
    }

    draw() {
        drawCircle(this.x, this.y, this.r, this.color);
    }

}

const pieces = [];
let moves = "";
let gameOver = false;
let winningPlayer = null;

function playMove(col){
    if(gameOver) return;
    let row = numPiecesInColumn(col);
    let color = ((moves.length%2) == 0) ? 'red' : 'gold';
    //make a new piece of the right color offscreen above the correct column
    pieces.push(new Piece(row, col, color));
    moves += col; //add this move to the string of moves
    //update the board
    board[row][col] = color;

    //calculate win
    let win = detectWin();
    if(win[0]){
        gameOver = win[0];
        winningPlayer = win[1];
    }
}

function clickAt(x, y){
    let col = xToColumn(x);
    let row = numPiecesInColumn(col);
    //if the column i clicked is not full
    let turn = ((moves.length % 2) == 0) ? 'red' : 'gold';
    //engine is off or it is reds turn
    if(row < 6 && (!engineToggle.checked || turn === 'red')){
        //play the move
        playMove(col);
    }
}

function resetGame(){
    pieces.length = 0;
    board.forEach(row => row.fill(false));
    moves = "";
    gameOver = false;
    winningPlayer = null;
}

const resetButton = document.getElementById("resetButton");

resetButton.addEventListener("click", function() {
    resetGame();
});

let piecesMoving;
let aiThinking = false;
function simulate(){
    piecesMoving = false;
    //let all the pieces act
    for (const p of pieces) {
        p.act();
        if(p.acceleration != 0)
            piecesMoving = true;
    }

    if(engineToggle.checked && !gameOver){ //engine is on
        let turn = ((moves.length % 2) == 0) ? 'red' : 'gold';
        if(turn === 'gold' && !aiThinking){
            aiThinking = true;
            console.log("moves is:");
            console.log(moves.split('').map(c => c.charCodeAt(0)));
            console.log(moves);
            (async () => {
                let result = await runExe(moves, 10);
                result = result.trim(); //remove newlines and spaces
                playMove(result);
                aiThinking = false;
            })();
        }
    }
}

function render(){
    //clear the screen
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    //draw the grid and pieces
    drawGridWithHolesAndPieces();

    if(!piecesMoving && gameOver){
        drawWinLines();
    }
}

let lastTime = 0;
function gameLoop(timestamp) {
    //calculate deltaTime
    deltaTime = (timestamp - lastTime) / 1000; // convert to seconds
    lastTime = timestamp;

    simulate();
    render();

    //do another loop
    requestAnimationFrame(gameLoop);
}

//start the game loop
gameLoop();