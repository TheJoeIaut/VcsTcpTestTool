/**
 * @file simple_message_client.c
 * 
 * Client
 *
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
#include <limits.h>     //needed for LONG_MIN and LONG_MAX


/*
 * --------------------------------------------------------------- defines --
 */
#define MAX_CHUNK_SIZE 256

/*
 * -------------------------------------------------------------- typedefs --
 */

/*
 * --------------------------------------------------------------- globals --
 */
/*
 * --------------------------------------------------------------- static --
 */
//indicates the verbose output
static int verbose = 0;

//programm arguments
static const char* sprogram_arg0 = NULL;

/*
 * ------------------------------------------------------------- functions --
 */
static void usage(FILE* stream, const char* cmnd, int exitcode);
static int sendall(int s, char *buf, int *len);
static void verbose_printf(int verbosity, const char *format, ...);
static int send_request(int socket_fd, const char *user, const char *message, const char *img_url);
static int write_file(char* recv_file_name_html, FILE *recv_fd, int html_len);

/**
 * \brief This is the main entry point for any C program.
 *
 * \param argc the number of arguments
 * \param argv the arguments itselves (including the program name in argv[0])
 *
 * \return returns success or error
 * \retval EXIT_SUCCESS returned on success
 * \retval EXIT_FAILURE returned on error
 */
