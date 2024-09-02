#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>

#define CTRL_KEY(k) ((k) & 0x1f) // Macro that maps a character to it's control key equivelent

// Global variable that stores original state of the terminal
struct termios orig_termios;

// Struct that holds editor configurations
struct editorConfig {
  int screenrows;
  int screencols;
  struct termios orig_termios;
};
struct editorConfig E; // Global instance of editorConfig

// Function that handles errors, restores terminal state and exit program
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4); // Clear screen
    write(STDOUT_FILENO, "\x1b[H", 3); // Reposition cursor

    perror(s); // Print the error that occured
    exit(1); // Exit the program
}

// Function to disable raw mode and restore original terminal attributes
void exitRawMode(void){
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

// Function to enable raw mode by modifying terminal attributes
void enableRawMode(void){
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    tcgetattr(STDIN_FILENO, &orig_termios); // Get current terminal attributes
    atexit(exitRawMode); // Ensure raw mode is disabled on exit

    struct termios raw = E.orig_termios; // Start with the original terminal
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // Disable certain input flags
    raw.c_oflag &= ~(OPOST); // Disable output proccessing
    raw.c_cflag |= (CS8); // Set character size to 8 bits per byte
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // Disable local flags
    raw.c_cc[VMIN] = 0; // Minimum number of bytes for non-canonical read
    raw.c_cc[VTIME] = 1; // Timeout in deciseconds for non-canonical read

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); // Apply new attributes

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

// Function to draw rows on screen
void editorDrawRows(void){
  for (int i = 0; i < E.screenrows; i++) {
    write(STDOUT_FILENO, "~\r\n", 3);
  }
}

// Function to refresh screen. (Clears screen and redraws tildes)
void editorRefreshScreen(void) {
  write(STDOUT_FILENO, "\x1b[2J", 4); // Clears screen
  write(STDOUT_FILENO, "\x1b[H", 3); // Repositions the cursor

  editorDrawRows();
  write(STDOUT_FILENO, "\x1b[H", 3);
}

// Reads user input
char editorReadKey(void){
    int nread;
    char c;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

int getCursorPosition(int *rows, int *cols) {
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  printf("\r\n");
  char c;
  while (read(STDIN_FILENO, &c, 1) == 1) {
    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } else {
      printf("%d ('%c')\r\n", c, c);
    }
  }
  editorReadKey();
  return -1;
}


// Function that gets size of the terminal window
int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  
  if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;

    return 0;
  }
}

void editorProcessKeypress(void){
    char c = editorReadKey();

    switch (c){
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
    }
}

void initEditor(void){
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main(void){
    enableRawMode();
    initEditor();
    
    while (1) {
        editorRefreshScreen();
       editorProcessKeypress();
  }
    return 0;
}
