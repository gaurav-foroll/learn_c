// kilo is the name of the editor from the tutorial

/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE // since getline is not a part of stdlib it part of posix hence we need this

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>


/*** defines ***/ 

#define KILO_VERSION "0.0.1" 
#define KILO_TAB_STOP 4
#define KILO_QUIT_TIMES 1

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {


    BACKSPACE = 127,
    ARROW_LEFT = 1000, // gave these higher values so that they don't conflict with char keys
    ARROW_RIGHT, // same as 1001
    ARROW_UP,   // same as 1002
    ARROW_DOWN, // same as 1003
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};


/*** data ***/


typedef struct erow {

    int size;
    int rsize;
    char* chars;
    char* render;


} erow; // struct for editor row

struct editorConfig {
    
    int cx, cy; 
    int rx;
    int rowoff; // this actually gives us the screen offset value for actual row user is on we take filerow below
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    erow* row; // make row an array of of erow structs  
    int dirty;
    char* filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct termios orig_termios;

};

struct editorConfig E;


/*** prototypes ***/

void editorSetStatusMessage(const char* fmt, ...);

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

    raw.c_cc[VMIN] = 0;  // even if input is 0 bytes it will work 
    raw.c_cc[VTIME] = 1; // 0.1 sec timeout this helps with handling escape charaters like control, pageup etc and also helps us with doing some animation or indicator like waiting or smthng
    
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr"); //tcsaflush will clear the input buffer it is an action not a mode or a flag 
}



int editorReadKey(void) {  //changed return type from char to int to handle editor keys

    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if(nread == -1 && errno != EAGAIN) die("read");
    }

    if (c == '\x1b') {
        char seq[3];

        if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {

            if(seq[1] >= '0' && seq[1] <= '9') {

                if(read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if(seq[2] == '~') {
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
            } else {
                switch (seq[1]) {

                    case 'A' : return ARROW_UP;
                    case 'B' :  return ARROW_DOWN;
                    case 'C' : return ARROW_RIGHT;
                    case 'D' : return ARROW_LEFT;
                    case 'H' : return HOME_KEY;
                    case 'F' : return END_KEY;

                }   


            }

        } else if (seq[0] == 'O') {

            switch (seq[1]) {

                case 'H' : return HOME_KEY;
                case 'F' : return END_KEY;
            }
        }

        return '\x1b';
    } else {
        return c;
    }

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


/*** row operations ***/


int editorRowCxToRx(erow* row, int cx) {
    int rx = 0;
    int j;
    for( j =0; j < cx; j++) {

        if( row->chars[j] == '\t' )
            rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
        rx++;
        
    }
    return rx;
}

void editorUpdateRow(erow* row) {
    int tabs = 0;
    int j;
     for( j = 0; j < row->size; j++)
        if (row->chars[j] == '\t') tabs++;
    free(row->render);
    row->render = malloc(row->size + tabs*(KILO_TAB_STOP - 1) + 1);

    int idx = 0;
    for ( j = 0; j < row->size; j++ ) {
        if(row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while(idx % KILO_TAB_STOP != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }

    }
    row->render[idx] = '\0';
    row->rsize = idx;


}

void editorInsertRow(int at, char* s, size_t len) {

    if(at < 0 || at > E.numrows) return;

    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    
    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);

    E.numrows++;
    E.dirty++;
}

void editorAppendRow(char* s, size_t len) {
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1); 
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);
    E.numrows++;
    E.dirty++;
}


void editorFreeRow(erow* row) {

    free(row->render);
    free(row->chars);
}

void editorDelRow(int at) {
    if(at < 0 || at >= E.numrows) return;
    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at -1));
    E.numrows--;
    E.dirty++;
}

void editorRowInsertChar(erow* row, int at, int c) {
    if(at < 0 || at >row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at+1], &row->chars[at], row->size - at + 1); // this also handles shifting all the rows up by one neat
    row->size++;
    row->chars[at] = c;
    editorUpdateRow(row);
    E.dirty++;
}

// backspace at the start of line is appending current line to the one above it deleting the current line and replacing it with one below it

