/*** includes ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <termio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>
#include <time.h>

/*** Definitions ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define MICHAEL_TRON_VER "0.7"

enum keys {
    BACKSPACE = 127,
    ARROW_UP = 1000,
    ARROW_LEFT,
    ARROW_DOWN,
    ARROW_RIGHT,
    CTRL_LEFT,
    CTRL_RIGHT,
    CTRL_UP,
    CTRL_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
    F1_FUNCTION_KEY,
    F2_FUNCTION_KEY,
    F3_FUNCTION_KEY,
    F4_FUNCTION_KEY,
    F5_FUNCTION_KEY,
    F6_FUNCTION_KEY,
    F7_FUNCTION_KEY,
    F8_FUNCTION_KEY,
    F9_FUNCTION_KEY,
    F10_FUNCTION_KEY,
    ALT_S,
    ALT_F
};

/*** Tron Variables ***/
typedef enum gameState {
    START_SCREEN,
    IN_GAME,
    PLAYER1_WIN,
    PLAYER2_WIN,
    DRAW
} gameState;

typedef struct lightCycle {
    int posX;
    int posY;
    int dirX;
    int dirY;
    bool alive;
} lightCycle;

typedef struct tron {
    char ** gameBoard;
    int boardRows;
    int boardCols;
    
    lightCycle player1;
    lightCycle player2;

    gameState curState;
    
    bool singlePlayer;
    int numTurn;
} tron;

/*** Tron Functions ***/
void gameInit(tron * this);
void gameStart(tron * this);
void makeBorder(tron * this);

bool updateCyclePos(tron * this, int playerNum);
void computerMakeMove(tron* this);
void deathHandler(tron* this);
bool crashChecker(int nextX, int nextY, tron * this);

/*** Input & Output ***/
void processKeypress(tron * tronGame);

typedef struct abuf {
    char* b;
    int len;
} abuf;

#define ABUF_INIT {NULL, 0}
void abAppend(struct abuf* ab, const char* s, int len);
void abFree(struct abuf* ab) {free(ab->b);}

void drawScreen(tron * this);

/*** terminal ***/
void enableRawMode(void);
void disableRawMode(void);
void die(const char* s);
int readKey(void); // add support for new special keys here
int getWindowSize (int* rows, int* cols);
int getCursorPosition(int* rows, int* cols);

/*** Global Varl declaration ***/
struct termios orig_termio;

int main() {
    enableRawMode();

    time_t t;
    srand((unsigned) time(&t));

    tron tronGame;
    gameInit(&tronGame);

    while (1) {
        drawScreen(&tronGame);
        processKeypress(&tronGame);
    }

    return 0;
}

/*** Tron Functions ***/
void gameInit(tron * this) {
    if (getWindowSize(&this->boardRows, &this->boardCols) == -1) {
        die("getWindowSize");
    }

    this->boardCols--; // this is just cuz WSL seem to overreport by one col
    this->boardRows--; // leave space for instructions

    this->gameBoard = (char **) malloc(sizeof(char *) * this->boardRows);
    for (int i = 0; i < this->boardRows; i++) {
        this->gameBoard[i] = (char *) malloc(this->boardCols);
        memset(this->gameBoard[i], ' ', this->boardCols);
    }
    
    this->player1.dirX = 0;
    this->player1.dirY = 0;
    this->player1.posX = this->boardCols * 1 / 3;
    this->player1.posY = this->boardRows / 2;
    this->player1.alive = true;

    this->player2.dirX = 0;
    this->player2.dirY = 0;
    this->player2.posX = this->boardCols * 2 / 3;
    this->player2.posY = this->boardRows / 2;
    this->player2.alive = true;

    this->curState = START_SCREEN;
    this->singlePlayer = false;
    this->numTurn = 0;
}

void gameStart(tron * this) {
    this->player1.dirX = 1;
    this->player1.dirY = 0;
    this->player1.posX = this->boardCols * 1 / 3;
    this->player1.posY = this->boardRows / 2;
    this->player1.alive = true;

    this->player2.dirX = -1;
    this->player2.dirY = 0;
    this->player2.posX = this->boardCols * 2 / 3;
    this->player2.posY = this->boardRows / 2;
    this->player2.alive = true;

    this->curState = IN_GAME;
    this->numTurn = 0;

    for (int i = 0; i < this->boardRows; i++) {
        memset(this->gameBoard[i], ' ', this->boardCols);
    }
    makeBorder(this);
    this->gameBoard[this->player1.posY][this->player1.posX] = '1';
    this->gameBoard[this->player2.posY][this->player2.posX] = '2';
}

