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
    dealSigtstp.sa_flags = 0;
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
            executeCommands(commands, numElements, &statusType, &statusValue, &ifBackground, &processList, &firstAction);
        }
    }

    return 0;
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
