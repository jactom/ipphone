/* client programme for the chat application. client connect to server and send messages*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ortp/ortp.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#include "g711.c"
#define MAX_SIZE 160//character buffer size

int runcond = 1; 
RtpSession *session;
pa_simple *s = NULL;
timer_t timerid;
int clockslide = 0;
int jitter = 0;

void sampler (int, siginfo_t*, void*);

/* client signal handler */
void sig_handler(int signumber)
{
	char ans[2];
	if(signumber == SIGINT){
		printf("receved CTRL-C\n");
		printf("Terminate y/n :");
		scanf("%s", ans);

		if(strcmp(ans, "y") == 0){
            runcond = 0;
			printf("Exiting....\n");
		}else{
			printf("Continuing..\n");
		}
	}
}

static const char *help="usage: cc dest_ip4addr dest_port [ --with-clockslide <value>  ] [ --with-jitter <milliseconds> ]\n";

int main(int argc, char* argv[])
{
	if (argc != 3){ 
      printf("%s", help);
      return -1;
	}

	int i;
    char *ssrc;
    sigset_t mask;

    for (i = 3; i < argc; i++) {
      if (strcmp(argv[i], "--with-clockslide") == 0) {
        i++;
        if (i >= argc) {
          printf("%s", help);
          return -1;
        }
        clockslide = atoi(argv[i]);
        ortp_message("Using clockslide of %i milisecond every 50 packets.",clockslide);
      } else if (strcmp(argv[i], "--with-jitter") == 0) {
        ortp_message("Jitter will be added to outgoing stream.");
        i++;
        if (i >= argc) {
          printf("%s", help);
          return -1;
        }
        jitter = atoi(argv[i]);
      }
    }

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

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = sampler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
      perror("sigaction");
      return -1;
    }

    struct itimerspec per;
    per.it_value.tv_sec = 0;
    per.it_value.tv_nsec =100000;
    per.it_interval.tv_sec = 0;
    per.it_interval.tv_nsec = 100000;
	int ret = 1;
	int error;

    printf("Blocking signal %d\n", SIGRTMIN);
    sigemptyset(&mask);
    sigaddset(&mask, SIGRTMIN);
    if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
      perror("sigprocmask");
      return -1;
    }

    /* create timer handler */
    struct sigevent sig;
    sig.sigev_notify = SIGEV_SIGNAL;
    sig.sigev_signo = SIGRTMIN;
    sig.sigev_value.sival_ptr = &timerid;

   /* if (signal(SIGALRM, sampler) == SIG_ERR) {
      perror("Signal");
    }*/

    if ((timer_create(CLOCK_REALTIME, &sig, &timerid)) == -1) {
      perror("timer: failed");
      return -1;
    }
    
    printf("timer created\n");

	if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
		fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        return -1;
	}
    printf("open mic\n");

	/* create configure and connect*/
    ortp_init();
    ortp_scheduler_init();
    ortp_set_log_level_mask( ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR);
    session = rtp_session_new(RTP_SESSION_SENDONLY);
    printf("create rtp\n");

    rtp_session_set_scheduling_mode(session, 1);
    rtp_session_set_blocking_mode(session, 1);
    rtp_session_set_connected_mode(session, TRUE);
    rtp_session_set_remote_addr(session, argv[1], atoi(argv[2]));
    rtp_session_set_payload_type(session, 1);
    printf("rtp config done\n");

    ssrc = getenv("SSRC");
    if (ssrc != NULL) {
      printf("using SSRC = %i.\n", atoi(ssrc));
      rtp_session_set_ssrc(session, atoi(ssrc));
    }


    /* start timer */
    if ((timer_settime(timerid, 0, &per,  NULL)) == -1) {
      perror("timer_settime");
      exit(1);
    }

    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
      perror("sigprocmask_");
      return -1;
    }

	if (signal(SIGINT, sig_handler) == SIG_ERR)
		printf("\ncan't catch SIGINT\n");

	/*client alwas run! */
	while (runcond) {
printf("%d\n",runcond);

    }
    timer_delete(timerid);
    if (s)
      pa_simple_free(s);
    rtp_session_destroy(session);
    ortp_exit();
    ortp_global_stats_display();
    printf("Done\n");

    return 0;
}


void sampler (int sig, siginfo_t* si, void* uc)
{
    printf("llo\n");
  static uint32_t user_ts = 0;
  uint16_t buf1[MAX_SIZE];
  uint8_t buf[MAX_SIZE];
  int len, i;
  int error;
  if (1/*signal == SIGALRM*/) {
    /* catch audio... */
    if (pa_simple_read(s, buf1, sizeof(buf1), &error) < 0) {
      fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
    }

    len = sizeof(buf1) / sizeof(buf1[0]);

    for(i = 0; i < len; i++) {
      buf[i] = Snack_Lin2Mulaw(buf1[i]);
    }  

    rtp_session_send_with_ts(session, buf, len, user_ts);
    user_ts += MAX_SIZE;

    if (clockslide!= 0 && user_ts %(MAX_SIZE * 50) == 0) {
      ortp_message("Clock sliding of %i milliseconds now", clockslide);
      rtp_session_make_time_distorsion(session, clockslide);
    }

    if (jitter && (user_ts % (8000) == 0)) {
      struct timespec pausetime, remtime;
      ortp_message("Simulating late packets now (%i miliseonds)", jitter);
      pausetime.tv_sec = jitter / 1000;
      pausetime.tv_nsec = (jitter % 1000) * 1000000;
      while (nanosleep(&pausetime, &remtime) == -1 && errno == EINTR) {
        pausetime = remtime;
      }
    }

    printf("hello\n");
    }
}
