#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

// Structure to store client information
struct clientMessage {
    char message[230];
    char username[15];
    char password[15];
    pid_t pid;
    int attempts;
    int firstTime;
};

struct clientMessage userArray[285];
int arraySize = 0;
pthread_mutex_t userArrayMutex = PTHREAD_MUTEX_INITIALIZER;

sem_t *fifoSemaphore;
sem_t *maxNbSem;
const int maxNbClient = 10;
#define MAX_USER_ARRAY_SIZE 285
const char PASSWORD[] = "AE86";
const int MAX_ATTEMPTS = 3;
const char disconnectToken[] = "&*221345//^^";

/**
 * Creates a FIFO (named pipe) for communication.
 * @return 1 if successful, -1 otherwise.
 */
int createFIFO() {
    unlink("messageFIFO");
    if (mkfifo("messageFIFO", 0777) != 0)
        return -1;

    chmod("messageFIFO", 0777);
    return 1;
}

/**
 * Opens an existing FIFO for reading.
 * @return File descriptor of the opened FIFO.
 */
int openFIFO() {
    return open("messageFIFO", O_RDONLY);
}

/**
 * Reads data from a FIFO.
 * @param fd File descriptor of the FIFO.
 * @param arr Character array to store the read data.
 * @param size Size of the character array.
 * @return Number of bytes read.
 */
int readFIFO(int fd, char arr[], size_t size) {
    return read(fd, arr, size);
}

/**
 * Parses a string into separate components.
 * @param input Input string to be parsed.
 * @param username Output parameter for username.
 * @param pid Output parameter for process ID.
 * @param message Output parameter for message.
 * @param pass Output parameter for password.
 */
void parseString(char input[], char username[], int *pid, char message[], char pass[]) {
    char *token = strtok(input, " ");

    if (token != NULL) {
        strcpy(username, token);
        token = strtok(NULL, " ");
    }

    if (token != NULL) {
        *pid = atoi(token);
        token = strtok(NULL, " ");
    }

    if (token != NULL) {
        strcpy(pass, token);
        token = strtok(NULL, "");
    }

    if (token != NULL) {
        strcpy(message, token);
    }
}

/**
 * Fills a structure with parsed data.
 * @param userArray Array of client messages.
 * @param arraySize Size of the userArray.
 * @param input Input string to be parsed.
 * @param username Output parameter for username.
 * @param pid Output parameter for process ID.
 * @param message Output parameter for message.
 * @param pass Output parameter for password.
 */
void fillStruct(struct clientMessage userArray[], int *arraySize, char input[], char username[], int *pid, char message[], char pass[]) {
    parseString(input, username, pid, message, pass);

    int isNewUser = 1;

    pthread_mutex_lock(&userArrayMutex);  // Lock the mutex before accessing userArray

    if (*arraySize >= MAX_USER_ARRAY_SIZE) {
        // Handle the case when the array is full (you may want to implement additional logic)
        // For now, we're not allowing more clients once the array is full.
    }

    for (int i = 0; i < *arraySize; ++i) {
        if (userArray[i].pid == *pid) {
            // If the user is already in the array, update their message
            strcpy(userArray[i].message, message);
            isNewUser = 0;
            break;
        }
    }

    if (isNewUser) {
        // If the user is new, add them to the array
        strcpy(userArray[*arraySize].username, username);
        strcpy(userArray[*arraySize].password, pass);
        userArray[*arraySize].pid = *pid;
        userArray[*arraySize].attempts = 0;
        strcpy(userArray[*arraySize].message, message);

        // Set the firstTime flag to 1 for a new user
        userArray[*arraySize].firstTime = 1;

        (*arraySize)++;
    } else {
        // If the user is not new, set the firstTime flag to 0
        userArray[*arraySize - 1].firstTime = 0;
    }

    pthread_mutex_unlock(&userArrayMutex);  // Unlock the mutex after modifying userArray
}

/**
 * Prints information from the clientMessage structure.
 * @param userArray Array of client messages.
 * @param arraySize Size of the userArray.
 * @param pid Process ID of the client.
 */
void printStruct(struct clientMessage userArray[], int arraySize, pid_t pid) {
    for (int i = 0; i < arraySize; ++i) {
        if (userArray[i].pid == pid) {
            if (userArray[i].firstTime) {
                // Print a message if it's the first time the user has connected
                printf("%s (%d) connected\n", userArray[i].username, userArray[i].pid);
            }

            if (strcmp(userArray[i].password, PASSWORD) == 0) {
                // If the password is correct, print the user's message
                printf("%s(%d): %s\n", userArray[i].username, userArray[i].pid, userArray[i].message);
            }
            return;
        }
    }
}