int main(int argc, const char *argv[])
{
    int socket_fd;
    int status;     //for checking several return values
    
    const char* server;
    const char* port;
    const char* user;
    const char* message;
    const char* image_url;

    sprogram_arg0 = argv[0];

    smc_parsecommandline(argc, argv, usage, &server, &port, &user, &message, &image_url, &verbose);
    verbose_printf(verbose, "[%s, %s(), line %d]: Using the following options: server=\"%s\" port=\"%s\", user=\"%s\", img_url=\"%s\", message=\"%s\"\n", __FILE__, __func__, __LINE__, server, port, user, image_url, message);
    
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct addrinfo *loop_serverinfo;  // will point to the results

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    
    if ((status = getaddrinfo(server, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "%s: Could not obtain address information: %s\n", sprogram_arg0, gai_strerror(status));
        return EXIT_FAILURE;
    }  
    
    //get ip or ipv6 addr
    for(loop_serverinfo = servinfo; loop_serverinfo != NULL; loop_serverinfo = loop_serverinfo->ai_next) {
        //@todo: correct verbose message
        verbose_printf(verbose, "[%s, %s(), line %d]: Obtained IPv%d address %s, port number %s for server %s and port %s\n", __FILE__, __func__, __LINE__, loop_serverinfo->ai_protocol, loop_serverinfo->ai_addr, port, loop_serverinfo->ai_canonname, port);
    
        socket_fd = socket(loop_serverinfo->ai_family, loop_serverinfo->ai_socktype, loop_serverinfo->ai_protocol);
        
        if(socket_fd == -1){
            verbose_printf(verbose, "[%s, %s(), line %d]: %s\n", __FILE__, __func__, __LINE__, strerror(errno));
	    continue;
        }
        //@todo: correct verbose message
        verbose_printf(verbose, "[%s, %s(), line %d]: Created IPvX XXXSOCK_STREAMXXX socket\n", __FILE__, __func__, __LINE__);

        if(connect(socket_fd, loop_serverinfo->ai_addr, loop_serverinfo->ai_addrlen) < 0){
            verbose_printf(verbose, "[%s, %s(), line %d]: %s\n", __FILE__, __func__, __LINE__,strerror(errno));
            close(socket_fd);
            continue;
        }
        
        verbose_printf(verbose, "[%s, %s(), line %d]: Connected to port %s (%s) of server %s (%s)\n", __FILE__, __func__, __LINE__, port, port, loop_serverinfo->ai_canonname,loop_serverinfo->ai_addr );
        break; //success
    }

    freeaddrinfo(servinfo); // free the linked-list, no longer needed

    if(loop_serverinfo == NULL){
        fprintf(stderr, "%s: Could not connect\n", sprogram_arg0);
	//socket is not open here, so do not close it
        return EXIT_FAILURE;
    }
      
    if(send_request(socket_fd, user, message, image_url) == -1){
        fprintf(stderr, "%s: Error when writing to socket\n", sprogram_arg0);
        close(socket_fd);
        return EXIT_FAILURE;
    }
   
    //shutdown writing, 1 -> further sends are disallowed
    if(shutdown(socket_fd, 1)){
        fprintf(stderr, "%s: Error when shutting down socket for writing: %s\n", sprogram_arg0, strerror(errno));
        close(socket_fd);
        return EXIT_FAILURE;
    }
    verbose_printf(verbose, "[%s, %s(), line %d]: Closed write part of socket\n", __FILE__, __func__, __LINE__);
    
    
    
    //READ FROM HERE________________________________________________________________________________________________________
    

    char* line = NULL, *pch = NULL;
    char *recv_file_name_html = NULL, *recv_img_name = NULL;
    size_t allocated_size;
    
    long html_len = 0, img_len = 0;
    
    FILE *recv_fd;
    recv_fd = fdopen(socket_fd,"r");
    
    if (recv_fd == NULL) {
       fprintf(stderr, "%s: Error when shutting down socket for writing: %s\n", sprogram_arg0, strerror(errno));
       close(socket_fd); 
       return EXIT_FAILURE;
    }
    
    //get status=...
    if(getline(&line, &allocated_size, recv_fd) == -1){
        fprintf(stderr, "%s: Error when getting line for \"status\"\n", sprogram_arg0);
        fclose(recv_fd);
	close(socket_fd); 
	free(line);
        return EXIT_FAILURE;
    }
    //wozu dient der status?
    //todo: implementiere status handling
    
    verbose_printf(verbose, "[%s, %s(), line %d]: Obtained response \"%s\" from server.\n", __FILE__, __func__, __LINE__, line);
    //todo: wellformed server response...
    verbose_printf(verbose, "[%s, %s(), line %d]: Processed status of server response.\n", __FILE__, __func__, __LINE__);
    
    
    //get file=...
    if(getline(&line, &allocated_size, recv_fd) == -1){
        fprintf(stderr, "%s: Error when getting line for \"file\"\n", sprogram_arg0);
        fclose(recv_fd);
	close(socket_fd);         
        free(line);
        return EXIT_FAILURE;
    }
    
    //set pch to the html filename
    pch = strstr(line, "file=");

    if((pch == NULL)){
        fprintf(stderr, "%s: Could not find \"file=\".\n", sprogram_arg0);        
        fclose(recv_fd);
	close(socket_fd);
        free(line);
        return EXIT_FAILURE;        
    }
    verbose_printf(verbose, "[%s, %s(), line %d]: Obtained response \"%s\" from server\n", __FILE__, __func__, __LINE__, line);
    
    
    pch = pch + strlen("file=");     //point to filename

    recv_file_name_html = (char*)malloc((strchr(pch,'\n')-pch+1));
    if(recv_file_name_html == NULL){
        fprintf(stderr, "%s: malloc() for html file name failed.\n", sprogram_arg0);        
        fclose(recv_fd);
	close(socket_fd); 
        free(line);
        return EXIT_FAILURE;
    }   
    
    strncpy(recv_file_name_html, pch, (strchr(pch,'\n')-pch+1));
    recv_file_name_html[(strchr(pch,'\n')-pch)] = '\0'; //do we need this?    
    
    verbose_printf(verbose, "[%s, %s(), line %d]: Wellformed server response \"%s\".\n", __FILE__, __func__, __LINE__, recv_file_name_html);
    
    //get len=...
    if(getline(&line, &allocated_size, recv_fd) == -1){
        fprintf(stderr, "%s: Error when getting line for \"len=\"\n", sprogram_arg0);
        fclose(recv_fd);
	close(socket_fd); 
        free(line);
        free(recv_file_name_html);
        return EXIT_FAILURE;
    }
    
    //set pch to the html file length
    pch = strstr(line, "len=");
    if((pch == NULL)){
        fprintf(stderr, "%s: Could not find \"len=\".\n", sprogram_arg0);
        fclose(recv_fd);
	close(socket_fd); 
        free(line);
        free(recv_file_name_html);
        return EXIT_FAILURE;        
    }
    
    verbose_printf(verbose, "[%s, %s(), line %d]: Obtained response \"%s\" from server\n", __FILE__, __func__, __LINE__, line);
    

    pch = pch + strlen("len=");     //point to length
    
    html_len = strtol(pch, NULL, 10);
    if((html_len == LONG_MIN || html_len == LONG_MAX) && errno == ERANGE){
        fprintf(stderr, "%s: Could not convert html \"len=\" to long.\n", sprogram_arg0);
        fclose(recv_fd);
	close(socket_fd);
        free(line);
        free(recv_file_name_html);
        return EXIT_FAILURE;          
    }    
    
    verbose_printf(verbose, "[%s, %s(), line %d]: Wellformed server response \"%d\".\n", __FILE__, __func__, __LINE__, html_len);
    
    //todo: error handling
    if(write_file(recv_file_name_html, recv_fd, html_len) == EXIT_FAILURE){
        fprintf(stderr, "%s: Could not write html file.\n", sprogram_arg0);
        fclose(recv_fd);
	close(socket_fd);
        free(line);
        free(recv_file_name_html);
    }
    free(recv_file_name_html);
    
    //set pointer after html file???
    
    //get file=...
    
    //set pch to the img filename
    if(getline(&line, &allocated_size, recv_fd) == -1){
        fprintf(stderr, "%s: Error when getting line for \"len=\"\n", sprogram_arg0);
        fclose(recv_fd);
	close(socket_fd);
        free(line);
        return EXIT_FAILURE;
    }    
    
    pch = strstr(line, "file=");

    if((pch == NULL)){
        fprintf(stderr, "%s: Could not find \"file=\".\n", sprogram_arg0);        
        fclose(recv_fd);
	close(socket_fd);
        free(line);
        return EXIT_FAILURE;        
    }

    pch = pch + strlen("file=");     //point to filename


    recv_img_name = (char*)malloc((strchr(pch,'\n')-pch+1));
    if(recv_img_name == NULL){
        fprintf(stderr, "%s: malloc() for html file name failed.\n", sprogram_arg0);        
        fclose(recv_fd);
	close(socket_fd);
        free(line);
        return EXIT_FAILURE;
    }   
    
    strncpy(recv_img_name, pch, (strchr(pch,'\n')-pch+1));
    recv_img_name[(strchr(pch,'\n')-pch)] = '\0'; //do we need this?    
    
    
    //get len=...
    if(getline(&line, &allocated_size, recv_fd) == -1){
        fprintf(stderr, "%s: Error when getting line for \"len=\"\n", sprogram_arg0);
        fclose(recv_fd);
	close(socket_fd);         
        free(line);
        free(recv_img_name);
        return EXIT_FAILURE;
    }        
    
    
    //set pch to the html file length
    pch = strstr(line, "len=");
    if((pch == NULL)){
        fprintf(stderr, "%s: Could not find \"len=\".\n", sprogram_arg0);
        fclose(recv_fd);
	close(socket_fd);
        free(line);
        free(recv_img_name);
        return EXIT_FAILURE;        
    }    
    
    pch = pch + strlen("len=");     //point to length
    
    img_len = strtol(pch, NULL, 10);
    if((img_len == LONG_MIN || img_len == LONG_MAX) && errno == ERANGE){
        fprintf(stderr, "%s: Could not convert html \"len=\" to long.\n", sprogram_arg0);
        fclose(recv_fd);
	close(socket_fd); 
        free(line);
        free(recv_img_name);
        return EXIT_FAILURE;          
    }   
    
    
    //wieso kann hier nicht weitergelesen werden???? vllt passt &length nicht???
    if(write_file(recv_img_name, recv_fd, img_len) == EXIT_FAILURE){
        fprintf(stderr, "%s: Could not write image.\n", sprogram_arg0);
        fclose(recv_fd);
	close(socket_fd); 
        free(line);
        free(recv_img_name);
    }

    fclose(recv_fd);
    close(socket_fd);
    free(line);
    free(recv_img_name);

    return  0;
}

/**
 *
 * \brief prints the usage messagei and terminates the process. used by smc_parsecommandline().
 *
 * \param stream stream to write the message to
 * \param cmnd message to print
 * \param exitcode indicates successfull or unsuccessfull termination
 *
 * \return void
 *
 */


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
        
        fprintf(stderr, "%s: Writing to stdout failed.\n", sprogram_arg0);
    }
    exit(exitcode);
}


