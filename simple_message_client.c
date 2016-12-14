/**
 * @file simple_message_client.c
 * Verteilte Systeme
 * TCP/IP Uebung
 * 
 * Client
 *
 * @author Juergen Schoener <ic16b049@technikum-wien.at>
 * @author Juergen Spandl <ic16b029@technikum-wien.at>
 * @date 2016/12/10
 *
 * @version 1
 *
 */

/*
 * -------------------------------------------------------------- includes --
 */

#include <stdio.h>



#include <simple_message_client_commandline_handling.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <string.h> /* memset */
#include <stdlib.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <errno.h>

#include <unistd.h>

#include <stdarg.h>     /* va_list, va_start, va_arg, va_end */


/*
 * --------------------------------------------------------------- defines --
 */

/*
 * -------------------------------------------------------------- typedefs --
 */

/*
 * --------------------------------------------------------------- globals --
 */
/*
 * --------------------------------------------------------------- static --
 */
/** Controls the verbose output. */
static int verbose = 0;

/** Current program arguments. */
static const char* sprogram_arg0 = NULL;

/*
 * ------------------------------------------------------------- functions --
 */
static void usage(FILE* stream, const char* cmnd, int exitcode);

static int sendall(int s, char *buf, int *len);
static int recv_all(int socket_fd, void *buf, size_t len, int flags);
static void verbose_printf(int verbosity, const char *format, ...);