/**
 * Checks if a client is blocked based on the number of login attempts.
 * @param userArray Array of client messages.
 * @param arraySize Size of the userArray.
 * @param pid Process ID of the client.
 * @return 1 if blocked, 0 otherwise.
 */
int isBlocked(struct clientMessage userArray[], int arraySize, pid_t pid) {
    for (int i = 0; i < arraySize; ++i) {
        if (userArray[i].pid == pid && userArray[i].attempts >= MAX_ATTEMPTS) {
            // Check if the user has reached the maximum number of login attempts
            return 1; // The user is blocked
        }
    }
    return 0; // The user is not blocked
}

/**
 * Updates the login attempts for a client.
 * @param userArray Array of client messages.
 * @param arraySize Size of the userArray.
 * @param pid Process ID of the client.
 */
void updateAttempts(struct clientMessage userArray[], int arraySize, pid_t pid) {
    for (int i = 0; i < arraySize; ++i) {
        if (userArray[i].pid == pid) {
            // Update the login attempts for the specified client
            userArray[i].attempts++;
            break;
        }
    }
}

/**
 * Processes client requests based on the provided data.
 * @param userArray Array of client messages.
 * @param arraySize Size of the userArray.
 * @param pid Process ID of the client.
 * @param userPass Password provided by the client.
 */
void processRequest(struct clientMessage userArray[], int *arraySize, pid_t pid, char userPass[]) {
    pthread_mutex_lock(&userArrayMutex);  // Lock the mutex before accessing userArray

    if (!isBlocked(userArray, *arraySize, pid)) {
        if (strcmp(userPass, PASSWORD) == 0) {
            // If the password is correct, process the client's request
            int index = -1;

            // Find the client in the array
            for (int i = 0; i < *arraySize; ++i) {
                if (userArray[i].pid == pid) {
                    index = i;
                    break;
                }
            }

            if (index != -1) {
                if (strcmp(userArray[index].message, disconnectToken) == 0) {
                    // Handle disconnection
                    printf("%s disconnected\n", userArray[index].username);
                    // Implement logic to handle disconnection (e.g., mark the user as disconnected)
                } else {
                    // Print the user's message
                    printStruct(userArray, *arraySize, pid);
                }
            }
        } else {
            // If the password is incorrect, update login attempts
            updateAttempts(userArray, *arraySize, pid);

            if (isBlocked(userArray, *arraySize, pid)) {
                // Skip processing the request if the user is blocked
                // This could include sending a message to the client indicating they are blocked
            }
        }
    } else {
        // Skip processing the request if the user is blocked
        // This could include sending a message to the client indicating they are blocked
    }

    pthread_mutex_unlock(&userArrayMutex);  // Unlock the mutex after accessing userArray
}

/**
 * Thread function to handle communication with a client.
 * @param arg File descriptor of the client's FIFO.
 * @return NULL.
 */
void *handleClient(void *arg) {
    int clientFd = *((int *)arg);

    char arr[255];

    // Read from the client's FIFO
    if (readFIFO(clientFd, arr, sizeof(arr) - 1) == -1) {
        printf("Cannot Read From FIFO \n");
        return NULL;
    }

    sem_post(fifoSemaphore);

    char username[15];
    int pid;
    char userPass[15];
    char message[230];

    // Fill the userArray with parsed data from the client's message
    fillStruct(userArray, &arraySize, arr, username, &pid, message, userPass);

    // Process the client's request
    processRequest(userArray, &arraySize, pid, userPass);

    // Close the client's FIFO
    close(clientFd);

    sem_post(maxNbSem);

    pthread_exit(NULL);
}

/**
 * Main function to start the server.
 * @return 0.
 */
int main() {
    printf("Waiting For Client To Connect \n");

    createFIFO();

    sem_unlink("/maxNbSem");
    sem_unlink("/myFifoSemaphore");

    // Create a named semaphore for controlling access to the critical section
    fifoSemaphore = sem_open("/myFifoSemaphore", O_CREAT, 0644, 1);
    if (fifoSemaphore == SEM_FAILED)
    {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    // Create a named semaphore for controlling the number of allowed instances
    maxNbSem = sem_open("/maxNbSem", O_CREAT, 0644, maxNbClient); // Initialize with maxNb clients
    if (maxNbSem == SEM_FAILED)
    {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    pthread_t threadId;

    while (1) {
        int fd = openFIFO();

        if (fd == -1) {
            printf("Cannot Open FIFO \n");
            return -1;
        }

        // Create a new thread for each client
        pthread_create(&threadId, NULL, handleClient, (void *)&fd);
    }

    printf("Bye");
    return 0;
}
