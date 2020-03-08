#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

#define BUFSIZE 64
#define DELIM " "

struct Node {
    pid_t data;
    struct Node *next;
} Node;

// Create PID Node
struct Node* newNode(pid_t pid) {
    struct Node* node = (struct Node*)malloc(sizeof(struct Node));
    node->data = pid;
    node->next = NULL;

    return node;
}
// Print List of Processes
void printList(struct Node* head) {
    struct Node* ptr = head;
    while(ptr) {
        printf("[%d] ", ptr->data);
        ptr = ptr->next;
    }
    printf("\n");
}
// Append PID to Processes List
void appendNode(struct Node** head, pid_t pid) {
    struct Node* curr = *head;
    struct Node* node = newNode(pid);

    if (curr == NULL)
        *head = node;
    else {
        while (curr->next != NULL)
            curr = curr->next;
        curr->next = node;
    }
    //printList(*head);
}
// Delete ID from Process Linked List 
void deleteID(struct Node **head, pid_t pid) {
    struct Node *curr = *head, *prev = NULL;
    while (curr != NULL) {
        if (pid == curr->data) {
            if (prev == NULL)
                *head = curr->next;
            else {
                prev->next = curr->next;
                curr->next = prev->next;
            }
            free(curr);
            //printList(*head);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}
// Creates an array of tokenized input
char **tokenize(char *input) {
    int bufsize = BUFSIZE, i = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "Allocation error\n");
        exit(EXIT_FAILURE);
    }
    token = strtok(input, DELIM);
    while (token != NULL) {
        tokens[i++] = token;

        if (i >= bufsize) {
            bufsize += BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, DELIM);
    }
    tokens[i] = NULL;
    return tokens;
}
/*** CHANGEDIR - Change the current directory ***/
int changedir(char **args) {
    if (chdir(args[1]) != 0) {
        perror("changedir Error:");
        return -1;
    }
    return 0;
}
/*** EXTERMINATE - TODO - Kill a process by given pid ***/
int exterm(struct Node **head, pid_t pid) {     
    if(kill(pid, SIGKILL)) {
        perror("Failure - Exterminate Error");
        return 1;
    }
    printf("[%d] was killed\n", pid);
    deleteID(head, pid);

    return 0;
}
/*** LASTCOMMANDS - prints out recently typed commands (with their arguments) ***/
int lastcommands(int flag, char **commands, int num) {
    int i;

    if (flag) { // Prints out recently typed commands
        for (i = 0; i < num; i++)
            printf("%s\n", commands[i]);
        return 0;
    }
    for (i = 0; i < num; i++) {
        free(commands[i]);
        commands[i] = NULL;
        if (!(commands[i] == NULL))
            printf("Did not free index %d\n", i);
    }
    return 0;
}
/*** RUN - Runs a program with optional parameters ***/
int run (char **program, int background, struct Node **head) {
    pid_t pid = fork(); // fork child process
    int got_pid, status; // checks exit status
    
    if (pid == -1) { // error handling
        perror("Error");
        exit(EXIT_FAILURE);
    } 
    else if (pid > 0) {
        // parent process
        if (background) {
            appendNode(head, pid);
            printf("[%d]\n", pid);
            return 0;
        }

        while ((got_pid = waitpid(pid, &status, 0)))   // wait for child process to finish
            if ((got_pid != -1) || (errno != EINTR)) 
                break;	// loop only for an interrupted system call
        
        if (got_pid == -1)	// system call error handling
            perror("waitpid");
        else if (WIFEXITED(status))     // process exited normally
            printf("Process exited with value %d\n", WEXITSTATUS(status));
        else if (WIFSIGNALED(status))   // child exited on a signal
            printf("Process exited due to signal %d\n", WTERMSIG(status));
        else if (WIFSTOPPED(status))	// child was stopped
            printf("Process was stopped by signal %d\n", WIFSTOPPED(status));       
    }
    else {
        // child process
        execv(program[0], program);
        _exit(EXIT_FAILURE);   // only happens on exec error
    }
    return 0;
}
/*** QUIT - Terminates the mysh shell ***/
void quit(char **commands, int num) {
    for (int i = 0; i < num; i++) {
        free(commands[i]);
        commands[i] = NULL;
        if (!(commands[i] == NULL))
            printf("Did not free index %d\n", i);
    }
    exit(0);
}
/*** WHEREAMI - Prints the current directory ***/
int whereami() {
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) != NULL)
        printf("%s\n", cwd);
    else {
        perror("Error: ");
        return 1;
    }
    return 0;
}
/*** REPEAT - Runs program (with optional paramaters) in the background given number of times ***/
void repeat (char **args, struct Node **head) {
    char **program = NULL;
    int i;
    int num = atoi(args[1]);

    if (num == 0)
        return;

    if (args[2] == NULL)
        printf("Missing program to run. Please try again\n");
    
    program = args + 2;

    for (i = 0; i < num; i++)
        run(program, 1, head);
    return;
}
/*** EXTERMINATEALL - Terminates all current programs ***/ 
int exterminateAll(struct Node **head) {
    struct Node *curr = *head;
    int n = 0;

    if (curr == NULL) {
        printf("No processes to kill\n");
        return 1;
    }
    printf("Killing processes...\n\n");
    while (curr != NULL) {
        exterm(&curr, curr->data);
        n++;
    }
    printf("\nTotal of %d processes\n", n);
    *head = NULL;
    return 0;
}

