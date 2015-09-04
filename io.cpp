#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "io.h"

#define FRAMES 4

/*
  ToDo: I've done this the wrong way.
  A better way would be:
   - The GUI thread asks for event to wake it up every 1/FRAMERATE seconds.
   - The gui asks to get the frame to display.
   - We pick the next frame, and send that back.
   - When we send back a frame to the gui, we can wake up the reader thread
     to let it store another frame.
   - This way, the gui thread never has to wait (except fr very short-lived
     mutex protecting the fifo).
   - We just return pointers into the fifo
   - If we have no more frames, we can return the last frame we have. Then
     we become behind, but we can remember how many frames we got behind, and
     catch up later by skipping frames we have any. However, we should check
     the clock if doing this I think ... probably it's not needed, as long
     as the gui time event is decent.
  But for now I'll keep what's here, if it turns out it does not work well
  -> implement something like above.
*/

static int num_ready_frames= 0;
static int first_free_frame= 0;
static int last_free_frame= FRAMES-1;
uint8_t frames[FRAMES][3*LEDS_X*LEDS_Y*LEDS_TANG];

pthread_mutex_t frames_mutex= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t frames_cond= PTHREAD_COND_INITIALIZER;

static int
get_free_slot()
{
  int frame;
  pthread_mutex_lock(&frames_mutex);
  while (num_ready_frames == FRAMES)
    pthread_cond_wait(&frames_cond, &frames_mutex);
  frame= first_free_frame;
  pthread_mutex_unlock(&frames_mutex);
  return frame;
}

static int
get_ready_slot()
{
  int frame;
  pthread_mutex_lock(&frames_mutex);
  while (num_ready_frames == 0)
    pthread_cond_wait(&frames_cond, &frames_mutex);
  frame= (last_free_frame+1) % FRAMES;
  pthread_mutex_unlock(&frames_mutex);
  return frame;
}

static void
slot_ready()
{
  pthread_mutex_lock(&frames_mutex);
  first_free_frame= (first_free_frame+1) % FRAMES;
  ++num_ready_frames;
  pthread_cond_signal(&frames_cond);
  pthread_mutex_unlock(&frames_mutex);
}

static void
release_slot()
{
  pthread_mutex_lock(&frames_mutex);
  last_free_frame= (last_free_frame+1) % FRAMES;
  --num_ready_frames;
  pthread_cond_signal(&frames_cond);
  pthread_mutex_unlock(&frames_mutex);
}

static void *
io_thread_handler(void *app_data __attribute__((unused)))
{
  /* Frame format is raw framebuffer data padded to multiple of 512 bytes. */
  uint8_t buf[(3*LEDS_X*LEDS_Y*LEDS_TANG+511)/512*512];

  for (;;)
  {
    unsigned sofar= 0;
    while (sofar < sizeof(buf))
    {
      ssize_t res= read(0, &(buf[sofar]), sizeof(buf) - sofar);
      if (res < 0 && errno != EINTR)
      {
        fprintf(stderr, "Error: read() returns res=%d: %d: %s\n",
                (int)res, errno, strerror(errno));
        exit(1);
      }
      if (res == 0)
      {
        /*
          End-of file.
          Try to seek to the start (works if normal file).
          If it doesn't work (eg. pipe from generator program), stop.
        */
        off_t ret= lseek(0, 0, SEEK_SET);
        if (ret == (off_t)-1)
          exit(0);
        sofar= 0;
      }
      sofar+= res;
    }

    int slot= get_free_slot();
    memcpy(frames[slot], buf, 3*LEDS_X*LEDS_Y*LEDS_TANG);
    slot_ready();
  }

  return NULL;
}


static uint8_t current_frame[3*LEDS_X*LEDS_Y*LEDS_TANG];
static pthread_mutex_t current_frame_mutex= PTHREAD_MUTEX_INITIALIZER;

/*
  This thread maintains the frame rate.
  It keeps a notion of the current frame.
  It sleeps to switch frames at the appropriate time.
*/
static void *
framerate_thread_handler(void *app_data __attribute__((unused)))
{
  struct timeval time_start, time_now;
  int res= gettimeofday(&time_start, NULL);
  if (res)
  {
    fprintf(stderr, "gettimeofday() returned %d: %d: %s\n",
            res, errno, strerror(errno));
    exit(1);
  }
  uint64_t frame= 0;
  for (;;)
  {
    int slot= get_ready_slot();
    pthread_mutex_lock(&current_frame_mutex);
    memcpy(current_frame, &(frames[slot]), sizeof(frames[slot]));
    pthread_mutex_unlock(&current_frame_mutex);
    release_slot();

    ++frame;

    int res= gettimeofday(&time_now, NULL);
    if (res)
    {
      fprintf(stderr, "gettimeofday() returned %d: %d: %s\n",
              res, errno, strerror(errno));
      exit(1);
    }

    uint64_t usec_passed;
    usec_passed= (uint64_t)1000000 * (time_now.tv_sec - time_start.tv_sec);
    usec_passed= (usec_passed + time_now.tv_usec) -time_start.tv_usec;
    uint64_t usec_target= frame * ((uint64_t)1000000 / FRAMERATE);
    if (usec_passed < usec_target)
    {
      usleep(usec_target - usec_passed);
    }
    else
    {
      /* Give up if we get too far behind. */
      uint64_t delta= usec_passed - usec_target;
      if (delta > (uint64_t)500000)
      {
        frame= 0;
        memcpy(&time_start, &time_now, sizeof(time_start));
      }
    }
  }

  return NULL;
}

const uint8_t *
get_current_frame()
{
  pthread_mutex_lock(&current_frame_mutex);
  return &(current_frame[0]);
}

void
release_frame()
{
  pthread_mutex_unlock(&current_frame_mutex);
}


static pthread_t io_thread;
static pthread_t current_frame_thread;

void
start_io_threads()
{
  int res= pthread_create(&io_thread, NULL, io_thread_handler, NULL);
  if (res != 0)
  {
    fprintf(stderr, "Error: pthread_create() failed: %d\n",  res);
    exit(1);
  }
  res= pthread_create(&current_frame_thread, NULL, framerate_thread_handler, NULL);
  if (res != 0)
  {
    fprintf(stderr, "Error: pthread_create() failed: %d\n",  res);
    exit(1);
  }
  // ToDo: A way to stop the thread nicely at app exit...
}
