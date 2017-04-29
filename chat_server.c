#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ortp/ortp.h>
#include <bctoolbox/vfs.h>

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>
#include "g711.c"

#define MAX_SIZE 160
#define BACKLOG 3

int cond = 1;
pa_simple *s = NULL;
RtpSession *session;
/* signal handler */
void sig_handler(int signumber)
{
  if (signumber == SIGINT) {
    printf("\nreceived CTRL-C\n Terminating....\n");

    cond = 0;
  }
}

void ssrc_cb(RtpSession *session) 
{
  printf("ssrc changed\n");
}

int main(int argc, char* argv[])
{
  if(argc != 2){// usage : ./cs <port number>
    fprintf(stderr,"usage : cs port\n");
    exit(1);
  }

  int i, j, len,  numbytes;
  uint8_t message_buf[MAX_SIZE];
  uint16_t m_buf[MAX_SIZE];
  int err;
  uint32_t ts = 0;
  int stream_received = 0;
  int local_port;
  int have_more;
  int jittcomp = 40;
  bool_t adapt = TRUE;

  static const pa_sample_spec ss = {
    .format = PA_SAMPLE_S16LE,
    .rate = 44100,
    .channels = 2
  };

  int ret = 1;
  int error;

  local_port = atoi(argv[1]);

  for (i = 2; i < argc; i++) {
    if (strcmp(argv[i], "--noadapt") == 0) adapt = FALSE;
    if (strcmp(argv[i], "--with-jitter") == 0) {
      i++;
      if (i < argc) {
        jittcomp = atoi(argv[i]);
        printf("Using jitter");
      }
    }
  }

  ortp_init();
  ortp_scheduler_init();
  ortp_set_log_level_mask( ORTP_DEBUG | ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR);
  signal(SIGINT, sig_handler);
  session=rtp_session_new(RTP_SESSION_RECVONLY);  
  rtp_session_set_scheduling_mode(session, 1);
  rtp_session_set_blocking_mode(session, 1);
  rtp_session_set_local_addr(session,"127.0.0.1", atoi(argv[1]), atoi(argv[1]) + 1);
  rtp_session_set_connected_mode(session, TRUE);
  rtp_session_set_symmetric_rtp(session, TRUE);
  rtp_session_enable_adaptive_jitter_compensation(session, adapt);
  rtp_session_set_jitter_compensation(session, jittcomp);
  rtp_session_set_payload_type(session, 1);
  rtp_session_signal_connect(session, "ssrc_changed", (RtpCallback)ssrc_cb, 0);
  rtp_session_signal_connect(session, "ssrc_changed", (RtpCallback)rtp_session_reset, 0);

  /* create socket */
  /* clear memory to avoid undefined behaviour */

  /* configure socket */

  if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
    fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
    exit(1);
  }

  /* listening */

  /*catch signal */

  /* server waits for client */
  while (cond){
    have_more = 1;
    while (have_more) {
      err = rtp_session_recv_with_ts(session, message_buf, MAX_SIZE, ts, &have_more);
      if (err > 0) stream_received = 1;
      if ((stream_received) && (err > 0)) {
        len = err;//sizeof(message_buf) / sizeof(message_buf[0]);
        for(j = 0; j < len ; j++) {
          m_buf[j] = Snack_Mulaw2Lin(message_buf[j]);
        }

        if (pa_simple_write(s, m_buf, (size_t) len, &error) < 0) {
          fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
        }

      }


    }
    ts += MAX_SIZE;
    /* accept client connection */
    /* receive client audio*/
    /* ... and play it */

  }


  if (s)
    pa_simple_free(s);

  exit(0);

  rtp_session_destroy(session);
  ortp_exit();

  ortp_global_stats_display();

  return 0;
}
