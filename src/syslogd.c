#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define LOGPATH "/var/log"
#define SOCKPATH "/dev/log"

static FILE *open_next_file(const char *dir)
{
  FILE *f;
  char path[0xff];
  static int n = 1;

  do
    {
      snprintf(path, sizeof path, "%s/syslog.%d", dir, n++);
    }
  while(access(path, F_OK) == 0);

  return fopen(path, "w");
}

int main(int argc, char *argv[])
{
  FILE *o;
  int sockfd;
  struct sockaddr_un addr;
  time_t fflush_timestamp;


  if((o = open_next_file(LOGPATH)) == NULL)
    {
      perror("fopen");
      exit(EXIT_FAILURE);
    }

  if((sockfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
    {
      perror("socket");
      exit(EXIT_FAILURE);
    }

  memset(&addr, 0x00, sizeof addr);
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof addr.sun_path, "%s", SOCKPATH);

  unlink(SOCKPATH);

  if(bind(sockfd, (struct sockaddr*) &addr, sizeof addr) < 0)
    {
      perror("bind");
      exit(EXIT_FAILURE);
    }

  fflush_timestamp = time(NULL) + 15;

  while(1)
    {
      fd_set readfds;
      struct timeval tv;
      int stts;

      FD_ZERO(&readfds);
      FD_SET(sockfd, &readfds);

      tv.tv_sec = 1;
      tv.tv_usec = 0;

      stts = select(sockfd + 1, &readfds, NULL, NULL, &tv);

      if(stts < 0)
	perror("select");
      else if(stts)
	{
	  char b[1024];
	  int cnt;

	  cnt = recv(sockfd, b, (sizeof b) - 1, MSG_NOSIGNAL);

	  printf("received %d\n", cnt);

	  if(cnt > 0)
	    {
	      off_t off;

	      b[cnt] = '\n';

	      fwrite(b, 1, cnt, o);

	      off = ftell(o);

	      if(off >= 0xf4240) // 1 MB
		{
		  fclose(o);

		  if((o = open_next_file(LOGPATH)) == NULL)
		    {
		      perror("fopen");
		      exit(EXIT_FAILURE);
		    }
		}
	    }
	}

      if(time(NULL) >= fflush_timestamp)
	{
	  fflush(o);
	  fflush_timestamp = time(NULL) + 15;
	}
    }

  return EXIT_SUCCESS;
}
