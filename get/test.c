#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include <ev.h>
#include <curl/curl.h>

#define CURL_TO_EV(action) (action&CURL_POLL_IN?EV_READ:0)|(action&CURL_POLL_OUT?EV_WRITE:0)

struct _HttpRequest;
struct _HttpProcessor;

typedef void(*HttpRequestCompleteCB)(struct _HttpRequest*,const char*,size_t,const char*,size_t);
typedef void(*HttpProcessorErrorCB)(struct _HttpProcessor *, const char*, CURLMcode);

/* HttpRequest:
 * Each request gets one of these structs.
 *
 * stores the result body and head, passing them to complete_cb after the request completes
 * */
typedef struct _HttpRequest {
  CURL *handle;
  HttpRequestCompleteCB complete_cb;
  struct ev_io *io_events;
  struct ev_timer *timer_events;
  char error[CURL_ERROR_SIZE];

  char *head;
  char *body;

  size_t head_size;
  size_t body_size;

} HttpRequest;

/* create a new request object */
HttpRequest *http_request_new();
/* set the request complete callback */
void http_request_set_complete_cb(HttpRequest *, HttpRequestCompleteCB);
/* free the new request, this can usually be done within the complete callback */
void http_request_free( HttpRequest *hr );

/* runs an event loop, for processing http requests
 * Typically, you'll only want 1 of these structs. It handles the main event loop.
 **/
typedef struct _HttpProcessor {
  struct ev_loop *loop;
  struct ev_timer timer_events;
  CURLM *handle;
  int running, started;

  HttpProcessorErrorCB error_cb;

} HttpProcessor;

HttpProcessor *http_processor_new();
int http_processor_add_request(HttpProcessor *hp, HttpRequest *hr);
void http_processor_loop(HttpProcessor *hp);
void http_processor_free(HttpProcessor *hp);
void http_processor_set_error_cb(HttpProcessor *, HttpProcessorErrorCB);

/***
 * Start Example:
 *
 * Make 2 concurrent requests, and print the header and body of each response to STDOUT
 */

/* The complete callback calls http_request_free */ 
static void on_complete(HttpRequest *hr, const char *head, size_t head_size, const char *body, size_t body_size)
{
  char *buf = (char*)strndup(head,head_size);
  printf("Response:\n%s", buf);
  free(buf);
  fwrite(body, sizeof(char), body_size, stdout);
  http_request_free(hr);
}

/* if anything goes wrong, report it and get out */
static void on_error(HttpProcessor *hp, const char*where, CURLMcode code)
{
  printf( "error: %s code(%d)\n", where, code );
  exit(1);
}

int main(int argc, char **argv)
{
  int i = 0;
  HttpProcessor *hp = http_processor_new();
  char *samples[] = {"http://www.google.com/","http://www.yahoo.com/"};

  http_processor_set_error_cb(hp, on_error);

  for( i = 0; i < 2; ++i ) {
    HttpRequest *hr = http_request_new();

    http_request_set_complete_cb(hr, on_complete);

    curl_easy_setopt(hr->handle, CURLOPT_URL, samples[i]);

    http_processor_add_request(hp, hr);
  }

  http_processor_loop(hp);

  http_processor_free(hp);

  puts("\n");

  return 0;
}

/***
 * End the example.
 *
 * What follows is the implementation of the above interface
 */

/* from http://cool.haxx.se/cvs.cgi/curl/docs/examples/hiperfifo.c */
static void http_processor_check_http_request_status( HttpProcessor *hp )
{
  int          msgs_left;
  CURLMsg     *msg;
  HttpRequest *hr=NULL;
  CURL        *easy;
  CURLcode     res;

  do {

    easy = NULL;

    while( (msg = curl_multi_info_read(hp->handle, &msgs_left)) ) {
      if( msg->msg == CURLMSG_DONE ) {
        easy = msg->easy_handle;
        res  = msg->data.result;
        break;
      }
    }
    if( easy ) {
      curl_easy_getinfo(easy, CURLINFO_PRIVATE, &hr);
      curl_multi_remove_handle(hp->handle, easy);

      if( hr ) {

        hp->started--;

        if( hr->complete_cb ) {
          hr->complete_cb(hr,hr->head,hr->head_size,hr->body,hr->body_size);
        }
        else {
          http_request_free( hr );
        }

      }
    }

  } while( easy );

  if( hp->running == 0 && hp->started == 0 ) {
    ev_unloop(hp->loop, EVUNLOOP_ALL);
  }
}

