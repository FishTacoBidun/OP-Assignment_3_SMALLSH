#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

//constants
#define MAX 512

//linked list
struct commandLine
{
    int lineID;
    struct commandLine** next;
};

//global variables
bool ifBackground = true;
bool ifForeground = false;
bool ifSigtstp = false;

int main()
{
    //variables
    char commands[MAX][MAX];
    int numElements = 0;
    int statusType = 1;
    int statusValue = 0;

    struct commandLine* processList = NULL;

    //ignore SIGINT
    struct sigaction ignore = {{0}};
    struct sigaction firstAction = {{0}};
    ignore.sa_handler = SIG_IGN;
    sigaction(SIGINT, &ignore, &firstAction);

    //deal with SIGSTP
    struct sigaction dealSigtstp = {{0}};
    dealSigtstp.sa_handler = dealWithSigtstp;
    sigfillset(&dealSigtstp.sa_mask);
    dealSigtstp.sa_flags = 0; // I don't think this line is necessary.
    sigaction(SIGTSTP, &dealSigtstp, NULL);

    while (true)
    {
        ifBackgroundProcess(&processList);

        getCommands(commands, &numElements);

        replaceTwo$(commands, numElements);

        if (strcmp(commands[0], "exit") == 0)
        {
            exitPreperation(&processList);
            break;
        }
        else if (strcmp(commands[0], "status") == 0)
        {
            outputStatus(statusType, statusValue);
        }
        else if (strcmp(commands[0], "cd") == 0)
        {
            if (numElements == 1)
            {
                changeDirectory("");
            }
            else
            {
                changeDirectory(commands[1]);
            }
        }
        else if (numElements == 0)
        {
        }
        else if (commands[0][0] == '#')
        {
        }
        else
        {
            execute(&processList, &firstAction, commands, numElements, &statusType, &statusValue, &ifBackground);
        }
    }

    return 0;
}


