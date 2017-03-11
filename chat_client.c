/* client programme for the chat application. client connect to server and send messages*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#define MAX_SIZE 8192//character buffer size

int sock; //socket file descriptor.
pa_simple *s = NULL;

/* A simple routine calling UNIX write() in a loop */
static ssize_t loop_write(int fd, const void*data, size_t size) {
    ssize_t ret = 0;

    while (size > 0) {
        ssize_t r;

        if ((r = send(fd, data, size, 0)) < 0)
            return r;

        if (r == 0)
            break;

        ret += r;
        data = (const uint8_t*) data + r;
        size -= (size_t) r;
    }

    return ret;
}

/* client signal handler */
void sig_handler(int signumber)
{
    char ans[2];
    if(signumber == SIGINT){
        printf("receved CTRL-C\n");
        printf("Terminate y/n :");
        scanf("%s", ans);

        if(strcmp(ans, "y") == 0){
            printf("Exiting....\n");
            if (s)
                pa_simple_free(s);
            close(sock);
            exit(0);
        }else{
            printf("Continuing..\n");
        }
    }
}

int main(int argc, char* argv[])
{
    if(argc != 3){ //usage is .cc <ip> <port>
        fprintf(stderr,"Usage:cc ip port\n");
        exit(1);
    }

    int c = 0;
    struct sockaddr_in server;
    char sendbuf[MAX_SIZE];

    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 2
    };
    int ret = 1;
    int error;

    if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        exit(1);
    }

    /* create socket*/
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("server: socket");
        exit(1);
    }

    printf("Socket created\n");

    /* configure socket */
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(argv[2]));

    /* connect */
    if(connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0){
        perror("Connect failed.");
        exit(1);
    }

    printf("Connected\n");

    /* catch signal*/
    if(signal(SIGINT, sig_handler) == SIG_ERR)
        printf("\ncan't catch SIGINT\n");

    /*client alwas run! */
    while(1){

        uint8_t buf[MAX_SIZE];

        /* Record some data ... */
        if (pa_simple_read(s, buf, sizeof(buf), &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
        }

        if (loop_write(sock, buf, sizeof(buf)) != sizeof(buf)) {
            fprintf(stderr, __FILE__": write() failed: %s\n", strerror(errno));
        }

    }


finish:

    if (s)
        pa_simple_free(s);

    return ret;
}
