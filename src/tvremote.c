#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>

#include "m3uparser.h"

pid_t current_player = 0;
int pipefd[2];

void kill_player(void) {
    printf("Killing player\n");
    if (current_player) {
        close(pipefd[1]);
        printf("Sending SIGINT to %d\n", current_player);
        kill(current_player, SIGINT);
        printf("Waiting for termination of %d\n", current_player);
        waitpid(current_player, NULL, 0);
        printf("%d has terminated\n", current_player);
        current_player = 0;
    }
}

void play(const char *url) {
    printf("Playing %s\n", url);
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(errno);
    }
    pid_t child = fork();
    if (child > 0) { // parent
        printf("Player started at %d\n", child);
        current_player = child;
        close(pipefd[0]);
    }
    else if (child == 0) { // child
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        setenv("LD_LIBRARY_PATH", "/opt/vc/lib:/usr/lib/omxplayer", 0);
        if (execl("/usr/bin/omxplayer.bin", "/usr/bin/omxplayer.bin", url, NULL) == -1) {
            perror("execl");
            exit(errno);
        }
    }
    else {
        perror("fork");
        exit(errno);
    }
}

void next_audio(void) {
    printf("Switching to the next audio channel\n");
    if (current_player) {
        printf("Found player at %d, sending 'k'\n", current_player);
        write(pipefd[1], "k", 1);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s playlist.m3u\n", argv[0]);
        return 1;
    }
    
    int channels, c = 0;
    char **url = NULL;
    
    if (!parse_m3u(argv[1], &url, &channels)) {
        fprintf(stderr, "Couldn't parse playlist.\n");
        return 1;
    }
    
    time_t temp_time, last_chan_change = 0, last_audio_change = 0;
    
    int fd, r;
    char buf[128], *ptr;
    struct sockaddr_un addr;
    
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/dev/lircd");
    
    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        exit(errno);
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        exit(errno);
    }
    
    for (;;) {
        r = read(fd, buf, 128);
        if (r == -1) {
            perror("read");
            exit(errno);
        }
        else if (r == 0) {
            exit(0);
        }
        
        ptr = strchr(buf, ' ');
        ptr = strchr(&ptr[1], ' ');
        ptr = &ptr[1];
        
        if (strncmp("KEY_POWER ", ptr, 10) == 0) {
            kill_player();
        }
        else if (strncmp("KEY_PREVIOUS ", ptr, 13) == 0 || strncmp("KEY_NEXT ", ptr, 9) == 0) {
            time(&temp_time);
            if (temp_time-last_chan_change > 2) {
                last_chan_change = temp_time;
                if (ptr[4] == 'P') { // prev
                    c = c > 0 ? c-1 : channels-1;
                }
                else { // next
                    c = (c+1)%channels;
                }
                kill_player();
                play(url[c]);
            }
        }
        else if (strncmp("KEY_AUDIO ", ptr, 10) == 0) {
            time(&temp_time);
            if (temp_time-last_audio_change > 1) {
                last_audio_change = temp_time;
                next_audio();
            }
        }
    }
}
