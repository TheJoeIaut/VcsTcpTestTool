/**
 * @file simple_message_server.c
 * Verteilte Systeme
 * TCP/IP Uebung
 * 
 * Server
 *
 * @author Juergen Schoener <ic16b049@technikum-wien.at>
 * @author Juergen Spandl <ic16b029@technikum-wien.at>
 * @date 2016/12/14
 *
 * @version 1
 *
 */

/*
 * -------------------------------------------------------------- includes --
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <limits.h>
#include <stdarg.h>
#include <getopt.h>

/*
 * --------------------------------------------------------------- defines --
 */

#define BL_NAME "simple_message_server_logic"
#define BL_PATH "/usr/local/bin/simple_message_server_logic"
#define UNUSED(x) (void)(x)

/*
 * --------------------------------------------------------------- globals --
 */

//programm arguments
static const char *sprogram_arg0 = NULL;

/*
 * ------------------------------------------------------------- functions --
 */

void print_usage(void);
void print_err(const char *fmt, ...);
void parse_commandline(int argc, const char *argv[], long *port);
int create_socket(long port);
int create_new_child(int sockfd);
int register_handler(void);
void sigchld_handler(int s);

/**
 *
 * \brief Main Program logic
 *
 * Main Entry Point. Parses The Command Line. Creates a socket and listens in a a loop for new childs.
 *
 * \param argc the number of arguments
 * \param argv the arguments
 *
 * \return always "success"
 * \retval 0 always
 *
 */
int main(int argc, const char *argv[])
{
    int socketfd;
    long port = -1;

    //Set Filename
    sprogram_arg0 = argv[0];

    //Parse Commandline arguments
    parse_commandline(argc, argv, &port);
   
   //Create Listening socket
    socketfd = create_socket(port);


    //Register Handler to reap all dear processes
    if (register_handler() < 0)
    {
        close(socketfd);
        print_err("Could not register Handler\n");
        exit(EXIT_FAILURE);
    }

    //Loop and accept new connections
    while (1)
    {
        if (create_new_child(socketfd) < 0)
            break;
    }
    
    close(socketfd);

    return 0;
}

/**
 *
 * \brief parses command line arguments
 *
 * Reads and parses the command line argument. Prints Usage if invalid command arguments are found
 *
 * \param argc number of arguments
 * \param argv the arguments
 * \param port the port, the server should listen for new connnections
 *
 * \return void
 * \retval void
 *
 */

void parse_commandline(int argc, const char *argv[], long *port)
{
    int c;
    char *strtol_end; //for checking several return values

    while ((c = getopt(argc, (char **const)argv, "p:h")) != -1)
    {
        *port = 1;
        switch (c)
        {
        case 'p':
            *port = strtol(optarg, &strtol_end, 10);
            if (optarg == strtol_end)
            {
                print_err("No digits parsed\n");
                print_usage();
            }
            else if (*port < 0 || *port > 65535) //strtol returns long LONG_MAX OR LONG_MIN when out of range
            {
                print_err("Port Out of Range\n");
                print_usage();
            }
            else if (*strtol_end != '\0')
            {
                print_err("Argument invalid\n");
                print_usage();
            }
            break;
        case 'h':
        case '?':
        default:
            print_usage();
            break;
        }
    }
    if (*port == -1)
    {
        print_err("Mandatory Option Port is missing");
        print_usage();
    }
}

/**
 *
 * \brief prints the usage
 *
 * Prints the usage for the simple_message_server. Terminates the program.
 *
 * \return void
 * \retval void
 *
 */

