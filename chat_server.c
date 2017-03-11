#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<signal.h>

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#define MAX_SIZE 8192
#define BACKLOG 3

int s_sock, c_sock; //file descriptors for sockets
    pa_simple *s = NULL;

/* signal handler */
void sig_handler(int signumber)
{
    if (signumber == SIGINT) {
        printf("\nreceived CTRL-C\n Terminating....\n");
        close(s_sock);
        close(c_sock);

        if (s)
            pa_simple_free(s);

        exit(0);
    }
}

int main(int argc, char* argv[])
{
    if(argc != 2){// usage : ./cs <port number>
        fprintf(stderr,"usage : cs port\n");
        exit(1);
    }

    int i, numbytes;
    struct sockaddr_in server, client;
    char message_buf[MAX_SIZE];

    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 2
    };

    int ret = 1;
    int error;

    /* create socket */
    if((s_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("server: socket");
        exit(1);
    }

    printf("Socket created\n");

    /* clear memory to avoid undefined behaviour */
    memset(&server, '0', sizeof(server));
    memset(&message_buf, '0', sizeof(message_buf));

    /* configure socket */
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(atoi(argv[1]));

    /* binding socket */
    if(bind(s_sock, (struct sockaddr *)&server, sizeof(server)) < 0){
        perror("bind");
        exit(1);
    }

    printf("bind done\n");

    if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        exit(1);
    }

    /* listening */
    listen(s_sock, BACKLOG);

    /*catch signal */
    if(signal(SIGINT, sig_handler) == SIG_ERR)
        printf("\n can't catch sigint\n");

    /* server waits for client */
    while(1){

        printf("Waiting for incoming connections...\n");
        i = sizeof(struct sockaddr_in);

        /* accept client connection */
        if((c_sock = accept(s_sock, (struct sockaddr *)&client, (socklen_t*)&i)) < 0){
            perror("accept failed");
            exit(1);
        }

        printf("Connection accepted\n");

        /* receive client messages */
        while((numbytes = recv(c_sock, message_buf, MAX_SIZE, 0)) > 0){
#if 0
            pa_usec_t latency;

            if ((latency = pa_simple_get_latency(s, &error)) == (pa_usec_t) -1) {
                fprintf(stderr, __FILE__": pa_simple_get_latency() failed: %s\n", pa_strerror(error));
                goto finish;
            }

            fprintf(stderr, "%0.0f usec    \r", (float)latency);
#endif
            /* ... and play it */
            if (pa_simple_write(s, message_buf, (size_t) numbytes, &error) < 0) {
                fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
            }

            memset(message_buf, '0', sizeof(message_buf));
        }

        if (numbytes == 0) {// if client disconnected wait for another.
            if (pa_simple_drain(s, &error) < 0) {
                fprintf(stderr, __FILE__": pa_simple_drain() failed: %s\n", pa_strerror(error));
            }

            if (pa_simple_flush(s, &error) < 0) {
                fprintf(stderr, __FILE__": pa_simple_flush() failed: %s\n", pa_strerror(error));
            }

            printf("Client disconnected\n");
            fflush(stdout);
            continue;
        }

        if(numbytes == -1)
            perror("recv failed");
    }


    return 0;
}
