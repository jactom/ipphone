/* client programme for the chat application. client connect to server and send messages*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#include "g711.c"
#define MAX_SIZE 8192//character buffer size

int sock; //socket file descriptor.
pa_simple *s = NULL;
timer_t timerid;

void sampler (union sigval);
static ssize_t loop_write(int, const void*, size_t);
/* A simple routine calling UNIX write() in a loop */

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
//			if (s)
//				pa_simple_free(s);
			close(sock);
            timer_delete(timerid);
			exit(0);
		}else{
			printf("Continuing..\n");
		}
	}
}

int main(int argc, char* argv[])
{
	if (argc != 3){ //usage is .cc <ip> <port>
		fprintf(stderr,"Usage:cc ip port\n");
		exit(1);
	}

	int c = 0;
	struct sockaddr_in server;

//    pthread_attr_t attr;
//    pthread_attr_init(&attr);

//    struct sched_param parm;
//    parm.sched_priority = 255;
//    pthread_attr_setschedparam(&attr, &parm);

	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE,
		.rate = 44100,
		.channels = 2
	};

    struct itimerspec per;
    per.it_value.tv_sec = 0;
    per.it_value.tv_nsec =100000;
    per.it_interval.tv_sec = 0;
    per.it_interval.tv_nsec = 100000;
	int ret = 1;
	int error;


    /* create timer handler */
    struct sigevent sig;
    sig.sigev_notify = SIGEV_THREAD;
    sig.sigev_notify_function = sampler;
    sig.sigev_value.sival_int = 10;
 //   sig. sigev_notify_attributes = &attr;

    if ((timer_create(CLOCK_REALTIME, &sig, &timerid)) == -1) {
      perror("timer: failed");
      exit(1);
    }
    
    printf("timer created\n");

	if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
		fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
		exit(1);
	}

	/* create socket*/
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("server: socket");
		exit(1);
	}

	printf("Socket created\n");

	/* configure socket */
	server.sin_addr.s_addr = inet_addr(argv[1]);
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(argv[2]));

	/* connect */
	if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0){
		perror("Connect failed.");
		exit(1);
	}

	printf("Connected\n");

	/* catch signal*/
	if (signal(SIGINT, sig_handler) == SIG_ERR)
		printf("\ncan't catch SIGINT\n");

    /* start timer */
    if ((timer_settime(timerid, 0, &per,  NULL)) == -1) {
      perror("timer_settime");
      exit(1);
    }

	/*client alwas run! */
	while (1) {


	}


finish:

	if (s)
		pa_simple_free(s);

	return ret;
}

static ssize_t loop_write(int fd, const void*data, size_t size) 
{
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

void sampler (union sigval val)
{

  uint16_t buf1[MAX_SIZE];
uint8_t buf[MAX_SIZE];
  int len, i;
  int error;

  /* catch audio... */
  if (pa_simple_read(s, buf1, sizeof(buf1), &error) < 0) {
    fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
  }

  len = sizeof(buf1) / sizeof(buf1[0]);
  
  for(i = 0; i < len; i++) {
    buf[i] = linear2alaw(buf1[i]);
  }  

  if (loop_write(sock, buf, sizeof(buf)) != sizeof(buf)) {
    fprintf(stderr, __FILE__": write() failed: %s\n", strerror(errno));
  }
}