void execute(struct commandLine** processList, struct sigaction* firstAction, char commands[MAX][MAX], int numElements, int* statusType, int* statusValue, bool* ifBackground)
{
    bool isInBackground = false;
    char redirectInput[MAX] = "";
    char redirectOutput[MAX] = "";

    if (strcmp(commands[numElements - 1], "&") == 0)
    {
        if (*ifBackground == true)
        {
            isInBackground = true;
        }

        numElements--;
    }

    bool doubleCheck = false;

    if (strcmp(commands[numElements - 2], "<") == 0)
    {
        strcpy(redirectInput, commands[numElements - 1]);
        numElements = numElements - 2;
        doubleCheck = true;
    }
    else if (strcmp(commands[numElements - 2], ">") == 0)
    {
        strcpy(redirectOutput, commands[numElements - 1]);
        numElements = numElements - 2;
        doubleCheck = true;
    }

    if (doubleCheck == true)
    {
        if (strcmp(commands[numElements - 2], "<") == 0)
        {
            strcpy(redirectInput, commands[numElements - 1]);
            numElements = numElements - 2;
        }
        else if(strcmp(commands[numElements - 2], ">") == 0)
        {
            strcpy(redirectOutput, commands[numElements - 1]);
            numElements = numElements - 2;
        }
    }

    char* arguments[numElements];

    for (int i = 0; i < numElements; i++)
    {
        arguments[i] = calloc(MAX, sizeof(char));
        strcpy(arguments[i], commands[i]);
    }
    
    arguments[numElements] = NULL;
    pid_t makePid = -5;
    int childExit = -5;
 
    makePid = fork();
 
    if (makePid == -1)
    {
        perror("Error when attempting to fork!\n");
        exit(1);
    }
    else if (makePid == 0)
    {
        if (isInBackground == true)
        {
            if (strcmp(redirectInput, "") == 0)
            {
                strcpy(redirectInput, "/dev/null");
            }
            if (strcmp(redirectOutput, "") == 0)
            {
                strcpy(redirectOutput, "/dev/null");
            }
        }

        if (strcmp(redirectInput, "") != 0)
        {
            int source = open(redirectInput, O_RDONLY);

            if (source == -1) {
                perror("Error when opening file for input redirection!");
                exit(1);
            }

            int result = dup2(source, 0);

            if (result == -1)
            {
                perror("Error when initiating input redirection!");
                exit(1);
            }
        }

        if (strcmp(redirectOutput, "") != 0)
        {
            int target = open(redirectOutput, O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (target == -1) 
            {
                perror("Error when opening file for output redirection!");
                exit(1);
            }

            int result = dup2(target, 1);

            if (result == -1)
            {
                perror("Error when initiating output redirection!");
                exit(1);
            }
        }

        if (isInBackground == false)
        {
            sigaction(SIGINT, firstAction, NULL);
        }

        struct sigaction ignore = {{0}};
        ignore.sa_handler = SIG_IGN;
        sigaction(SIGTSTP, &ignore, NULL);

        if (execvp(*arguments, arguments) < 0)
        {
            perror("Error when attempting to execute command!");
            exit(1);
        }
    }
    
    if (isInBackground == false)
    {
        ifForeground = true;
        ifSigtstp = false;

        int pidResult = -1;
        
        while (pidResult == -1)
        {
            pidResult = waitpid(makePid, &childExit, 0);
        }

        ifForeground = false;

        if (ifSigtstp == true)
        {
            ifSigtstp = false;
            toggleMode();
        }

        if (WIFEXITED(childExit) != 0)
        {
            *statusType = 1;
            *statusValue = WEXITSTATUS(childExit);
        }
        else if (WIFSIGNALED(childExit) != 0)
        {
            *statusType = 0;
            *statusValue = WTERMSIG(childExit);
            printf("terminated by signal %d\n", *statusValue);
        } 
        else 
        {
            perror("A process ended for reasons unknown!");
            exit(1);
        }
    } 
    else 
    {
        char backgroundMessage[MAX];
        printf(backgroundMessage, "background pid is %d", makePid);
        printf("%s\n", backgroundMessage);

        rememberProcess(processList, makePid);
    }

    for (int i = 0; i < numElements; i++)
    {
        free(arguments[i]);
    }

    return;
}

void toggleMode()
{
    if (ifBackground == true)
    {
        ifBackground = false;
        printf("\nEntering foreground-only mode (& is now ignored)\n");
    }
    else
    {
        ifBackground = true;
        printf("\nExiting foreground-only mode\n");
    }
}


void dealWithSigtstp(int signo)
{
    if (ifForeground == false)
    {
        toggleMode();
    }
    else
    {
        ifSigtstp = true;
    }
    return;
}


void forget(int lineID, struct commandLine** processList)
{
    struct commandLine* curr = *processList;
    struct commandLine* prev = NULL;

    while (curr->lineID != lineID)
    {
        prev = curr;
        curr = curr->next;
    }

    if (prev != NULL)
    {
        prev->next = curr->next;
    }
    else
    {
        *processList = curr->next;
    }

    free(curr);

    return;
}


void rememberProcess(struct commandLine** processList, int remember)
{
    if (*processList == NULL) 
    {
        *processList = (struct commandLine*)malloc(sizeof(struct commandLine));
        (*processList)->lineID = remember;
        (*processList)->next = NULL;
    } 
    else 
    {
        struct commandLine* curr = (*processList)->next;
        struct commandLine* prev = *processList;
        
        while (curr != NULL) 
        {
            prev = curr;
            curr = curr->next;
        }

        prev->next = (struct commandLine*)malloc(sizeof(struct commandLine));
        
        prev->next->lineID = remember;
        prev->next->next = NULL;
    }

    return;
}


void ifBackgroundProcess(struct commandLine** processList)
{
    struct commandLine* curr = *processList;
    struct commandLine* temp = NULL;

    while (curr != NULL)
    {
        temp = curr;
        curr = curr->next;
        processStatus(temp->lineID, processList);
    }

    return;
}


void processStatus(int lineID, struct commandLine** processList)
{
    int ifExited = -5;
    int childExit = -5;
    int statusValue = -5;
    char status[MAX];

    ifExited = waitpid(lineID, &childExit, WNOHANG);

    if (ifExited == 0) 
    {
        return;
    }

    if (WIFEXITED(childExit) != 0)
    {
        statusValue = WEXITSTATUS(childExit);
        printf(status, "background pid %d is done: exit value %d", lineID, statusValue);
        printf("%s\n", status);
        forget(lineID, processList);
    }
    else if (WIFSIGNALED(childExit) != 0)
    {
        statusValue = WTERMSIG(childExit);
        printf(status, "background pid %d is done: terminated by signal %d", lineID, statusValue);
        printf("%s\n", status);
        forget(lineID, processList);
    }

    return;
}


void getCommands(char commands[MAX][MAX], int* numElements)
{
    size_t bufferSize = 0;
    int numChars = -5;
    char* line = NULL;

    while(true)
    {
        printf(": ");

        numChars = getline(&line, &bufferSize, stdin);

        if (numChars == -1)
        {
            clearerr(stdin);
        }
        else
        {
            break;
        }
    }

    line[numChars - 1] = 0;

    char* token = NULL;
    int index = 0;

    token = strtok(line, " ");
    while (token != NULL)
    {
        strcpy(commands[index], token);
        index++;
        token = strtok(NULL, " ");
    }

    free(line);

    *numElements = index;
    return;
}


void replaceTwo$(char commands[MAX][MAX], int numElements)
{
    for (int i = 0; i < numElements; i++) 
    {

        bool currWord = false;

        while (currWord == false)
        {
            int wordLength = strlen(commands[i]);
            bool change = false;

            for (int j = 0; j < wordLength - 1; j++)
            {
                char firstChar = commands[i][j];
                char secondChar = commands[i][j + 1];

                if (firstChar == '$' && secondChar == '$')
                {
                    char temp[MAX + 10];
                    int l;

                    for (l = 0; l < j; l++)
                    {
                        temp[l] = commands[i][l];
                    }

                    int pid = getpid();
                    char tempPid[10];
                    sprintf(tempPid, "%d", pid);
                    int tempPidLength = strlen(tempPid);

                    for (l = 0; l < tempPidLength; l++)
                    {
                        temp[j + l] = tempPid[l];
                    }

                    int g = 0;

                    for (l = j + 2; l < wordLength; l++)
                    {
                        temp[j + tempPidLength + g] = commands[i][l];
                        g++;
                    }

                    temp[j + tempPidLength + g] = 0;

                    strcpy(commands[i], temp);

                    change = true;

                    break;
                }
            }

            if (change == false)
            {
                currWord = true;
            }
        }
    }

    return;
}


void outputStatus(int statusType, int statusValue)
{
    if (statusType == 1)
    {
        printf("exit value ");
    }
    else
    {
        printf("terminated by signal "); 
    }

    char SigOrVal[5];
    printf(SigOrVal, "%d", statusValue);
    printf("%s\n", SigOrVal);

    return;
}

void changeDirectory(char temp[MAX])
{
    const char* home = getenv("HOME");

    if (strlen(temp) == 0 || strcmp(temp, "~") == 0)
    {
        chdir(home);
    }
    else
    {
        chdir(temp);
    }

    return;
}


void exitPreperation(struct commandLine** processList)
{
    struct commandLine* curr = *processList;
    struct commandLine* temp = NULL;

    while (curr != NULL)
    {
        temp = curr;
        curr = curr->next;
        kill(temp->lineID, SIGKILL);

        int childExit;
        int statusValue;
        char status[MAX];

        waitpid(temp->lineID, &childExit, 0);

        if (WIFEXITED(childExit) != 0)
        {
            statusValue = WEXITSTATUS(childExit);
            printf(status, "background pid %d is done: exit value %d", temp->lineID, statusValue);
            printf("%s\n", status);
            forget(temp->lineID, processList);
        }
        else if (WIFSIGNALED(childExit) != 0)
        {
            statusValue = WTERMSIG(childExit);
            printf(status, "background pid %d is done: terminated by signal %d", temp->lineID, statusValue);
            printf("%s\n", status);
            forget(temp->lineID, processList);
        }
    }

    return;
}