void makeBorder(tron * this) {
    // border
    for (int i = 0; i < this->boardCols; i++) {
        this->gameBoard[0][i] = '*';
        this->gameBoard[this->boardRows - 1][i] = '*';
    }
    for (int i = 0; i < this->boardRows; i++) {
        this->gameBoard[i][0] = '*';
        this->gameBoard[i][this->boardCols - 1] = '*';
    }
}

bool updateCyclePos(tron * this, int playerNum) {
    lightCycle * cycle = playerNum == 1 ? &this->player1 : &this->player2;
    
    int nextPosX = cycle->posX + cycle->dirX;
    int nextPosY = cycle->posY + cycle->dirY;
    // check for border wall
    if (nextPosX >= 1 && nextPosX <= this->boardCols - 2
        && nextPosY >= 1 && nextPosY <= this->boardRows - 2) {
        // moving
        char nextPosChar = this->gameBoard[nextPosY][nextPosX];
        if (nextPosChar != '*' && nextPosChar != 'B' && nextPosChar != 'Y') { // allow 2 cycle to go into same pos here. Will check for draw later
            this->gameBoard[cycle->posY][cycle->posX] = playerNum == 1 ? 'B' : 'Y';
            cycle->posX += cycle->dirX;
            cycle->posY += cycle->dirY;
            this->gameBoard[cycle->posY][cycle->posX] = playerNum == 1 ? '1' : '2';
            return true;
        }
    }
     
    // Only get here if a cycle hit wall and die
    cycle->alive = false;
    return false;
}

void deathHandler(tron* this) {
    lightCycle * one = &this->player1;
    lightCycle * two = &this->player2;

    if (one->alive && two->alive) return;

    one->dirX = 0;
    one->dirY = 0;
    two->dirX = 0;
    two->dirY = 0;

    if (!one->alive && !two->alive) {
        this->curState = DRAW;
        return;
    }
    if (one->alive && !two->alive) {
        this->curState = PLAYER1_WIN;
        return;
    }
    if (!one->alive && two->alive) {
        this->curState = PLAYER2_WIN;
        return;
    }
}

