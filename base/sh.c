// Shell.

#include "types.h"
#include "user.h"
#include "fcntl.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10
#define MAXHIST 10
#define MAXCMDLEN 100

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

//struct histcmd{
  //cmdHist should malloc enough bytes to store a command every time we call a new cmd
  char *cmdHist[MAXHIST+1][MAXCMDLEN]; //stores the history, we want an extra row in the bottom of the array to act as a trash can
  int cmdAmt;
  //cmdHist is empty, we print the entire thing no matter if something is in it
//};


int fork1(void);  // Fork but panics on failure.
void panic(char*);
struct cmd *parsecmd(char*);

// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  //int p[2];
  //struct backcmd *bcmd;
  struct execcmd *ecmd;
  //struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;
  if(cmd == 0)
    exit();

  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit();
    exec(ecmd->argv[0], ecmd->argv);
    printf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    //printf(2, "Redirection Not Implemented\n");
    // cast to redircmd struct
    rcmd = (struct redircmd*) cmd;
    // close fd to be redirected
    close(rcmd->fd);
    // open selected file
    if (open(rcmd->file, rcmd->mode) < 0) {
      printf(2, "open %s failed\n", rcmd->file);
      exit();
    }

    // run command after redirection
    runcmd(rcmd->cmd);
    break;

  case LIST:
    printf(2, "List Not Implemented\n");
    break;

  case PIPE:
    // printf(2, "Pipe Not implemented\n");
    int pdfs[2];
    pcmd = (struct pipecmd*) cmd;

    int proc_pipe = pipe(pdfs);
    if(proc_pipe < 0) { 
      panic("pipe");
    }

    int first_child = fork1();
    if(first_child == 0) { // in first child
        close(1);
        dup(pdfs[1]);
        close(pdfs[0]);
        close(pdfs[1]);

        runcmd(pcmd->left);
    }

    int second_child = fork1();
    if(second_child == 0) { // in second child
        close(0);
        dup(pdfs[0]);
        close(pdfs[1]);
        close(pdfs[0]);
        
        runcmd(pcmd->right);
    }

    close(pdfs[1]);
    close(pdfs[0]);
    wait();
    wait();
    break;

  case BACK:
    printf(2, "Backgrounding not implemented\n");
    break;
  }
  exit();
}

int
getcmd(char *buf, int nbuf)
{
  printf(2, "$ ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

void addHist(char *cmd){
  //if our cmd is non null
  if(cmd[0] != '\0'){
    //printf(2,"curr cmd count: %p\n", cmdAmt);
    int cmdLen = strlen(cmd);
    if(cmdAmt < MAXHIST){
      cmdAmt++;
      //printf(2, "adding to cmdAmt. Curr amt = %p\n", cmdAmt);
    }
    int index;
    for(index =cmdAmt-1; index>=0;index--){ //start from the bottom and move things down
      memmove(cmdHist[index+1],cmdHist[index],sizeof(char)*MAXCMDLEN); //if we are at 10, we move the last cmd into the trash
    }
    memmove(cmdHist[0], cmd, sizeof(char)*MAXCMDLEN);
  }
}

int
main(void)
{
  static char buf[100];
  int fd;
  //cmdAmt =0; //how many commands we have
  // Ensure that three file descriptors are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Chdir must be called by the parent, not the child.
      buf[strlen(buf)-1] = 0;  // chop \n
      if(chdir(buf+3) < 0)
        printf(2, "cannot cd %s\n", buf+3);
      continue;
    }
    else if(buf[0] == 'h' && buf[1] == 'i' && buf[2] == 's' && buf[3] == 't' && buf[4] == ' '){
      //printf(2, "this is where we do history stuff\n");
      //breaking the code a tiny bit
      //this is the code that should execute if we have "print" after "hist"
      //otherwise if the char that comes after hist is a digit, call runcmd(parsecmd(cmdHist[index]))????
      int asciiVal = (int)buf[5];
      int secondChar = (int)buf[6];
      //printf(2, "ascii val of index is %d\n", asciiVal);
      if(buf[5] == 'p'){
        int index;
        if(cmdAmt>0){
          for(index=0;index<cmdAmt;index++){
            printf(2, "Previous command %d: %s",index+1, cmdHist[index]);
          }
        }
      }
      else if(asciiVal > 47 && asciiVal < 58 && asciiVal-48 <= cmdAmt){
        char *cmd;
        int index;
        //in case we have 10 which is a double digit
        if(asciiVal == 49 && secondChar == 48){
          cmd = cmdHist[9];
        }
        //else we should be fine for single digits
        else{
            for(index =0; index<cmdAmt-1;index++){
            if(index == asciiVal-49){
              cmd = cmdHist[index];
              //printf(2, "extracting cmd %s",cmdHist[index]);
              break;
            }
          }
        }
        if(fork1() == 0){
          runcmd(parsecmd(cmd));
        }
      }
      else{
        printf(2,"Error: incorrect history command format\ncontinuing...\n");
      }
      continue;
    }
    else{
      addHist(buf);
    }
    if(fork1() == 0){
      runcmd(parsecmd(buf));
    }
    wait();
  }
  exit();
}

void
panic(char *s)
{
  printf(2, "%s\n", s);
  exit();
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Constructors

struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

struct cmd*
parsecmd(char *s)
{
  //s is the input
  char *es;
  //es is the input + its len at the end
  struct cmd *cmd;
  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  //if there is parts of the input that isnt being used/processed
  if(s != es){
    printf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd*
//ps is a pointer to a string
parseline(char **ps, char *es)
{
  struct cmd *cmd;
  //ps is a pointer to the input
  cmd = parsepipe(ps, es);
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;
  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;
  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    case '+':  // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd*
parseblock(char **ps, char *es)
{
  struct cmd *cmd;
  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;
  if(peek(ps, es, "(")){
    return parseblock(ps, es);
  }

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS){
      panic("too many args");
    }
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd*
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;
  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