void editorRowAppendString(erow* row, char* s, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowDelChar(erow* row, int at) {
    if(at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

/*** editor operations ***/
void editorInsertChar(int c) {
    if(E.cy == E.numrows) {
        editorInsertRow(E.numrows, "", 0);
    }
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++;
}

void editorDelChar(void) {
    if(E.cy == E.numrows) return;
    if(E.cx == 0 && E.cy == 0) return;

    erow* row = &E.row[E.cy];
    if(E.cx > 0) {
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    } else {

        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
}

/*** file i/o ***/

char* editorRowsToString(int*buflen) {
    int totlen = 0;
    int j;
    for(j = 0; j < E.numrows; j++)
        totlen += E.row[j].size + 1;
    *buflen = totlen;
    
    char* buf = malloc(totlen);
    char* p = buf;
    for(j = 0; j < E.numrows; j++) {
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }

    return buf;
}

void editorOpen(char* filename) {
    free(E.filename);
    E.filename = strdup(filename);
    FILE *fp = fopen(filename, "r");

    if(!fp) die("open");

    char* line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    

    while((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n'|| line[linelen - 1] == '\r')) linelen--;
        
        editorInsertRow(E.numrows, line, linelen);
    
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
    
}

void editorSave(void) {

    if(E.filename == NULL) return;

    int len;
    char* buf = editorRowsToString(&len);

    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);

    if(fd != -1) {
        if(ftruncate(fd, len) != -1) {
            if(write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                E.dirty = 0;
                editorSetStatusMessage("%d bytes written to disk", len);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    editorSetStatusMessage("can't save! I/O error %s", strerror(errno));
}
/* 
The normal way to overwrite a file is to pass the O_TRUNC flag to open(), which truncates the file completely, making it an empty file, before writing the new data into it. By truncating the file ourselves to the same length as the data we are planning to write into it, we are making the whole overwriting operation a little bit safer in case the ftruncate() call succeeds but the write() call fails. In that case, the file would still contain most of the data it had before. But if the file was truncated completely by the open() call and then the write() failed, you’d end up with all of your data lost.
*/

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

void editorScroll(void) {

    E.rx = 0;
    if(E.cy < E.numrows) {
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
    }

    if(E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if(E.cy >= E.rowoff + E.screenrows) {

        E.rowoff = E.cy - E.screenrows + 1;
    }
    if(E.rx < E.coloff){
        E.coloff = E.rx;
    }
    if(E.rx >= E.coloff + E.screencols){
        E.coloff = E.rx - E.screencols + 1;
    }
}

void editorDrawRows(struct abuf* ab) {
    int y;
    for(y = 0; y < E.screenrows; y++) {

        int filerow = y + E.rowoff;

        if(filerow >= E.numrows) {
            // if we don't have a file we print this 
            if(E.numrows == 0 && y == E.screenrows/3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome), "kilo editor -- version %s", KILO_VERSION);

                if(welcomelen > E.screencols) welcomelen = E.screencols;

                int padding = (E.screencols - welcomelen)/2;
                if(padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            } else {
    	        abAppend(ab, "~", 1);
            }

        } else {

            int len = E.row[filerow].rsize - E.coloff;
            if(len < 0) len = 0;
            if(len > E.screencols) len = E.screencols;
            abAppend(ab, &E.row[filerow].render[E.coloff], len); // handling linewise the amount of text to be displayed
        }

        abAppend(ab, "\x1b[K", 3); // erase characters after cursor or else the old line will remain
        
	    abAppend(ab, "\r\n", 2);

    
    }
}


void editorDrawStatusBar(struct abuf* ab) {

    abAppend(ab, "\x1b[7m", 4);

    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.filename ? E.filename : "[no name]", E.numrows, E.dirty ? "(modified)":  "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cy + 1, E.numrows);
    if(len > E.screencols) len = E.screencols;
    abAppend(ab, status, len);
    while (len < E.screencols) {

        if(E.screencols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        } else {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}


void editorDrawMessageBar(struct abuf* ab) {

    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if(msglen > E.screencols) msglen = E.screencols;
    if (msglen && time(NULL) - E.statusmsg_time < 5)
        abAppend(ab, E.statusmsg, msglen); 

}


void editorRefreshScreen(void) {
    editorScroll();
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6); // hide cursor so flickers are not shown 
    // abAppend(&ab, "\x1b[2J", 4); // we don't want to clear entire screen 
    abAppend(&ab, "\x1b[H", 3);
    
    editorDrawRows(&ab); // draw each row
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1, (E.rx - E.coloff) + 1); // basically translationg file position to position on  terminal
    abAppend(&ab, buf, strlen(buf));
    abAppend(&ab, "\x1b[?25h", 6);
    write(STDIN_FILENO, ab.b, ab.len);
    abFree(&ab);
}


void editorSetStatusMessage(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

/*** input ***/


void editorMoveCursor(int key) {

    erow* row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch(key) {

        case ARROW_LEFT:
            if (E.cx != 0) {
                E.cx--;
            } else if (E.cy > 0) {
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if (row && E.cx < row->size) {
                E.cx++;
            } else if (row && E.cx == row->size) {
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if (E.cy != 0) {
                E.cy--;
            }
            break;
        case ARROW_DOWN:
            if (E.cy < E.numrows) {
                E.cy++;
            }
            break;
    }

    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen  = row ? row->size : 0;
    if(E.cx > rowlen) {
        E.cx = rowlen;
    } 


}


void editorProcessKeyPress(void) {

    static int quit_times = KILO_QUIT_TIMES;

    int c = editorReadKey();

    switch(c) {
        case '\r':
            // todo
            break;


        case CTRL_KEY('q'):
            if(E.dirty && quit_times > 0) {
                editorSetStatusMessage("WARNING!!! file has unsaved changes. ""press ctrl-q %d more times to quit.", quit_times);
                quit_times--;
                return;
            }
            write(STDIN_FILENO, "\x1b[2J", 4); // clear screen 
            write(STDIN_FILENO, "\x1b[H", 3); // put cursor at top left position default (1,1)
            exit(0);
            break;

        case CTRL_KEY('s'):
            editorSave();
            break;


        case HOME_KEY:
            E.cx = 0;
            break;
        
        case END_KEY:
            if (E.cy < E.numrows) 
                E.cx = E.row[E.cy].size;
            break;

        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if(c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
            editorDelChar();
            break;


        case PAGE_UP:
        case PAGE_DOWN:
            {
                // modify mouse position first then scroll 

                if (c == PAGE_UP) {

                    E.cy = E.rowoff;
                } else if ( c == PAGE_DOWN) {
                    E.cy = E.rowoff + E.screenrows - 1;
                    if(E.cy > E.numrows) E.cy = E.numrows;
                }

                // main scrolling for key
                int times = E.screenrows;
                while(times--) {
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
                }
            }
            break;

        case ARROW_UP:
        case ARROW_LEFT:
        case ARROW_DOWN:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;

        case CTRL_KEY('l'):
        case '\x1b':
            break;
        
        default:
            editorInsertChar(c);
            break;
    }
    quit_times = KILO_QUIT_TIMES;
}



/*** init ***/

void initEditor(void) {

    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.dirty = 0;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;;
    if(getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
    E.screenrows -= 2;
}

int main(int argc, char* argv[]) {

    enableRawMode();
    write(STDOUT_FILENO, "\x1b[2J", 4);
    initEditor();
    if(argc >= 2) {
        editorOpen(argv[1]);
    }

    editorSetStatusMessage("help: ctrl-s = save | ctrl-q = quit");

    while(1) {
        editorRefreshScreen();
        editorProcessKeyPress();
    }


    return 0;
}





// File (full content)
// filerow = rowoff + y
// ────────────────────────────────────

//           rowoff = 10
//               ↓

// filerow 10   → screen row 0   (y = 0)
// filerow 11   → screen row 1   (y = 1)
// filerow 12   → screen row 2   (y = 2)
// filerow 13   → screen row 3   (y = 3)
// filerow 14   → screen row 4   (y = 4)

// ────────────────────────────────────




// File (huge)
// -------------------------
// |                       |
// |   rowoff → 10         |
// |   ↓                   |
// |   10  ← screen row 0  |
// |   11  ← screen row 1  |
// |   12  ← screen row 2  |
// |   13  ← screen row 3  |
// |   14  ← screen row 4  |
// |                       |
// -------------------------
