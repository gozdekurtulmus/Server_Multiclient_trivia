#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <stdbool.h>
#include <math.h>
#include <sys/mman.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_CLIENTS 2
#define MAX_QUESTIONS 7

pthread_mutex_t lock;

// Dictionary of questions and answers
char questions[][256] = {"In which Italian city can you find the Colosseum?\na)Venice\nb)Rome\nc)Naples\nd)Milan",
                         "Who wrote Catch-22\na)Mark Twain\nb)Ernest Hemingway\nc)Charles Dickens\nd)Joseph Heller",
                         "Which country does not share a border with Romania?\na)Ukraine\nb)Bulgaria\nc)Hungary\nd)Poland",
                         "How many sides has a Hexagon?\na)5\nb)6\nc)7\nd)8",
                         "What is the longest river in the world?\na)Nile\nb)Yellow River\nc)Congo River\nd)Amazon River",
                         "Which country is the footballer Cristiano Ronaldo from?\na)Spain\nb)Brazil\nc)Uruguay\nd)Portugal",
                         "What does the Richter scale measure??\na)Wind Speed,\nb)Temperature\nc)Tornado Strength\nd)Earthquake intensity",
                         "Which artist and author made the SMurfs comic strips?\na)Herge\nb)Morris\nc)Peyo\nd)Edgar P. Jacobs",
                         "If you are born on the 1st of January, which star sign are you?\na)Capricorn\nb)Scorpion\nc)Libra\nd)Aries",
                         "Which famous inventor invented the telephone?\na)Thomas Edison\nb)Benjamin Franklin\nc)Alexander Graham Bell\nd)Nikola Tesla",
                         "In the Big Bang Theory, what is the name of Sheldon and Leonard's neighbour?\na)Penny\nb)Patty\nc)Lily\nd)Jessie",
                         "In which museum can you find Leonardo Da Cinvi's Mona Lisa?\na)Le Louvre\nb)Uffizi Museum\nc)British Museum\nd)Metropolitan Museum of Art",
                         "What is the largest active volcano in the world?\na)Mount Etna\nb)Mount Vesuvius\nc)Mouna Loa\nd)Mount Batur",
                         "Apart from water, what is the most popular drink in the world?\na)Tea\nb)Coffee\nc)Beer\nd)Orange Juice",
                         "Which one of the following companies has a mermaid in its logo?\na)Twitter\nb)HSBC\nc)Target\nd)Starbucks",
                         "What is the capital of New Zealand?\na)Christchurch\nb)Wellington\nc)Auckland\nd)Dunedin",
                         "What is the national animal of England?\na)Lion\nb)Puffin\nc)Rabbit\nd)Fox",
                         "What is the name of the gem in the movie Titanic?\na)Call of the Ocean\nb)Heart of Love\nc)Heart of the Ocean\nd)Call of Love",
                         "What is Marshall's job in How I Met Your Mother?\na)Architect\nb)Teacher\nc)Journalist\nd)Lawyer"};
char answers[][256] = {"b", "d", "d", "b", "a", "d", "d", "c", "a", "c", "a", "a", "c", "a", "d", "b", "a", "c", "d"};


