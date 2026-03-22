#include <stdio.h> // printf, fprintf, stderr, getchar, perror
#include <sys/wait.h> // for waitpid and its macros
#include <unistd.h> // for chdir, fork, exec, pid_t
#include <stdlib.h> // for malloc, realloc, free, exit, execvp, EXIT_SUCCESS, EXIT_FAILURE
#include <string.h> // for strcmp, strtok

#define LSH_RL_BUFSIZE 1024
#define LSH_TOKEN_BUFSIZE 64
#define LSH_TOKEN_DELIM " \t\r\n\a"


void lsh_loop(void);

char* lsh_read_line(void);

char** lsh_split_line(char*);

int lsh_execute(char**);

int lsh_launch(char** args);

/*
  Function Declarations for builtin shell commands:
*/
int lsh_cd(char **args);
int lsh_help(char **args);
int lsh_exit(char **args);

char* builtin_str[] = {
    "cd",
    "help",
    "exit"
};

int (*builtin_func[]) (char**) = {
    &lsh_cd,
    &lsh_help,
    &lsh_exit
};

int lsh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char*);
}


int main(int argc, char** argv) {

    // load config files, if any

    // run command loop
    lsh_loop();

    // perform ant shutdown/cleanup

    return EXIT_SUCCESS;

}



void lsh_loop(void) {

    char* line; // an array of char to represent line

    char** args; // an array of array of char basically array of tokens

    int status = 0;

    do {

        printf("> ");
        line = lsh_read_line();
        args = lsh_split_line(line);
        status = lsh_execute(args);

        free(line);
        free(args);
        
    } while (status);
}




char* lsh_read_line(void) {

    int bufsize = LSH_RL_BUFSIZE;
    int position = 0;
    char* buffer = malloc(sizeof(char) * bufsize);
    int c;

    if(!buffer) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // read a character
        c = getchar();

        // if we hit EOF, replace it with a null character and return

        if (c == EOF || c == '\n') {

            buffer[position] = '\0';
            return buffer;

        } else {
            buffer[position] = c;
        }
        position++;

        if (position >= bufsize) {
            bufsize += LSH_RL_BUFSIZE;
            buffer = realloc(buffer, bufsize);
            if (!buffer) {

                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

    }


}


/* 

char *lsh_read_line(void)
{
  char *line = NULL;
  ssize_t bufsize = 0; // have getline allocate a buffer for us

  if (getline(&line, &bufsize, stdin) == -1){
    if (feof(stdin)) {
      exit(EXIT_SUCCESS);  // We recieved an EOF
    } else  {
      perror("readline");
      exit(EXIT_FAILURE);
    }
  }

  return line;
}

*/

char** lsh_split_line(char* line) {

    int bufsize = LSH_TOKEN_BUFSIZE, position = 0;
    char** tokens = malloc(bufsize * sizeof(char*));
    char* token;

    if(!tokens) {
        fprintf(stderr, "lsh: allocation erro\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, LSH_TOKEN_DELIM);

    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += LSH_TOKEN_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            
            if (!tokens) {
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, LSH_TOKEN_DELIM);

    }
    tokens[position] = NULL;
    
    return tokens;
}



int lsh_launch (char** args) {

    pid_t pid, wpid;
    int status;


    pid = fork();

    if (pid == 0) {
        // child process

        if (execvp(args[0], args) == -1) {

            perror("lsh");
        }
        exit(EXIT_FAILURE);

    } else if (pid < 0) {
        
        // error forking
        perror("lsh");

    } else {
        
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    }
    
    return 1;
}


int lsh_execute (char** args) {

    int i;

    if (args[0] == NULL) {
        // an empty command was entered
        return 1;
    }

    for (i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return lsh_launch(args);
}



/*
    Builtin function implementations
*/

int lsh_cd (char** args) {

    if (args[1] == NULL) {

        fprintf(stderr, "lsh: expected argument to \"cd\"\n");

    } else {

        if (chdir(args[1]) != 0) {
            perror("lsh");
        }
    }
    return 1;
}


int lsh_help (char** args) {

    int i;
    printf("foroll's lsh\n");
    printf("type program names and arguments, and hit enter.\n");
    printf("the following are built in:\n");

    for (i = 0; i < lsh_num_builtins(); i++ ) {
        printf("    %s\n", builtin_str[i]);
    }

    printf("use the man command for information on other programs.\n");
    return 1;
}

int lsh_exit (char** args) {

    return 0;
}






// notes to look at 
/* 



strtok


#include <stdio.h>
#include <string.h>

int main() {
    char str[] = "Learn,Code,Repeat";
    char *token;

    // Get the first token
    token = strtok(str, ",");

    // Walk through the rest of the string
    while (token != NULL) {
        printf("Token: %s\n", token);
        
        // Pass NULL to continue from the saved position
        token = strtok(NULL, ",");
    }

    return 0;
}


----------------------------------------------------------------
execvp

#include <stdio.h>
#include <unistd.h>

int main() {
    // The argument vector (array of strings)
    // Must end with NULL!
    char *args[] = {"ls", "-l", "/usr/include", NULL};

    printf("About to run ls...\n");

    // Replace current process with 'ls'
    execvp(args[0], args);

    // This line ONLY runs if execvp fails (e.g., command not found)
    perror("execvp failed");
    return 1;
}

------------------------------------------------------------------

*/



// thanks to this blog post https://brennan.io/2015/01/16/write-a-shell-in-c/