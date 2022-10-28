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


/*** Definitions ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define MICHAEL_TRON_VER "0.1"

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
typedef enum direction {
    UP,
    DOWN,
    LEFT,
    RIGHT
} direction;

typedef enum gameState {
    START_SCREEN,
    IN_GAME,
    LOST
} gameState;

typedef struct tron {
    char ** gameBoard;
    int boardRows;
    int boardCols;
    
    int playerPosX;
    int playerPosY;
    direction playerDir;

    gameState curState;
} tron;

/*** Tron Functions ***/
void gameInit(tron * this);
void makeBorder(tron * this);

/*** Input & Output ***/
void processKeypress(tron * this);

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

    tron tronGame;
    gameInit(&tronGame);
    makeBorder(&tronGame);

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

    this->gameBoard = (char **) malloc(sizeof(char *) * this->boardRows);
    for (int i = 0; i < this->boardRows; i++) {
        this->gameBoard[i] = (char *) malloc(this->boardCols);
        memset(this->gameBoard[i], ' ', this->boardCols);
    }

    this->playerDir = RIGHT;
    this->playerPosX = this->boardCols / 2;
    this->playerPosY = this->boardRows / 2;

    this->curState = IN_GAME;
}

void makeBorder(tron * this) {
    for (int i = 0; i < this->boardCols; i++) {
        this->gameBoard[0][i] = '*';
        this->gameBoard[this->boardRows - 1][i] = '*';
    }
    for (int i = 0; i < this->boardRows; i++) {
        this->gameBoard[i][0] = '*';
        this->gameBoard[i][this->boardCols - 1] = '*';
    }
}

/*** Global Input & Output ***/
// input
void processKeypress(tron * this) {
    
    
    int c = readKey();

    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4); 
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case '\0':
            // so vmin works in WSL now, and 0.1s per update is decently fast
            if (this->gameBoard[0][0] == '*') {
                this->gameBoard[0][0] = '@';
            }
            else {
                this->gameBoard[0][0] = '*';
            }
            break;
        case ARROW_UP:
        case ARROW_LEFT:
        case ARROW_DOWN:
        case ARROW_RIGHT:
            break;
    }
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
            abAppend(&ab, &charToPrint, 1);
        }
        
        abAppend(&ab, "\x1b[K", 3);  // clear line right of cursor
        if (i != this->boardRows - 1) {
            abAppend(&ab, "\r\n", 2);
        }
    }

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
    /*
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if(nread == -1 && errno != EAGAIN) die("read");
    }
    */
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
    //printf("\r\n%s\r\n", &buf[1]);
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) !=2) return -1;
}