void sigchld_handler(int signo)
{
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

struct Client
{
    char name[256];
    int questions_known;
    bool knew_answer; // new variable to keep track of whether the client knew the current answer
};

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void server_full(int *clientsocket)
{
    write(*clientsocket, "SERVER_FULL", sizeof("SERVER_FULL"));
    close(*clientsocket);
    fprintf(stderr,"New client is trying to connect, but server reached the limit. Not accepting the new client.\n");
}

void game_finished(int* newsockfd, struct Client* clients, int client_index, int* shared_connection_counter)
{
    char buffer[256];
    int n;

    bzero(buffer, 256);
    sprintf(buffer, "GAME_FINISHED");
    n = write(*newsockfd, buffer, strlen(buffer));
    sleep(1);

    bzero(buffer, 256);
    pthread_mutex_lock(&lock);
    for (int i = 0; i < *shared_connection_counter; i++)
    {
        sprintf(buffer + strlen(buffer), "\n%s: %i points", clients[i].name, clients[i].questions_known);
    }
    pthread_mutex_unlock(&lock);
    

    sprintf(buffer + strlen(buffer), "\nGame is finished.");
    n = write(*newsockfd, buffer, strlen(buffer));

    close(*newsockfd);
    pthread_mutex_lock(&lock);
    *shared_connection_counter-=1;
    pthread_mutex_unlock(&lock);

    fprintf(stderr,"%s is removed from the server.\n", clients[client_index].name);


}

void client_disconnected(int* newsockfd, int* shared_connection_counter, struct Client* clients, int client_index)
{

    fprintf(stderr,"%s is disconnected.\n", clients[client_index].name);
    pthread_mutex_lock(&lock);
    strcpy(clients[client_index].name,"NOTINITIALIZED");
    *shared_connection_counter-=1;
    clients[client_index].questions_known = 0;
    clients[client_index].knew_answer = false;
    pthread_mutex_unlock(&lock);

    close(*newsockfd);



}

void client_handler(int *sockfd, int *shared_answer_counter, int *shared_connection_counter, struct Client *clients, int *shared_question_number)
{
    struct sockaddr_in cli_addr;
    socklen_t clilen;
    char buffer[256];
    bool disconnected = false;

    int newsockfd, question, client_index,n;

    question = 0;
    while (question < MAX_QUESTIONS && disconnected == false)
    {

        // Accept new connection
        newsockfd = accept(*sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
        {
            error("Error on accept!\n");
        }

        if((int)*shared_connection_counter == MAX_CLIENTS) 
        {
            server_full(&newsockfd);
        }

        else
        {

            pthread_mutex_lock(&lock); // Acquire the lock
            *shared_connection_counter += 1;
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if(strcmp(clients[i].name, "NOTINITIALIZED") == 0)
                {
                    client_index =i;
                    break;
                }
            }
            printf("Client connected to child process %i.\n", getpid());
            pthread_mutex_unlock(&lock); // Release the lock

            // Get the username of the client
            sprintf(buffer, "Please enter your username: ");
            n = write(newsockfd, buffer, strlen(buffer));

            if (n < 0)
            {
                error("Error writing to socket");
            }

            bzero(buffer, 256);
            n = read(newsockfd, buffer, 255);

            if (n < 0)
            {
                disconnected = true;
                client_disconnected(&newsockfd, shared_connection_counter, clients, client_index);
                break;
            }

            // Using shared memory here, for adding the connected user and giving an index to it.
            // Also, for starting the new connected players from the same question, it wwill be needed.
            // Locks are used for this purpose.

            pthread_mutex_lock(&lock); // Acquire the lock

            strcpy(clients[client_index].name, buffer);
            clients[client_index].questions_known = 0;
            clients[client_index].knew_answer = false;

            question = *shared_question_number;
            strcpy(clients[client_index].name, buffer);
            clients[client_index].questions_known = 0;
            pthread_mutex_unlock(&lock); // Release the lock
            question += 1;

            printf("Client on child process %i's name is %s.\n", getpid(), clients[client_index].name);

            bzero(buffer, 256);
            sprintf(buffer, "Welcome, %s!\n\nQuestion: %s", clients[client_index].name, questions[question]);
            n = write(newsockfd, buffer, strlen(buffer));
            if (n < 0)
            {
                error("ERROR writing to socket");
            }

            while (1)
            {
                 // Wait for client's answer
                bzero(buffer, 256);
                n = read(newsockfd, buffer, 255);
                if (n < 0)
                {
                    disconnected = true;
                    client_disconnected(&newsockfd, shared_connection_counter, clients, client_index);
                    break;
                }

                clients[client_index].knew_answer = strcmp(buffer, answers[question]) == 0;
                pthread_mutex_lock(&lock); // Acquire the lock
                *shared_answer_counter += 1;
                pthread_mutex_unlock(&lock); // Release the lock

                if (clients[client_index].knew_answer)
                {
                    clients[client_index].questions_known += 1;
                }

                while (*shared_answer_counter != *shared_connection_counter)

                bzero(buffer, 256);
                pthread_mutex_lock(&lock);
                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (strcmp(clients[i].name, "NOTINITIALIZED") != 0)
                    {
                        sprintf(buffer + strlen(buffer), "\n%s: %i points", clients[i].name, clients[i].questions_known);
                    }
                }
                *shared_question_number = question;
                *shared_answer_counter = 0;
                pthread_mutex_unlock(&lock); // Release the lock
                question += 1;

                if (question == MAX_QUESTIONS)
                {
                    break;
                }

                sprintf(buffer + strlen(buffer), "\n\nQuestion: %s", questions[question]);
                n = write(newsockfd, buffer, strlen(buffer));

                if (n < 0)
                {
                    error("ERROR writing to socket");
                }
                sleep(2);
            }
        }
    }

    if (!disconnected)
    {
        game_finished(&newsockfd, clients, client_index, shared_connection_counter);
    }
}