void computerMakeMove(tron * this) {
    // find destination
    int destX = this->player1.posX + 4 * this->player1.dirX;
    if (destX < 1) destX = 1;
    if (destX > this->boardCols - 2) destX = this->boardCols - 2;
    
    int destY = this->player1.posY + 2 * this->player1.dirY;
    if (destY < 1) destY = 1;
    if (destY > this->boardRows - 2) destY = this->boardRows - 2;

    lightCycle * computer = &this->player2;
    // try all direction. First check if no death, then minimize distance to destination
    // default is not turning
    int nextX = computer->posX + computer->dirX;
    int nextY = computer->posY + computer->dirY;
    int bestDistance = abs(destX - nextX) + abs(destY - nextY);
    int potentialDistance;
    bool gonnaCrash = crashChecker(nextX, nextY, this);
    bool potentialCrash;

    int ogdirX = computer->dirX;
    int ogdirY = computer->dirY;

    // test up
    if (ogdirY == 0) {
        nextX = computer->posX;
        nextY = computer->posY - 1;
        potentialDistance = abs(destX - nextX) + abs(destY - nextY);
        potentialCrash = crashChecker(nextX, nextY, this);

        bool turnUp = false;
        if (gonnaCrash && !potentialCrash) {
            turnUp = true;
        }
        else if (!potentialCrash && potentialDistance < bestDistance) {
            turnUp = true;
        }

        if (turnUp) {
            gonnaCrash = potentialCrash;
            bestDistance = potentialDistance;
            computer->dirX = 0;
            computer->dirY = -1;
        }
    }
    // test down
    if (ogdirY == 0) {
        nextX = computer->posX;
        nextY = computer->posY + 1;
        potentialDistance = abs(destX - nextX) + abs(destY - nextY);
        potentialCrash = crashChecker(nextX, nextY, this);

        bool turnDown = false;
        if (gonnaCrash && !potentialCrash) {
            turnDown = true;
        }
        else if (!potentialCrash && potentialDistance < bestDistance) {
            turnDown = true;
        }

        if (turnDown) {
            gonnaCrash = potentialCrash;
            bestDistance = potentialDistance;
            computer->dirX = 0;
            computer->dirY = 1;
        }
    }
    // test left
    if (ogdirX == 0) {
        nextX = computer->posX - 1;
        nextY = computer->posY;
        potentialDistance = abs(destX - nextX) + abs(destY - nextY);
        potentialCrash = crashChecker(nextX, nextY, this);

        bool turnLeft = false;
        if (gonnaCrash && !potentialCrash) {
            turnLeft = true;
        }
        else if (!potentialCrash && potentialDistance < bestDistance) {
            turnLeft = true;
        }

        if (turnLeft) {
            gonnaCrash = potentialCrash;
            bestDistance = potentialDistance;
            computer->dirX = -1;
            computer->dirY = 0;
        }
    }
    // test right
    if (ogdirX == 0) {
        nextX = computer->posX + 1;
        nextY = computer->posY;
        potentialDistance = abs(destX - nextX) + abs(destY - nextY);
        potentialCrash = crashChecker(nextX, nextY, this);

        bool turnRight = false;
        if (gonnaCrash && !potentialCrash) {
            turnRight = true;
        }
        else if (!potentialCrash && potentialDistance < bestDistance) {
            turnRight = true;
        }
        
        if (turnRight) {
            gonnaCrash = potentialCrash;
            bestDistance = potentialDistance;
            computer->dirX = 1;
            computer->dirY = 0;
        }
    }
    
    return;
}

bool crashChecker(int nextX, int nextY, tron * this) {
    if (nextX >= 1 && nextX <= this->boardCols - 2
        && nextX >= 1 && nextY <= this->boardRows - 2) { // make sure we don't get a seg fault
        char nextChar = this->gameBoard[nextY][nextX];
        if (nextChar == 'B' || nextChar == 'Y' || nextChar == '*' || nextChar == '1' || nextChar == '2') {
            return true;
        }
        else {
            return false;
        }
    }
    else {
        return true;
    }
}

/*** Input & Output ***/
// input
void processKeypress(tron * tronGame) {
    int c = readKey();

    if (tronGame->curState == IN_GAME && tronGame->singlePlayer) {
        computerMakeMove(tronGame);
        tronGame->numTurn++;
    }

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4); 
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case '1':
        case '2':
            if (tronGame->curState != IN_GAME) {
                tronGame->singlePlayer = c == '1' ? true : false;
                gameStart(tronGame);
            }
            break;
        case ' ':
            if (tronGame->curState != IN_GAME) {
                gameStart(tronGame);
            }
            break;
        case 'w':
        case 'W':
            if (tronGame->curState == IN_GAME && tronGame->player1.dirY == 0) {
                tronGame->player1.dirY = -1;
                tronGame->player1.dirX = 0;
            }
            break;
        case 'a':
        case 'A':
            if (tronGame->curState == IN_GAME && tronGame->player1.dirX == 0) {
                tronGame->player1.dirX = -1;
                tronGame->player1.dirY = 0;
            }
            break;
        case 's':
        case 'S':
            if (tronGame->curState == IN_GAME && tronGame->player1.dirY == 0) {
                tronGame->player1.dirY = 1;
                tronGame->player1.dirX = 0;
            }
            break;
        case 'd':
        case 'D':
            if (tronGame->curState == IN_GAME && tronGame->player1.dirX == 0) {
                tronGame->player1.dirX = 1;
                tronGame->player1.dirY = 0;
            }
            break;
        case ARROW_UP:
            if (tronGame->curState == IN_GAME && !tronGame->singlePlayer && tronGame->player2.dirY == 0) {
                tronGame->player2.dirY = -1;
                tronGame->player2.dirX = 0;
            }
            break;
        case ARROW_LEFT:
            if (tronGame->curState == IN_GAME && !tronGame->singlePlayer && tronGame->player2.dirX == 0) {
                tronGame->player2.dirX = -1;
                tronGame->player2.dirY = 0;
            }
            break;
        case ARROW_DOWN:
            if (tronGame->curState == IN_GAME && !tronGame->singlePlayer && tronGame->player2.dirY == 0) {
                tronGame->player2.dirY = 1;
                tronGame->player2.dirX = 0;
            }
            break;
        case ARROW_RIGHT:
            if (tronGame->curState == IN_GAME && !tronGame->singlePlayer && tronGame->player2.dirX == 0) {
                tronGame->player2.dirX = 1;
                tronGame->player2.dirY = 0;
            }
            break;
        case '\0':
            break;
    }

    updateCyclePos(tronGame, 1);
    updateCyclePos(tronGame, 2);
    
    // check if player2's movement kill player 1/both
    int player1PosX = tronGame->player1.posX;
    int player1PosY = tronGame->player1.posY;
    if (player1PosX >= 1 && player1PosX <= tronGame->boardCols - 2
        && player1PosY >= 1 && player1PosY <= tronGame->boardRows - 2) { // make sure we don't get a seg fault
        char player1Char = tronGame->gameBoard[player1PosY][player1PosX];
        if (player1Char == '2') { // simultaenously ran into same pos
            tronGame->player1.alive = false;
            tronGame->player2.alive = false;
        }
        else if (player1Char == 'Y') {
            tronGame->player1.alive = false;
        }
    }

    deathHandler(tronGame);
}