static int send_request(int socket_fd, const char *user, const char *message, const char *img_url);

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
    int socket_fd;
    
    const char* server;
    const char* port;
    const char* user;
    const char* message;
    const char* image_url;

    int status;     //for checking several return values

    sprogram_arg0 = argv[0];

    smc_parsecommandline(argc, argv, usage, &server, &port, &user, &message, &image_url, &verbose);
    verbose_printf(verbose, "[%s, %s(), line %d]: Using the following options: server=\"%s\" port=\"%s\", user=\"%s\", img_url=\"%s\", message=\"%s\"\n", __FILE__, __func__, __LINE__, server, port, user, image_url, message);
    
    //gerraddrinfo() stuff START
    struct addrinfo hints;
    struct addrinfo *servinfo, *loop_serverinfo;  // will point to the results

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    
    if ((status = getaddrinfo(server, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "%s: Could not obtain address information: %s\n", __FILE__, gai_strerror(status));
        return EXIT_FAILURE;
    }  
    

    //get ip or ipv6 addr
    for(loop_serverinfo = servinfo; loop_serverinfo != NULL; loop_serverinfo = loop_serverinfo->ai_next) {
        //@todo: correct verbose message
        verbose_printf(verbose, "[%s, %s(), line %d]: Obtained IPv%d address %s, port number %s for server %s and port %s\n", __FILE__, __func__, __LINE__, loop_serverinfo->ai_protocol, loop_serverinfo->ai_addr, port, loop_serverinfo->ai_canonname, port);
    
        socket_fd = socket(loop_serverinfo->ai_family, loop_serverinfo->ai_socktype, loop_serverinfo->ai_protocol);
        
        if(socket_fd == -1){
            //@todo: correct verbose message, mabye add errno???
            //fprintf(stderr, "%s: Could not create socket file descriptor", __FILE__);
            //return EXIT_FAILURE;
            verbose_printf(verbose, "%s: ", __FILE__, strerror(errno));
            continue;
        }
        //@todo: correct verbose message
        verbose_printf(verbose, "[%s, %s(), line %d]: Created IPvX XXXSOCK_STREAMXXX socket\n", __FILE__, __func__, __LINE__);

        // connect!

        if(connect(socket_fd, loop_serverinfo->ai_addr, loop_serverinfo->ai_addrlen) != -1){
            //success message
            //@todo: correct verbose message
            verbose_printf(verbose, "[%s, %s(), line %d]: Connected to port %s (%s) of server %s (%s)\n", __FILE__, __func__, __LINE__, port, port, loop_serverinfo->ai_canonname,loop_serverinfo->ai_addr );
            break; //success
        }
        
        //could not connect
        //@todo: correct verbose string!
        verbose_printf(verbose, "%s: ", __FILE__, strerror(errno));
        close(socket_fd);
    }

    freeaddrinfo(servinfo); // free the linked-list, no longer needed

    if(loop_serverinfo == NULL){
        fprintf(stderr, "%s: Could not connect\n", __FILE__);
        close(socket_fd);
        return EXIT_FAILURE;
        //freeaddrinfo(servinfo); // free the linked-list, is it necessary?
    }
      
    
    if(send_request(socket_fd, user, message, image_url) == -1){
        fprintf(stderr, "%s: Error when writing to socket\n", __FILE__);
        close(socket_fd);
        return EXIT_FAILURE;
    }
   
    //shutdown writing
    if(shutdown(socket_fd, 1)){
        fprintf(stderr, "%s: Error when shutting down socket for writing\n", __FILE__);
        close(socket_fd);
        return EXIT_FAILURE;
    }
    verbose_printf(verbose, "[%s, %s(), line %d]: Closed write part of socket\n", __FILE__, __func__, __LINE__);

    
    //@todo: was soll getan werden wenn der buffer zu klein ist??? das muss noch geaendert werden
    char buf[10000];
    
    //receive all stuff
    recv_all(socket_fd, buf, 10000 - 1, 0);    
    
    char *recv_file_name_html = NULL, *recv_img_name = NULL;
    long html_status = 0, html_len = 0, img_len = 0;
    
    const char *records[6] = { "status=", "file=", "len=", "file=", "len=" };
    
    
    char * pch;

    //set pch to the status
    pch = strstr(buf, records[0]);  
    html_status = strtol(pch, NULL, 10);
      
    //set pch to the html filename
    pch = strstr(pch, records[1]);
    pch = strchr(pch, '=');
    pch++;
    
    
    recv_file_name_html = (char*)malloc((strchr(pch,'\n')-pch+1));
    if(!recv_file_name_html){
        free(recv_file_name_html);
        close(socket_fd); 
        fprintf(stderr, "%s: malloc() for html file name failed.\n", __FILE__);
        return EXIT_FAILURE;
    }
    
    strncpy(recv_file_name_html, pch, (strchr(pch,'\n')-pch+1));
    recv_file_name_html[(strchr(pch,'\n')-pch)] = '\0'; //do we need this?
    
    //set pch to the html file length
    pch = strstr(pch, records[2]);
    pch = strchr(pch, '=');
    pch++;
      
    html_len = strtol(pch, NULL, 10);
        
        
    //set pch to the html data
    pch = strchr(pch, '\n');
    pch++;
    
    FILE *f;
    
    f = fopen(recv_file_name_html, "w+");
    
    if (f == NULL)
    {
        free(recv_file_name_html);
        close(socket_fd); 
        fprintf(stderr, "%s: Opening html file failed.\n", __FILE__);
        return EXIT_FAILURE;
    }
    
    int bytes_written= 0;
    bytes_written = fwrite(pch, sizeof(char), html_len, f);
        
    if(bytes_written != html_len)
    {
        free(recv_file_name_html);
        close(socket_fd); 
        fprintf(stderr, "%s: Writing to html file failed.\n", __FILE__);
        return EXIT_FAILURE;
    }
        
    free(recv_file_name_html);
    fclose(f);
        
    //set pch to image name
    pch = strstr(pch, records[3]);
    pch = strchr(pch, '=');
    pch++;

    
    recv_img_name = malloc(sizeof(char) * (strchr(pch,'\n')-pch+1));

    if(!recv_img_name){
        free(recv_img_name);
        close(socket_fd); 
        fprintf(stderr, "%s: malloc() for image name failed.\n", __FILE__);
        return EXIT_FAILURE;
    }
    
    strncpy(recv_img_name, pch, (strchr(pch,'\n')-pch)+1);
    recv_img_name[(strchr(pch,'\n')-pch)] = '\0'; //do we need this?
        
        
    //set pch to image length
    pch = strstr(pch, records[4]);
    pch = strchr(pch, '=');
    pch++;
        
    img_len = strtol(pch, NULL, 10);

    //set pch to image
    pch = strchr(pch, '\n');
    pch++;
        
    f = fopen(recv_img_name, "w+");
    if (f == NULL)
    {        
        free(recv_img_name);
        close(socket_fd); 
        fprintf(stderr, "%s: Opening png file failed.\n", __FILE__);
        return EXIT_FAILURE;        
    }

    bytes_written = fwrite(pch, sizeof(char), img_len, f);
        
    if(bytes_written != img_len)
    {
        free(recv_img_name);
        close(socket_fd); 
        fprintf(stderr, "%s: Writing to png file failed.\n", __FILE__);
        return EXIT_FAILURE;
    }
        
    free(recv_img_name);
    fclose(f);
    close(socket_fd);

    return  0;
}