int main(int argc, char *argv[])
{

    // Create shared memory objects to be able to use in the forks
    int fd1, fd2, fd3, fd4;

    int *shared_answer_counter;
    fd1 = shm_open("/shared_answer", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    ftruncate(fd1, sizeof(int));
    shared_answer_counter = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd1, 0);
    *shared_answer_counter = 0;

    int *shared_connection_counter;
    fd2 = shm_open("/shared_connection", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    ftruncate(fd2, sizeof(int));
    shared_connection_counter = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd2, 0);
    *shared_connection_counter = 0;

    struct Client *clients;
    fd3 = shm_open("/shared_clients", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    ftruncate(fd3, sizeof(struct Client) * MAX_CLIENTS);
    clients = mmap(NULL, sizeof(struct Client) * MAX_CLIENTS, PROT_READ | PROT_WRITE, MAP_SHARED, fd3, 0);

    int *shared_question_number;
    fd4 = shm_open("/question_number", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    ftruncate(fd4, sizeof(int));
    shared_question_number = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd4, 0);
    *shared_question_number = 0;

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        strcpy(clients[i].name, "NOTINITIALIZED");
        clients[i].questions_known = 0;
        clients[i].knew_answer = false;
    }

    // Initialize the lock, We will use locks so that forks doesn't try changing a shared object at the same time.
    pthread_mutex_init(&lock, NULL);

    struct sockaddr_in serv_addr;
    int sockfd, newsockfd, portno, pid, n, val;

    if (2 != argc)
    {

        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        fprintf(stderr, "Could not create a socket!\n");
        exit(1);
    }
    else
    {
        fprintf(stderr, "Socket created!\n");
    }

    val =1;
    n = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)); 
    if (n < 0) 
    {
        perror("server2");
        return 0;
    }


    // port number for listening
    portno = atoi(argv[1]);

    // setup the address structure
    // use INADDR_ANY to bind to all local addresses
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno);

    //  bind to the address and port with our socket
    n = bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    if (n == 0)
    {
        fprintf(stderr, "Bind completed!\n");
    }
    else
    {
        fprintf(stderr, "Could not bind to address!\n");
        close(sockfd);
        exit(1);
    }

    /*listen on the socket for connections      */
    n = listen(sockfd, 5);

    if (n == -1)
    {
        fprintf(stderr, "Cannot listen on socket!\n");
        close(sockfd);
        exit(1);
    }

    // Create the list with random questions for the play session
    int randomQuestions[MAX_QUESTIONS];

    for (int i = 0; i < MAX_QUESTIONS; i++)
    {
        randomQuestions[i] = rand() % (sizeof(questions) / sizeof(questions[0]) + 1);
        for (int j = 0; j < i; j++)
        {
            if (randomQuestions[j] == randomQuestions[i])
            {
                i--;
                break;
            }
        }
    }

    signal(SIGCHLD, sigchld_handler);

    // Fork as much as max clients, then will wait for the clients.
    for (int x = 0; x < MAX_CLIENTS+1; x++)
    {
        if ((pid = fork()) == 0)
        {
            while(1)
            {
                client_handler(&sockfd, shared_answer_counter, shared_connection_counter, clients, shared_question_number);
                printf("New connection can be made to %i now.\n", getpid());
            }
        }

    }

    wait(NULL);
    pthread_mutex_destroy(&lock);

    munmap(shared_answer_counter, sizeof(int));
    munmap(shared_connection_counter, sizeof(int));
    munmap(clients, sizeof(struct Client) * MAX_CLIENTS);

    shm_unlink("/shared_answer");
    shm_unlink("/shared_connection");
    shm_unlink("/shared_clients");

    fprintf(stderr, "Server is closed.");

    return 0;
}
