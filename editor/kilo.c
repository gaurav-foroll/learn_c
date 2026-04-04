
// kilo is the name of the editor from the tutorial 

/*** includes ***/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>



/*** data ***/

struct termios orig_termios;



/*** terminal ***/

void die(const char *s) {
    perror(s);
    exit(1);
}

// Disable raw mode at exit
void disableRawMode(void) {

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
        die("tcsetattr");
    }
}


void enableRawMode(void) {
    
    if(tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);

    struct termios raw = orig_termios;
    
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // bit mask and to turn of echo mode

    raw.c_cc[VMIN] = 0;  //even if input is 0 bytes it will work 
    raw.c_cc[VTIME] = 1; // 0.1 sec timeout this helps with handling escape charaters and also helps us with doing some animation or indicator like waiting or smthng
    
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr"); //tcsaflush will clear the input buffer it is an action not a mode or a flag 
}


/*** main ***/

int main(void) {

    enableRawMode();


    while(1) {

        char c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
        
        if(iscntrl(c)) {
            printf("%d\r\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }

        if (c == 'q') break;
    }


    return 0;
}