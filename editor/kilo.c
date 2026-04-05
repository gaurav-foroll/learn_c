
// kilo is the name of the editor from the tutorial 

/*** includes ***/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>


/*** defines ***/ 

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct editorConfig {

    int screenrows;
    int screencols;
    struct termios orig_termios;

};

struct editorConfig E;



/*** terminal ***/

void die(const char *s) {
    
    write(STDIN_FILENO, "\x1b[2J", 4);
    write(STDIN_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

// Disable raw mode at exit
void disableRawMode(void) {

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr");
    }
}


void enableRawMode(void) {
    
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // bit mask and to turn of echo mode

    raw.c_cc[VMIN] = 0;  //even if input is 0 bytes it will work 
    raw.c_cc[VTIME] = 1; // 0.1 sec timeout this helps with handling escape charaters and also helps us with doing some animation or indicator like waiting or smthng
    
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr"); //tcsaflush will clear the input buffer it is an action not a mode or a flag 
}



char editorReadKey(void) {

    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if(nread == -1 && errno != EAGAIN) die("read");
    }
    return c;

}

int getCursorPosition(int* rows, int* cols) {

    char buf[32];
    unsigned int i = 0;
    
    if(write(STDIN_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) -1) {

        if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if(buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if(buf[0] != '\x1b' || buf[1] != '[') return -1;
    if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    // printf("\r\n");
    // char c;
    // while(read(STDIN_FILENO, &c, 1) == 1) {

    //     if(iscntrl(c)) {

    //         printf("%d\r\n", c);

    //     } else {

    //         printf("%d ('%c')\r\n", c, c);

    //     }
    // }

    return 0;
}



int getWindowSize(int* rows, int* cols) {

    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        
        if(write(STDIN_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols); // fallback if ioctl does not work in getting rows and cols of terminal
    } else {

        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}


/*** append buffer ***/

// look in chapter 3 append buffer
struct abuf {

    char* b;
    int len;
};

#define ABUF_INIT {NULL, 0}


void abAppend(struct abuf* ab, const char* s, int len) {

    char* new =  realloc(ab->b, ab->len + len);

    if(new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}


void abFree(struct abuf* ab) {
    free(ab->b);
}


/*** output ***/

void editorDrawRows(void) {
    int y;
    for(y = 0; y < E.screenrows; y++) {

        write(STDIN_FILENO, "~", 1);

        if(y < E.screenrows -1) {
            write(STDIN_FILENO, "\r\n", 2);
        }
    }
}


void editorRefreshScreen(void) {
    write(STDIN_FILENO, "\x1b[2J", 4);
    write(STDIN_FILENO, "\x1b[H", 3);
    
    editorDrawRows(); // draw ~ in each row
    write(STDIN_FILENO, "\x1b[H", 3);
}




/*** input ***/

void editorProcessKeyPress(void) {

    char c = editorReadKey();

    switch(c) {

        case CTRL_KEY('q'):
            write(STDIN_FILENO, "\x1b[2J", 4); // clear screen 
            write(STDIN_FILENO, "\x1b[H", 3); // put cursor at top left position default (1,1)
            exit(0);
            break;
    }
}



/*** init ***/

void initEditor(void) {
    if(getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main(void) {

    enableRawMode();
    initEditor();

    while(1) {
        editorRefreshScreen();
        editorProcessKeyPress();
    }


    return 0;
}