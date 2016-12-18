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


#define BL_NAME "simple_message_server_logic"
#define BL_PATH "/usr/local/bin/simple_message_server_logic"
#define UNUSED(x) (void)(x)

void print_usage(void);
void print_err(const char *fmt, ...);
void parse_commandline(int argc, const char *argv[], long *port);
int create_socket(long port);
int create_new_child(int sockfd);
int register_handler(void);
void sigchld_handler(int s);

/**
 *
 * \brief The most minimalistic C program
 *
 * This is the main entry point for any C program.
 *
 * \param argc the number of arguments
 * \param argv the arguments itselves (including the program name in argv[0])
 *
 * \return always "success"
 * \retval 0 always
 *
 */
int main(int argc, const char *argv[])
{
    int socketfd;

    long port = -1;

    parse_commandline(argc, argv, &port);


   socketfd = create_socket(port);

if(register_handler()<0){
     close(socketfd);
     print_err("Could not register Handler\n");
     exit(EXIT_FAILURE);
}




    while (1)
    {
        if(create_new_child(socketfd)<0)
            break;

             printf("loopy");
    }
    close(socketfd);

     return 0;
}

void parse_commandline(int argc, const char *argv[], long *port)
{
    int c;
    char *strtol_end; //for checking several return values

    while ((c = getopt(argc, (char **const)argv, "p:h")) != -1)
    {*port =1;
        switch (c)
        {
        case 'p':
            *port = strtol(optarg, &strtol_end, 10);
            if (optarg == strtol_end)
            {
                print_err("No digits parsed\n");
                print_usage();
            }
            else if (*port < 0 || *port > 65535)
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

void print_usage()
{
    printf("Usage: -p port [-h]\n");
    exit(EXIT_SUCCESS);
}

int create_socket(long port)
{

    struct addrinfo hints, *res,*p;
    int sockfd;

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // fill in my IP for me
    

char cport[63];
snprintf(cport, 63, "%ld", port);

int s;
     if ((s=getaddrinfo(NULL, cport, &hints, &res))!= 0) {
               print_err("getaddrinfo: %s\n", gai_strerror(s));
               exit(EXIT_FAILURE);
           }

for(p = res; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
        perror("socket");
        continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0){
        close(sockfd);
        print_err("setsockopt(SO_REUSEADDR) failed\n");
        continue;
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        close(sockfd);
        perror("bind");
        continue;
    }

    if (listen(sockfd, 100) < 0)
    {
        print_err("listen() failed.");
        close(sockfd);
        return -1;
    }

    break; // if we get here, we must have connected successfully
}
freeaddrinfo(res);

if (p == NULL) {
    // looped off the end of the list with no successful bind
    print_err("failed to bind socket\n");
    exit(EXIT_FAILURE);
}

return sockfd;
}

int create_new_child(int sockfd)
{
    struct sockaddr_storage addr_inf;
    socklen_t len = sizeof(addr_inf);
    int confd,pid;

  
        if ((confd = accept(sockfd, (struct sockaddr*) &addr_inf, &len)) < 0)
        {
            print_err("Accepting new Client failed\n");
            return 0;
        }

        printf("Client accepted\n");

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
            //assert(0); 
            exit(EXIT_FAILURE);
        }      
        else //pid > 0 -> parent
        {
            close(confd);
            return 0;
        }
    
    return -1;
}

void print_err(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

void sigchld_handler(int s)
{  UNUSED(s);
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

int register_handler(){
    struct sigaction sa;   
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return -1;
    }
    return 0;
}