// output
void abAppend(struct abuf * ab, const char * s, int len) {
    char * new = (char *) realloc(ab->b, ab->len + len);
    if (new == NULL) return;

    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void drawScreen(tron * this){
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6); //hide cursor

    // draw board
    abAppend(&ab, "\x1b[H", 3);
    for (int i = 0; i < this->boardRows; i++) {
        for (int j = 0; j < this->boardCols; j++) {
            char charToPrint = this->gameBoard[i][j];
            switch (charToPrint) {
                case '1':
                    abAppend(&ab, this->player1.alive ? "\x1b[36m" : "\x1b[31m", 5); // cyan
                    abAppend(&ab, this->player1.alive ? "1" : "X", 1);
                    abAppend(&ab, "\x1b[m", 3);
                    break;
                case 'B':
                    abAppend(&ab, "\x1b[7;36m", 7);
                    abAppend(&ab, " ", 1);
                    abAppend(&ab, "\x1b[m", 3);
                    break;
                case '2':
                    abAppend(&ab, this->player2.alive ? "\x1b[33m" : "\x1b[31m", 5); // yellow
                    abAppend(&ab, this->player2.alive ? "2" : "X", 1);
                    abAppend(&ab, "\x1b[m", 3);
                    break;
                case 'Y':
                    abAppend(&ab, "\x1b[7;33m", 7);
                    abAppend(&ab, " ", 1);
                    abAppend(&ab, "\x1b[m", 3);
                    break;
                default:
                    abAppend(&ab, &charToPrint, 1);
                    break;
            }
        }

        // Potential message overlay
        if (this->curState != IN_GAME && i == this->boardRows / 3) {
            abAppend(&ab, "\r", 1);
            char message[80];
            int msgLen = 0;
            switch (this->curState) {
                case START_SCREEN:
                    msgLen = snprintf(message, sizeof(message), "Michael's Tron -- ver %s(start by pressing 1/2 to select # of player)", MICHAEL_TRON_VER);
                    break;
                case PLAYER1_WIN:
                    msgLen = snprintf(message, sizeof(message), "\x1b[36mPlayer 1 Win \x1b[0m(restart by pressing 1/2 to select # of player)");
                    break;
                case PLAYER2_WIN:
                    msgLen = snprintf(message, sizeof(message), "\x1b[33mPlayer 2 Win \x1b[0m(restart by pressing 1/2 to select # of player)");
                    break;
                case DRAW:
                    msgLen = snprintf(message, sizeof(message), "\x1b[32mDraw \x1b[0m(restart by pressing 1/2 to select # of player)");
                    break;
            }
            
            int padding = (this->boardCols - msgLen) / 2;
            for (int z = padding; z > 0; z--) abAppend(&ab, " ", 1);

            if (msgLen > this->boardCols) msgLen = this->boardCols;
            abAppend(&ab, message, msgLen);
        }
        
        abAppend(&ab, "\x1b[K", 3);  // clear line right of cursor (optional in our case)
        abAppend(&ab, "\r\n", 2);
    }
    // Instructions
    abAppend(&ab, "\x1b[7m", 4); // invert color
    char * instruction = this->singlePlayer? "Player 1: WASD | Player 2: Computer | Ctrl-Q to quit" : 
                                                "Player 1: WASD | Player2: Arrow keys | Ctrl-Q to quit";
    int instLen = strlen(instruction);
    abAppend(&ab, instruction, instLen > this->boardCols ? this->boardCols : instLen);
    abAppend(&ab, "\x1b[m", 3); // invert color

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** terminal ***/
void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_termio) == -1) {
        die("tcgetattr");
    }
    atexit(disableRawMode); // it's cool that this can be placed anywhere

    struct termios raw = orig_termio;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;    // adds a timeout to read (in 0.1s)

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) { // TCSAFLUCH defines how the change is applied
        die("tcsetattr"); 
    }
}

