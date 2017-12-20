#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

/* Function Declarations for shell commands */
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_ls(char **args);
int lsh_create(char **args);
int lsh_exit(char **args);
int lsh_delete(char **args);
int remove_directory(const char *path);
int mkdir(const char *path, mode_t mode);

/* List of commands */
char *builtin_str[] = 
{
  "cd",
  "help",
  "ls",
  "create",
  "delete",
  "exit"

};

int (*builtin_func[]) (char **) = 
{
  &lsh_cd,
  &lsh_help,
  &lsh_ls,
  &lsh_create,
  &lsh_delete,
  &lsh_exit
};

int num_builtins() {
  return sizeof(builtin_str) / sizeof(char *);
}

/* Go to directory */
int lsh_cd(char **args)
{
  if (args[1] == NULL) 
  {
    fprintf(stderr, "shell: expected argument to \"cd\"\n");
  } 
  else 
  {
    if (chdir(args[1]) != 0) 
    {
      perror("shell");
    }
  }
  return 1;
}

/* HELP PLS */
int lsh_help(char **args)
{
  int i;
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (i = 0; i < num_builtins(); i++) 
  {
    printf("  %s\n", builtin_str[i]);
  }

  return 1;
}

/* GO AWAY */
int lsh_exit(char **args)
{
  return 0;
}

/* ls */
int lsh_ls(char **args)
{
  DIR *dir = opendir(".");
  if(dir)
  {
    struct dirent *ent;
    while((ent = readdir(dir)) != NULL)
    {
      puts(ent->d_name);
    }
  }
  else
  {
    fprintf(stderr, "Error opening directory\n");
  }

    return 1;  
}

/* Create directory */
int lsh_create(char **args) 
{ 
  
  if (args[1] == NULL)
   {
    fprintf(stderr, "shell: expected argument to \"create\"\n");
  } 
  else 
  { 
    mkdir(args[1], 0700);
  }

  return 1; 
}

/* Delete directory */
int lsh_delete (char **args)
{
  remove_directory(args[1]);
  return 1;
}

int remove_directory(const char *path)
{
   DIR *d = opendir(path);
   size_t path_len = strlen(path);
   int r = -1;

   if (d)
   {
      struct dirent *p;
      r = 0;
      while (!r && (p=readdir(d)))
      {
          int r2 = -1;
          char *buf;
          size_t len;
          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
          {
             continue;
          }

          len = path_len + strlen(p->d_name) + 2; 
          buf = malloc(len);

          if (buf)
          {
             struct stat statbuf;
             snprintf(buf, len, "%s/%s", path, p->d_name);
             if (!stat(buf, &statbuf))
             {
                if (S_ISDIR(statbuf.st_mode))
                {
                   r2 = remove_directory(buf);
                }
                else
                {
                   r2 = unlink(buf);
                }
             }

             free(buf);
          }

          r = r2;
      }
      closedir(d);
   }
   if (!r)
   {
      r = rmdir(path);
   }
   return r;
}

/* launch shell */
int lsh_launch(char **args)
{
  pid_t pid, wpid;
  int status;

  pid = fork();
  if (pid == 0) {
    if (execvp(args[0], args) == -1) {
      perror("shell");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    perror("shell");
  } else {
    do {
      wpid = waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

/* Check is the command builtin or no */
int lsh_execute(char **args)
{
  int i;

  if (args[0] == NULL) {
    return 1;
  }

  for (i = 0; i < num_builtins(); i++) 
  {
    if (strcmp(args[0], builtin_str[i]) == 0) 
    {
      return (*builtin_func[i])(args);
    }
  }

  return lsh_launch(args);
}

/* Read a line of input */
#define RL_BUFSIZE 1024
char *read_line(void)
{
  int bufsize = RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) 
  {
    fprintf(stderr, "shell: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) 
  {
    c = getchar();
    if (c == EOF || c == '\n') 
    {
      buffer[position] = '\0';
      return buffer;
    } 
    else 
    {
      buffer[position] = c;
    }
    position++;
    if (position >= bufsize) 
    {
      bufsize += RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer) {
        fprintf(stderr, "shell: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}

/* Split a line into tokens */
#define TOK_BUFSIZE 64
#define TOK_DELIM " \t\r\n\a"
char **split_line(char *line)
{
  int bufsize = TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;

  if (!tokens) 
  {
    fprintf(stderr, "shell: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, TOK_DELIM);
  while (token != NULL) 
  {
    tokens[position] = token;
    position++;

    if (position >= bufsize) 
    {
      bufsize += TOK_BUFSIZE;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) 
      {
        fprintf(stderr, "shell: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}


void lsh_loop(void)
{
  char *line;
  char **args;
  int status;

  do 
  {
    printf("> ");
    line = read_line();
    args = split_line(line);
    status = lsh_execute(args);

    free(line);
    free(args);
  } while (status);
}

/* Loop getting input and executing it */
int main(int argc, char **argv)
{
  printf("It's shell! To find out what it does, write ''help'' without quotes!\n");
  lsh_loop();

  return EXIT_SUCCESS;
}