int main(void) {
    char input[1024];
    char **args;
    char **program = NULL;
    char *commands[1024];

    int n = 0;
    int inputLen = 0;

    struct Node *head = NULL; 

    do {
        printf("# ");
        scanf(" %[^\n\r]", input);
        
        if (!strcmp(input,"quit") && n == 0)
            return 0;
        
        inputLen = strlen(input);
        commands[n] = malloc(sizeof(char)*inputLen+1);
        strcpy(commands[n++], input);
        
        // convert input into tokens
        args = tokenize(input);

        //-- BACKGROUND
        if (!strcmp(args[0],"background")) {
            if (args == NULL)
                printf("Missing program. Please try again\n");
            else {
                program = args + 1;
                run(program, 1, &head);
            }    
        }
        //-- CHANGEDIR
        else if (!strcmp(args[0],"changedir")) {
            changedir(args);
        }
        //-- EXTERMINATE
        else if (!strcmp(args[0],"exterminate")) {
            // checks if there is a ProcessID passed
            if (args[1] == NULL || !isnumber(*args[1]))
                printf("Missing or invalid process. Please try again\n");
            else
                exterm(&head, atoi(args[1]));
        }
        //-- LAST COMMANDS
        else if (!strcmp(args[0],"lastcommands")) {  
            if (args[1] == NULL) // print last commands! 
                lastcommands(1, commands, n);
            else if (!strcmp(args[1],"-c")) { // Clear last commands!
                lastcommands(0, commands, n);
                n = 0;
            }
            else
                printf("Invalid parameter passed to lastcommands\n");
        }
        //-- RUN
        else if (!strcmp(args[0],"run")) {
            // checks if there is a program passed
            if (args[1] == NULL) {
                printf("Missing program to run. Please try again\n");
            }
            else {
                program = args + 1;
                run(program, 0, &head);
            }
        }
        //-- QUIT
        else if (!strcmp(args[0],"quit")) {
            quit(commands, n);
        }
        //-- WHEREAMI
        else if (!strcmp(args[0],"whereami")) {
            whereami();  
        }
        //-- REPEAT 
        else if (!strcmp(args[0],"repeat")) {
            if (args[1] == NULL || atoi(args[1]) < 1) {
                printf("Missing or invalid number of repeats. Please try again\n");
                continue;
            }
            if (args[2] == NULL) {
                printf("Missing program. Please try again\n");
                continue;
            }
            repeat(args, &head);
        }
        //-- EXTERMINATEALL
        else if (!strcmp(args[0], "exterminateall")) {
            exterminateAll(&head);
        }
        //-- INVALID COMMAND
        else {
            printf("Invalid command. Please try again\n");
        }
    } while(strcmp(args[0],"quit"));
    return 0;
}
