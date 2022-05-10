/***  includes ***/

#include<ctype.h>
#include<errno.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/ioctl.h>
#include<termios.h>
#include<unistd.h>


/***  terminal  ***/

// Mimic <Ctrl-(something)> key
#define CTRL_KEY(k) ((k) & 0x1f)


/***  data  ***/
struct editorConfig
{
  int screenrows;
  int screencols;
  struct termios base_termios;
};

struct editorConfig E;


/***  terminal  ***/

void
die(const char *s)
{
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}


void
disableRawMode()
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.base_termios) == -1)
    die("tcsetattr");
}


void
enableRawMode()
{
  if (tcgetattr(STDIN_FILENO, &E.base_termios) == -1)
    die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = E.base_termios;

  // read the current terminal attributes into raw.
  tcgetattr(STDIN_FILENO, &raw);

  /* 
   *  FLAGS:
   *  ------
   *  
   *  CS8    : (Not actually a flag); sets the character size (CS) to 8 bits per byte.
   *  --------
   *  BRKINT : Turn off possible SIGINT signals to be sent (like pressing <Ctrl-C>).
   *  ICRNL  : Make <Ctrl-M> to be read as ASCII 13 instead of being translated to ASCII 10.
   *  INPCK  : Turn off parity checking.
   *  ISTRIP : Turn off feature that causes 8th bit of each input byte to be stripped.
   *  IXON   : Turn off <Ctrl-S> and <Ctrl-Q> transmission control.
   *  --------
   *  ECHO   : Turn off printing each key press to the terminal.
   *  ICANON : Turn off canonical mode (we can read input byte-by-byte instead of line-by-line).
   *  IEXTEN : Turn off <Ctrl-V> waiting feature.
   *  ISIG   : Turn off <Ctrl-C> (SIGINT) and <Ctrl-Z> (SIGTSTP) signals.
   *  --------
   *  OPOST  : Turn off output translation from '\n' to '\r\n'.
   *
   */

  raw.c_cflag |= (CS8);
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_oflag &= ~(OPOST);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}


// Waits for one keypress and returns it
char
editorReadKey()
{
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
  {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }
  return c;
}


int getWindowSize(int *rows, int *cols)
{
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    return -1;

  *cols = ws.ws_col;
  *rows = ws.ws_row;
  return 0;
}


/***  output  ***/

void
editorDrawRows()
{
  for (int y = 0; y < E.screenrows; y++)
    write(STDOUT_FILENO, "~\r\n", 3);
}

void
editorRefreshScreen()
{
  // write 4 bytes to the terminal to clear the entire screen.
  write(STDOUT_FILENO, "\x1b[2J", 4);
  // Reposition the cursor from the bottom of the screen to the top-left corner.
  write(STDOUT_FILENO, "\x1b[H", 3);

  editorDrawRows();

  // Go back up
  write(STDOUT_FILENO, "\x1b[H", 3);
}


/***  input  ***/

// waits for one keypress and handles it.
void
editorProcessKeyPress()
{
  char c = editorReadKey();

  switch (c)
  {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);

      exit(0);
      break;
  }
}


/***  init  ***/

void
initEditor()
{
  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("getWindowSize");
}


int
main()
{
  enableRawMode();
  initEditor();

  while (1)
  {
    editorRefreshScreen();
    editorProcessKeyPress();
  }
  return 0;
}