/* from http://cool.haxx.se/cvs.cgi/curl/docs/examples/hiperfifo.c */
static void http_processor_mcode_or_die(HttpProcessor *hp, const char *where, CURLMcode code)
{
  if ( CURLM_OK != code ) {
    const char *s;
    switch (code) {
    case     CURLM_CALL_MULTI_PERFORM: s="CURLM_CALL_MULTI_PERFORM"; break;
    case     CURLM_OK:                 s="CURLM_OK";                 break;
    case     CURLM_BAD_HANDLE:         s="CURLM_BAD_HANDLE";         break;
    case     CURLM_BAD_EASY_HANDLE:    s="CURLM_BAD_EASY_HANDLE";    break;
    case     CURLM_OUT_OF_MEMORY:      s="CURLM_OUT_OF_MEMORY";      break;
    case     CURLM_INTERNAL_ERROR:     s="CURLM_INTERNAL_ERROR";     break;
    case     CURLM_UNKNOWN_OPTION:     s="CURLM_UNKNOWN_OPTION";     break;
    case     CURLM_LAST:               s="CURLM_LAST";               break;
    default: s="CURLM_unknown";
      break;
    case     CURLM_BAD_SOCKET:         s="CURLM_BAD_SOCKET";
     // fprintf(stderr, "ERROR: %s returns %s\n", where, s);
      /* ignore this error */
      return;
    }
    //fprintf(stderr, "ERROR: %s returns %s\n", where, s);

    if( hp->error_cb ) {
      hp->error_cb(hp, where, code);
    }
    else {
      exit(code);
    }
  }
}


/* Follows the sockets of a HttpRequest object, consider this private to the HttpRequest */
typedef struct _SockInfo {
  struct ev_io io_events;

  curl_socket_t sockfd;

  int action;
  int evset:1;

  CURL *handle;
  HttpProcessor *hp;

} SockInfo;

static void sock_info_watch_event(SockInfo *si, CURL *easy, curl_socket_t sockfd, int action );

/* called by libev on action from a curl multi socket */
static void sock_info_io_cb( struct ev_loop *loop, struct ev_io *w, int revents )
{
  CURLMcode rc;
  int mask = 0;
  SockInfo *si = (SockInfo*)w->data;
  HttpProcessor *hp = si->hp;

  if( revents & EV_READ ) mask |= CURL_CSELECT_IN;
  if( revents & EV_WRITE ) mask |= CURL_CSELECT_OUT;
  if( revents & EV_ERROR ) mask |= CURL_CSELECT_ERR;

  do {
    rc = curl_multi_socket_action( si->hp->handle, si->sockfd, mask, &(si->hp->running) );
  } while( rc == CURLM_CALL_MULTI_PERFORM);

  /* NOTE: si may have been freed at this point */
  http_processor_mcode_or_die( hp, __FUNCTION__, rc ); 
  http_processor_check_http_request_status( hp );
}


static SockInfo *sock_info_new(CURL *easy, HttpProcessor *hp, curl_socket_t sockfd, int action)
{
  SockInfo *si = (SockInfo*)calloc(1,sizeof(SockInfo));

  si->hp = hp;

  sock_info_watch_event( si, easy, sockfd, action );

  return si;
}

static void 
sock_info_watch_event(SockInfo *si, CURL *easy, curl_socket_t sockfd, int action )
{
  int kind = CURL_TO_EV(action);
 
  si->sockfd = sockfd;
  si->handle = easy;
  si->action = action;

  if( si->evset ) { ev_io_stop( si->hp->loop, &si->io_events ); }

  si->io_events.data = si;
  ev_io_init( &si->io_events, sock_info_io_cb, sockfd, kind );
  ev_io_start( si->hp->loop, &si->io_events );
 
  si->evset = 1;
}
static void 
sock_info_free(SockInfo *si)
{
  if( si ) {
    if( si->evset ) { ev_io_stop( si->hp->loop, &si->io_events ); }
    free(si);
  }
}

static size_t http_request_head_write_cb(void *ptr, size_t size, size_t nmemb, void *userp)
{
  HttpRequest *hr = (HttpRequest*)userp;
  size_t realsize = size * nmemb;

  if( hr->head ) {
    hr->head = (char*)realloc(hr->head, realsize + hr->head_size);
    memcpy(hr->head+hr->head_size, ptr, realsize);
    hr->head_size += realsize;
  }
  else {
    hr->head = (char*)malloc(realsize);
    memcpy(hr->head, ptr, realsize);
    hr->head_size = realsize;
  }

  return realsize;
}
static size_t http_request_body_write_cb(void *ptr, size_t size, size_t nmemb, void *userp)
{
  HttpRequest *hr = (HttpRequest*)userp;
  size_t realsize = size * nmemb;
  
  if( hr->body ) {
    hr->body = (char*)realloc(hr->body, realsize + hr->body_size);
    memcpy(hr->body+hr->body_size, ptr, realsize);
    hr->body_size += realsize;
  }
  else {
    hr->body = (char*)malloc(realsize);
    memcpy(hr->body, ptr, realsize);
    hr->body_size = realsize;
  }

//  printf( "write body action\n" );

  return realsize;
}