/**
 *
 * \brief prepares and sends a request
 *
 * assembles the message for the request depending on the passed parameters
 * and writes it to the passed socked_fd. 
 *
 * \param socket_fd
 * \param user user to send
 * \param message message to send
 * \param img_url the url of the image. this can be null.
 *
 * \return returns success or error
 * \retval EXIT_SUCCESS returned on success
 * \retval EXIT_FAILURE returned on error
 *
 */

static int send_request(int socket_fd, const char *user, const char *message, const char *img_url){
    int len;            //length of the message to send
    char* conc_message; //message to send
    
    // calculate message size
    if (img_url) {
	//with img_url
        len = strlen("user=") + strlen(user) + strlen("\nimg=") + strlen(img_url) + strlen("\n") + strlen(message);
    }else {
	//no img_url
	len = strlen("user=") + strlen(user) + strlen("\n") + strlen(message);
    }
    
    conc_message = (char*) malloc(len+1);
    
    if (conc_message == NULL) {
        fprintf(stderr, "%s: malloc() for message to send failed.\n", sprogram_arg0);
	return EXIT_FAILURE;
    }
    
    //build message
    if (img_url == NULL) {
	//no image
	if(sprintf(conc_message, "user=%s\n%s", user, message) < 0){
            fprintf(stderr, "%s: Writing message failed.\n", sprogram_arg0);
            free(conc_message);
            return EXIT_FAILURE;
        }
    } 
    else {
	// with image
	if(sprintf(conc_message, "user=%s\nimg=%s\n%s", user, img_url, message) < 0){
            fprintf(stderr, "%s: Writing message failed.\n", sprogram_arg0);
            free(conc_message);
            return EXIT_FAILURE;
        }
    }
	
    verbose_printf(verbose, "[%s, %s(), line %d]: Going to send the following message consisting of %d bytes ...\n%s\n", __FILE__, __func__, __LINE__, len, conc_message);
    
    if (sendall(socket_fd, conc_message, &len) == -1) {
        fprintf(stderr, "%s: Writing message failed.\n", __FILE__);
        free(conc_message);
        return EXIT_FAILURE;
    }
    //@todo: correct verbose output
    verbose_printf(verbose, "[%s, %s(), line %d]: Sent request user=\"%s\", img_url=\"%s\", message=\"%s\" to server %s (%s)\n", __FILE__, __func__, __LINE__, user, img_url, message, "HOSTNAME", "IPIPIPIPIP");
    
    free(conc_message);
    
    return 0;
}