void print_usage()
{
    if (fprintf(stdout, "Usage:\nsimple_message_server -p port [-h]\n") < 0)
    {
        print_err("Could not print usage");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

/**
 *
 * \brief Creates the Connect Socket File Descriptor
 *
 * Creates the Socket File Descriptor. Sets the option to reuse local adresses(SO_REUSEADDR).
 * Binds and starts listening on the first available adresse return from getaddrinfo
 *
 * \param port The Port the socket should be opend on
 *
 * \return returns successful bound socket file Descriptor, Exits on Failure
 * \retval SUCCESS Connect Socket File Descriptor
 *
 */

int create_socket(long port)
{

    struct addrinfo hints, *res, *p;
    int sockfd;

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // fill in my IP for me

    //stuff long port into char array to pass into getaddrinfo
    char cport[63];
    snprintf(cport, 63, "%ld", port);

    int s;
    //get the addrinfo list for my configuration
    if ((s = getaddrinfo(NULL, cport, &hints, &res)) != 0)
    {
        print_err("getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }
    //loop adressinfo until successful bind
    for (p = res; p != NULL; p = p->ai_next)
    {
        //create socket
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("socket");
            continue;
        }

        //Set Reuse of Local adresses (SO_REUSEADDR)
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        {
            close(sockfd);
            print_err("Set Reuse of Local adresses (SO_REUSEADDR) failed\n");
            continue;
        }

        //Bind socket
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("bind");
            continue;
        }

        //Start listening on the socket
        if (listen(sockfd, 100) < 0)
        {
            print_err("listening on the bound socket failed.\n");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        break; // if we get here, we must have connected successfully
    }
    freeaddrinfo(res);

    if (p == NULL)
    {
        // looped off the end of the list with no successful bind
        print_err("failed to bind socket\n");
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

/**
 *
 * \brief Accepts incoming requests and creates the business logic in a new fork
 *
 * Accepts incoming requests for the listening socket. Then tries to fork the process.
 * The parent thread always returns.
 * The newly created fork points stdin and stdout to the connected socket fd and then executes the Businesslogic.
 * Busineslogic is defined in Macros (BL_PATH, BL_NAME). The child forks never returns
 *
 * \param sockfd The Listening socket File Descriptor
 *
 * \return Parent process returns. Child processes never return
 * \retval 0 fork successful created
 * \retval -1 fork could not be created
 *
 */

int create_new_child(int sockfd)
{
    struct sockaddr_storage addr_inf;
    socklen_t len = sizeof(addr_inf);
    int confd, pid;

    /* wait for incoming requests */
    if ((confd = accept(sockfd, (struct sockaddr *)&addr_inf, &len)) < 0)
    {
        print_err("Accepting new Client failed\n");
        return 0;
    }

    printf("Client accepted\n");

    /* fork process */
    if ((pid = fork()) < 0)
    {
        print_err("Forking new Client failed\n");
        close(confd);
        return -1;
    }
    //When pid -> newly created child
    if (pid == 0)
    {

        /* point stdin and stdout to newly connected socket */
        if ((dup2(confd, STDIN_FILENO) == -1) || (dup2(confd, STDOUT_FILENO) == -1))
        {
            print_err("Dupping stdin and stdout failed.\n");
            close(confd);
            exit(EXIT_FAILURE);
        }

        //Close listening socket for forked process
        if (close(sockfd) != 0)
        {
            print_err("Forked process could not close listening Socket.\n");
            close(confd);
            exit(EXIT_FAILURE);
        }

        // Close Connect Socket for forked process
        if (close(confd) != 0)
        {
            print_err("Forked process could not close connect socket.\n");
            exit(EXIT_FAILURE);
        }

        //Replace Forked Process with business logic
        if (execl(BL_PATH, BL_NAME, NULL) < 0)
        {
            print_err("Could not start server business logic.\n");
            exit(EXIT_FAILURE);
        }
        exit(EXIT_FAILURE);
    }
    else //pid > 0 -> parent
    {
        close(confd);
        return 0;
    }

    return -1;
}

/**
 *
 * \brief Prints error messages tostderr stdout
 *
 * Prints error message with arguments to stdout
 *
 * \param fmt Error message
 *
 */

void print_err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    /* print filename */
    fprintf(stderr, "%s: ", sprogram_arg0);
    /* print error message with arguments */
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

/**
 *
 * \brief Waits for all child processes, that are zombies, to be reaped
 *
 * Waits for all child processes, that are zombies, to be reaped
 *
 * \param s sigaction (UNUSED)
 *
 */

void sigchld_handler(int s)
{
    UNUSED(s);
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    errno = saved_errno;
}

/**
 *
 * \brief Registers the handler to reap dead processes
 *
 * Registers the handler to reap dead processes
 *
 * \return SUCCESS OR Failure
 * \retval 0 successful
 * \retval -1 Failure
 **/

int register_handler()
{
    struct sigaction sa;
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        return -1;
    }
    return 0;
}