static void usage(FILE *stream, const char *cmnd, int exitcode) {
    if(fprintf(stream, "usage: %s options \n\
        options:\n\
        -s, --server <server>   full qualified domain name or IP address of the server\n\
        -p, --port <port>       well-known port of the server [0..65535]\n\
        -u, --user <name>       name of the posting user\n\
        -i, --image <URL>       URL pointing to an image of the posting user\n\
        -m, --message <message> message to be added to the bulletin board\n\
        -v, --verbose           verbose output\n\
        -h, --help\n", cmnd) < 0){
                
        //PRERROR("can't print to stdout");
    }
    exit(exitcode);
}


static int send_request(int socket_fd, const char *user, const char *message, const char *img_url){
    int len;
    char* conc_message;
    //printf("%d\n", len);

    
    /* calculate message size */
    if (img_url) {
	//with img_url
        len = strlen("user=") + strlen(user) + strlen("\nimg=") + strlen(img_url) + strlen("\n") + strlen(message);
    }else {
	//no img_url
	len = strlen("user=") + strlen(user) + strlen("\n") + strlen(message);
    }
    
    conc_message = (char*) malloc(len+1);
    
    if (conc_message == NULL) {
	//printErrorMessage("client: malloc message size");
        //free(conc_message);
	return EXIT_FAILURE;
    }
    
    /* assemble message */
    if (img_url == NULL) {
	/* without image */
	sprintf(conc_message, "user=%s\n%s", user, message);
    } 
    else {
	/* with image */
	sprintf(conc_message, "user=%s\nimg=%s\n%s", user, img_url, message);
    }
	
    verbose_printf(verbose, "[%s, %s(), line %d]: Going to send the following message consisting of %d bytes ...\n%s\n", __FILE__, __func__, __LINE__, len, conc_message);
    
    if (sendall(socket_fd, conc_message, &len) == -1) {
        //printf("We only sent %d bytes because of the error!\n", len);
        free(conc_message);
        return -1;
    }
    //@todo: correct verbose output
    verbose_printf(verbose, "[%s, %s(), line %d]: Sent request user=\"%s\", img_url=\"%s\", message=\"%s\" to server %s (%s)\n", __FILE__, __func__, __LINE__, user, img_url, message, "HOSTNAME", "IPIPIPIPIP");
    
    free(conc_message);
    
    return 0;
}

static int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
} 

static int recv_all(int socket_fd, void *buf, size_t len, int flags)
{
    size_t toread = len;
    char  *bufptr = (char*) buf;

    while (toread > 0)
    {
        ssize_t rsz = recv(socket_fd, bufptr, toread, flags);
        if (rsz <= 0)
            return rsz;  /* Error or other end closed cnnection */

        toread -= rsz;  /* Read less next time */
        bufptr += rsz;  /* Next buffer position to read into */
    }

    return len;
}


static void verbose_printf(int verbosity, const char *format, ...)
{

    // va_list is a special type that allows hanlding of variable
    // length parameter list
    va_list args;
    va_start(args, format);

    // If verbosity flag is on then print it
    if (verbosity){
        vfprintf (stdout, format, args);
    } else{
        // Do nothing

    }
}

/*
 * =================================================================== eof ==
 */