void disableRawMode(void) {
    write(STDOUT_FILENO, "\x1b[?25h", 6);
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termio) == -1) {
        die("tcsetattr");
    }
}

void die(const char* s) { //error handling
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    write(STDOUT_FILENO, "\r", 1);
    exit(1);
}

int readKey(void) {
    int nread;
    char c;

    nread = read(STDIN_FILENO, &c, 1);
    if(nread == -1 && errno != EAGAIN) die("read");
    else if (nread != 1) return '\0';

    if (c == '\x1b') {
        char seq[5];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        
        switch(seq[0]) {
            case 's': return ALT_S;
            case 'f': return ALT_F;
        }

        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
                else if (seq[2] == ';') {
                    if (read(STDIN_FILENO, &seq[3], 2) != 2) return '\x1b'; //this means something went wrong
                    if (seq[3] == '5') { //ctrl key pressed, seq is esc[1;5C, C is from arrow key, 1 is unchanged default keycode (used by page up/down and delete), 5 is modifier meaning ctrl
                                        //https://en.wikipedia.org/wiki/ANSI_escape_code#:~:text=0%3B%0A%7D-,Terminal%20input%20sequences,-%5Bedit%5D
                        switch (seq[4]) {
                            case 'A': return CTRL_UP;
                            case 'B': return CTRL_DOWN;
                            case 'C': return CTRL_RIGHT;
                            case 'D': return CTRL_LEFT;
                        }
                    }
                }
                else if (read(STDIN_FILENO, &seq[3], 1) == 1) {
                    if (seq[3] == '~' && seq[1] == '1') {
                        switch (seq[2]) {
                            case '5': return F5_FUNCTION_KEY;
                            case '7': return F6_FUNCTION_KEY;
                            case '8': return F7_FUNCTION_KEY;
                            case '9': return F8_FUNCTION_KEY;
                        }
                    }
                    else if (seq[3] == '~' && seq[1] == '2' && seq[2] == '0') {
                        return F9_FUNCTION_KEY;
                    }
                }
            }
            else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        }
        else if (seq[0] == '0') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'P': return F1_FUNCTION_KEY;
                case 'Q': return F2_FUNCTION_KEY;
                case 'R': return F3_FUNCTION_KEY;
                case 'S': return F4_FUNCTION_KEY;
                case 't': return F5_FUNCTION_KEY;
                case 'u': return F6_FUNCTION_KEY;
                case 'v': return F7_FUNCTION_KEY;
                case 'l': return F8_FUNCTION_KEY;
                case 'w': return F9_FUNCTION_KEY;
                case 'x': return F10_FUNCTION_KEY;
            }
        }

        return '\x1b';
    }
    else {
        return c;
    }
}

int getWindowSize (int* rows, int* cols) {
    struct winsize ws;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;  //try move cursor a ton
        return getCursorPosition(rows, cols);  
    }
    else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}

int getCursorPosition(int* rows, int* cols) {
    char buf[32];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1; //ask terminal for cursor loc

    for (i = 0; i < sizeof(buf) - 1; i++) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) !=2) return -1;
}

