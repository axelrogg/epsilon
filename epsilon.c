/***  includes ***/

#include<ctype.h>
#include<errno.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/ioctl.h>
#include<termios.h>
#include<unistd.h>


/***  defines  ***/

// Mimic <Ctrl-(something)> key
#define CTRL_KEY(k) ((k) & 0x1f)
#define EPSILON_VERSION "0.0.1"

enum editorKey
{
  // 1000 is an arbitrary choice so it doesn't conflict with <w>, <a>, <s>, or <d>
  // and with any other char representation.
  ARROW_LEFT  = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
};


/***  data  ***/
struct editorConfig
{
  int cx, cy;
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
int
editorReadKey()
{
  int nread;
  char c;

  while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
  {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }

  if (c == '\x1b')
  {
    char seq[3];

    // assume the user pressed the <escp> key if we can't read two extra bytes after an escape char.
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[')
    {
      switch (seq[1])
      {
        case 'A': return ARROW_UP;
        case 'B': return ARROW_DOWN;
        case 'C': return ARROW_RIGHT;
        case 'D': return ARROW_LEFT;
      }
    }
    return '\x1b';
  }
  return c;
}


// Gets cursor position
int
getCursorPosition(int *rows, int *cols)
{
  char buf[32];
  unsigned int i = 0;

  // n command gives terminal status information
  // 6 argument asks for cursor position
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;

  while (i < sizeof(buf) -1)
  {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
  return 0;

  editorReadKey();
  return -1;
}


// Gets size of terminal window
int
getWindowSize(int *rows, int *cols)
{
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
  {
    // C command moves cursor the right
    // B command moves cursor down
    // the result should be to move the cursor to the bottom right without going passed the edge of the screen.
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    
    return getCursorPosition(rows, cols);
  }

  *cols = ws.ws_col;
  *rows = ws.ws_row;
  return 0;
}


/***  append buffer  ***/

struct abuf
{
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}


void
abAppend(struct abuf *ab, const char *s, int len)
{
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL)
    return;

  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}


void
abFree(struct abuf *ab)
{
  free(ab->b);
}

/***  output  ***/

void
drawWelcomeMsg(struct abuf *ab)
{
  char wbuff[80];

  char *wmsg[] = {"Welcome to Epsilon",
                   "Created by antirez <http://github.com/antirez/kilo>",
                   "Lightly modified by Axel Rodríguez Chang"};
  int sentencesno = sizeof(wmsg) / sizeof(wmsg[0]);

  for (int i = 0; i < sentencesno + 1; i++)
  {
    int wlen;
    // printing an extra empty line is the only way to avoid some of the last sentence of the welcome message
    // to be printed in the first line. Couldn't figure out exactly why this happens.
    if (i == sentencesno)
    {
      wlen = snprintf(wbuff, sizeof(wbuff), "%s", "");
    }
    else
    {
      wlen = snprintf(wbuff, sizeof(wbuff), "%s\n\r", wmsg[i]);
    }

    if (wlen > E.screencols)
      wlen = E.screencols;

    int padding = (E.screencols - wlen) / 2;
    if (padding)
    {
      abAppend(ab, "~", 1);
      padding--;
    }
    while (padding--)
      abAppend(ab, " ", 1);
    abAppend(ab, wbuff, wlen);
  }
}

void
editorDrawRows(struct abuf *ab)
{
  for (int y = 0; y < E.screenrows; y++)
  {
    // the +3 takes care of the 3-row vertical offset generated by the three-sentence message.
    if (y == (E.screenrows / 3) + 3)
    {
      drawWelcomeMsg(ab);
    } else
    {
      abAppend(ab, "~", 1);
    }

    // erase part of the line to the left of the cursor
    abAppend(ab, "\x1b[K", 3);

    if (y < E.screenrows - 1)
      abAppend(ab, "\r\n", 2);
  }
}

void
editorRefreshScreen()
{
  struct abuf ab = ABUF_INIT;

  // make the cursor invisible (hides possible flickering effect while repainting screen).
  abAppend(&ab, "\x1b[?25l", 6);
  // Reposition the cursor from the bottom of the screen to the top-left corner.
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);

  // move the cursor based
  char buf[32];
  // the terminal uses 1-indexing instead of 0-indexing
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  abAppend(&ab, buf, strlen(buf));

  // make the cursor visible again
  abAppend(&ab, "\x1b[?25h", 6);


  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}


/***  input  ***/

void
editorMoveCursor(int key)
{
  switch (key)
  {
  case ARROW_LEFT:
    E.cx--;
    break;
  case ARROW_RIGHT:
    E.cx++;
    break;
  case ARROW_UP:
    E.cy--;
    break;
  case ARROW_DOWN:
    E.cy++;
    break;
  }
}

// waits for one keypress and handles it.
void
editorProcessKeyPress()
{
  int c = editorReadKey();

  switch (c)
  {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);

      exit(0);
      break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;
  }
}


/***  init  ***/

// Initialize all the fields in `E` struct.
void
initEditor()
{
  E.cx = 0;
  E.cy = 0;

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