/**
 *
 * \brief writes all the passed data in the buffer to passed socket
 *
 * writes all the data from the buffer, so that there is nothing left.
 * this one is taken from beej's guide to network programming
 * https://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#sendall
 *
 * \param s socket to write the data to
 * \param buf data that has to be written
 * \param len number of bytes to write
 *
 * \return returns success or error
 * \retval -1 returned on error
 * \retval 0 retruned on success
 *
 */

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

/**
 *
 * \brief reads data from passed file descriptor and writes it to a file
 *
 * reads data from recv_fd in chunks and writes it to a file named recv_file_name_html
 * chunkwise.
 *
 * \param recv_file_name_html name of the file to write the data to
 * \param recv_fd file descriptor the data is read from
 * \param html_len expected length of the data to read and write
 *
 * \return void
 * \retval void
 *
 */

static int write_file(char* recv_file_name_html, FILE *recv_fd, int html_len){
    char buf[MAX_CHUNK_SIZE];
    FILE *f;
    int chunk_number = html_len / MAX_CHUNK_SIZE;
    int last_chunk = html_len - ( MAX_CHUNK_SIZE * chunk_number);
    
    verbose_printf(verbose, "[%s, %s(), line %d]: Opening file \"%s\" for writing of %d bytes in %d chucks @%d bytes and a last remainder chunk @%d bytes ...\n", __FILE__, __func__, __LINE__, recv_file_name_html, html_len, chunk_number, MAX_CHUNK_SIZE, last_chunk);
    
    f = fopen(recv_file_name_html, "w+");
    
    if (f == NULL)
    {
        fprintf(stderr, "%s: Opening html file failed.\n", sprogram_arg0);
        return EXIT_FAILURE;
    }
    
    verbose_printf(verbose, "[%s, %s(), line %d]: Opened file \"%s\" for writing of %d bytes in %d chucks @%d bytes and a last remainder chunk @%d bytes ...\n", __FILE__, __func__, __LINE__, recv_file_name_html, html_len, chunk_number, MAX_CHUNK_SIZE, last_chunk);
    
    int bytes_written= 0;
    int bytes_read = 0;
    
    int read_chunk_size = 0;
    int write_chunk_size = 0;
    
    for(int i = 0; i <= chunk_number; i++){
    //while(bytes_read != html_len && bytes_written != html_len){
        
        if(html_len - bytes_read > MAX_CHUNK_SIZE){ // >=????
            read_chunk_size = MAX_CHUNK_SIZE;
        }else{
            read_chunk_size = html_len - bytes_read;
        }
        
        read_chunk_size = fread(buf,sizeof(char),read_chunk_size, recv_fd);
        write_chunk_size = fwrite(buf, sizeof(char), read_chunk_size, f);
        
        if(read_chunk_size != write_chunk_size){
            fprintf(stderr, "%s: Writing html file failed.\n", sprogram_arg0);
            if(fclose(f)){
                fprintf(stderr, "%s: Closing html file failed: %s.\n", sprogram_arg0, strerror(errno));
                return EXIT_FAILURE;    
            }
            return EXIT_FAILURE;
        }
        
        bytes_read += read_chunk_size;
        bytes_written += write_chunk_size;

        verbose_printf(verbose, "[%s, %s(), line %d]: Copied chunk %d @%d bytes ...\n", __FILE__, __func__, __LINE__, i, write_chunk_size);
        
    }
    
    if(fclose(f)){
        fprintf(stderr, "%s: Closed file \"%s\"\n", sprogram_arg0, recv_file_name_html);
        return EXIT_FAILURE;    
    }
    
    return EXIT_SUCCESS;
}

/**
 *
 * \brief Makes the verbose output
 *
 * Checks if the verbose flag is set to 1 and makes an output of so.
 * Does nothing if it is set to 0.
 *
 * \param verbosity indicates if verbose flag is set
 * \param format todo: what does it???
 *
 * \return void
 * \retval void
 *
 */
static void verbose_printf(int verbosity, const char *format, ...)
{

    // va_list is a special type that allows hanlding of variable
    // length parameter list
    va_list args;
    va_start(args, format);

    // If verbosity flag is on then print it
    if (verbosity){
        fprintf(stdout, "%s ", sprogram_arg0); //todo:check for errors
        vfprintf(stdout, format, args); //todo: check for errors
    } else{
        // Do nothing

    }
}

/*
 * =================================================================== eof ==
 */