HttpRequest *http_request_new()
{
  HttpRequest *hr = (HttpRequest*)malloc(sizeof(HttpRequest));
  memset(hr,0,sizeof(HttpRequest));
  hr->handle = curl_easy_init();

  hr->error[0] = '\0';

  curl_easy_setopt(hr->handle, CURLOPT_HEADERFUNCTION, http_request_head_write_cb);
  curl_easy_setopt(hr->handle, CURLOPT_HEADERDATA, hr);
  curl_easy_setopt(hr->handle, CURLOPT_WRITEFUNCTION, http_request_body_write_cb);
  curl_easy_setopt(hr->handle, CURLOPT_WRITEDATA, hr);
  curl_easy_setopt(hr->handle, CURLOPT_ERRORBUFFER, hr->error);
  curl_easy_setopt(hr->handle, CURLOPT_PRIVATE, hr);

  hr->io_events = (struct ev_io*)malloc(sizeof(struct ev_io));
  memset(hr->io_events,0,sizeof(struct ev_io));

  hr->timer_events = (struct ev_timer*)malloc(sizeof(struct ev_timer));
  memset(hr->timer_events,0,sizeof(struct ev_timer));

  hr->head = NULL;
  hr->head_size = 0;
  hr->body = NULL;
  hr->body_size = 0;

  return hr;
}

void http_request_set_complete_cb(HttpRequest *hr, HttpRequestCompleteCB hrc)
{
  hr->complete_cb = hrc;
}

void http_processor_set_error_cb(HttpProcessor *hp, HttpProcessorErrorCB hpc)
{
  hp->error_cb = hpc;
}

void http_request_free( HttpRequest *hr )
{
  curl_easy_cleanup( hr->handle );
  free( hr->io_events );
  free( hr->timer_events );
  if( hr->head ) {
    free( hr->head );
  }
  if( hr->body ) {
    free( hr->body );
  }
  free( hr );
}

/* CURLMOPT_SOCKETFUNCTION */
static int http_processor_socket_cb( CURL *easy,
                                     curl_socket_t sockfd,
                                     int action, /* on of CURL_CSELECT_[IN|OUT|ERR] */
                                     void *userp,
                                     void *socketp )
{
  HttpProcessor *hp = (HttpProcessor*)userp;
  SockInfo *si = (SockInfo*)socketp;

  if( si ) {
    if( action == CURL_POLL_REMOVE ) {
      curl_multi_assign( hp->handle, sockfd, NULL );
      sock_info_free( si ) ;
      si = NULL;
    }
    else {
      sock_info_watch_event( si, easy, sockfd, action );
      curl_multi_assign( hp->handle, sockfd, si );
    }
  }
  else {
    si = sock_info_new( easy, hp, sockfd, action );
    curl_multi_assign( hp->handle, sockfd, si );
  }
  return 0;
}

static void http_processor_timer_timeout_cb( struct ev_loop *loop, struct ev_timer *w, int revents)
{
  CURLMcode rc;
  HttpProcessor *hp = (HttpProcessor*)w->data;

  do {
    rc = curl_multi_socket_action(hp->handle, CURL_SOCKET_TIMEOUT, 0, &hp->running);
  } while (rc == CURLM_CALL_MULTI_PERFORM);
  
  http_processor_check_http_request_status( hp );
}

static int http_processor_timer_cb( CURLM *multi,
                                    long timeout_ms,
                                    void *userp )
{
  HttpProcessor *hp = (HttpProcessor*)userp;

  //  printf("update timeout: %.5f seconds or %ld ms \n", (timeout_ms/60.0), timeout_ms);
  hp->timer_events.data = hp;
  if( timeout_ms == 0 ) {
    http_processor_timer_timeout_cb( hp->loop, &hp->timer_events, 0 );
  }
  else {
    ev_timer_init(&hp->timer_events, http_processor_timer_timeout_cb, 0.0, (timeout_ms / 60.0));
    ev_timer_again(hp->loop, &hp->timer_events);
  }

  return 0;
}

HttpProcessor *http_processor_new()
{
  HttpProcessor *hp = (HttpProcessor*)malloc(sizeof(HttpProcessor));
  memset(hp,0,sizeof(HttpProcessor));

  hp->handle = curl_multi_init();

  /* setup callback functions */
  curl_multi_setopt(hp->handle, CURLMOPT_SOCKETFUNCTION, http_processor_socket_cb);
  curl_multi_setopt(hp->handle, CURLMOPT_SOCKETDATA, hp);
  curl_multi_setopt(hp->handle, CURLMOPT_TIMERFUNCTION, http_processor_timer_cb);
  curl_multi_setopt(hp->handle, CURLMOPT_TIMERDATA, hp);

  hp->error_cb = NULL;

  hp->loop = ev_loop_new(0);

  hp->running = hp->started = 0;

  return hp;
}

int http_processor_add_request(HttpProcessor *hp, HttpRequest *hr)
{
  CURLMcode rc;

  hp->started++;
  rc = curl_multi_add_handle( hp->handle, hr->handle );

  return rc;
}

void http_processor_loop(HttpProcessor *hp)
{
  ev_loop(hp->loop,0);
}

void http_processor_free(HttpProcessor *hp)
{
  curl_multi_cleanup(hp->handle);
  ev_loop_destroy(hp->loop);
  free(hp);
}
