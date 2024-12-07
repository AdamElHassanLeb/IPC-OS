#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <semaphore.h>

sem_t *fifoSemaphore;
sem_t *maxNbSem;
const int FIFOSECONDS = 5;
const int CLIENTSECONDS = 10;

/**
 * @brief Opens the FIFO for writing and returns the file descriptor.
 * 
 * @return int The file descriptor of the opened FIFO.
 */
int openFIFO()
{
    // Open the FIFO for writing
    return open("messageFIFO", O_WRONLY);
}

/**
 * @brief Writes data to the specified file descriptor.
 * 
 * @param fd The file descriptor to write to.
 * @param arr The array of characters to write.
 * @param size The size of the array to write.
 * @return int Returns 0 on success, -1 on failure.
 */
int writeToFIFO(int fd, char arr[], size_t size)
{
    // Write data to the specified file descriptor
    return write(fd, arr, size) == -1;
}

/**
 * @brief Builds a string structure with username, process ID, and password.
 * 
 * @param username The username.
 * @param pid The process ID.
 * @param newArr The destination array to store the built string.
 * @param pass The password.
 */
void buildStringStructure(char username[], int pid, char newArr[], char pass[])
{
    // Build a string structure with username, process ID, and password
    newArr[0] = '\0';

    strcat(newArr, username);
    strcat(newArr, " ");
    char int_string[10];
    sprintf(int_string, "%d", pid);
    strcat(newArr, int_string);
    strcat(newArr, " ");
    strcat(newArr, pass); // Add password at the end
}

/**
 * @brief Creates a message string based on the user's input.
 * 
 * @param arr The destination array to store the final message.
 * @param createdString The initial string structure.
 * @param message The user's input message.
 */
void createMessageString(char arr[], char createdString[], char message[])
{
    // Create a message string based on the user's input
    if (strcmp(message, "disconnect") == 0)
    {
        strcpy(arr, createdString);
        strcat(arr, " &*221345//^^");
    }
    else
    {
        strcpy(arr, createdString);
        strcat(arr, " ");
        strcat(arr, message);
    }
}

/**
 * @brief The main function where the user interacts with the program.
 * 
 * @return int Returns 0 on successful execution.
 */
int main()
{
    char arr[230];
    char sendArr[255] = "";
    char username[15];
    char password[10];
    char builtArr[40];
    int fd;
    pid_t mypid;
    const int maxNbClient = 10;

    mypid = getpid();

    // Open semaphores for synchronization
    fifoSemaphore = sem_open("/myFifoSemaphore", O_RDWR);
    maxNbSem = sem_open("/maxNbSem", O_RDWR);

    // Get user credentials
    printf("Please Enter Your Username \n");
    scanf("%s", username);

    printf("Please Enter The Password \n");
    scanf("%s", password);

    // Build the initial string structure
    buildStringStructure(username, mypid, builtArr, password);

    while (1)
    {
        int c;
        // Prompt user for message input
        printf("Please Enter The Message You Want to Send %s \n", username);
        fflush(stdout);
        fgets(arr, sizeof(arr), stdin);

        // Remove the newline character if present
        if ((strlen(arr) > 0) && (arr[strlen(arr) - 1] == '\n'))
        {
            arr[strlen(arr) - 1] = '\0';
        }

        // Create the final message string
        createMessageString(sendArr, builtArr, arr);

        // Wait until a spot in the queue becomes available
        sem_wait(maxNbSem);

        // Open the FIFO for writing
        fd = openFIFO();

        if (fd == -1)
        {
            printf("Cannot Open FIFO \n");
            return -1;
        }

        // Wait until semaphore is available
        sem_wait(fifoSemaphore);

        // Write the message to the FIFO
        if (writeToFIFO(fd, sendArr, sizeof(sendArr) - 1) == -1)
        {
            printf("Cannot Write To FIFO \n");
            return -1;
        }

        // Close the FIFO
        close(fd);

        // Release the spot in the queue
        sem_post(maxNbSem);

        // Check for disconnect command
        if (strcmp(arr, "disconnect") == 0)
        {
            printf("Disconnecting...\n");
            break;
        }
    }

    // Close the semaphores
    sem_close(fifoSemaphore);
    sem_close(maxNbSem);

    return 0;
}
