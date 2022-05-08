#include<ctype.h>
#include<errno.h>
#include<stdio.h>
#include<stdlib.h>
#include<termios.h>
#include<unistd.h>


struct termios base_termios;


void
die(const char *s)
{
  perror(s);
  exit(1);
}


void
disableRawMode()
{
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &base_termios) == -1)
    die("tcsetattr");
}


void
enableRawMode()
{
  if (tcgetattr(STDIN_FILENO, &base_termios) == -1)
    die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = base_termios;

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


int
main()
{
  enableRawMode();

  while (1)
  {
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
      die("read");

    if (iscntrl(c))
    {
      printf("%d\r\n", c);
    } else
    {
      printf("%d ('%c')\r\n", c, c);
    }
    if (c == 'q') break;
  }
  return 0;
}
