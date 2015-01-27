#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

char* trim(char *str) {
    char *end;
    
    while (isspace((unsigned char) *str)) str++;
    
    if (*str == 0) return str;
    
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char) *end)) end--;
    
    *(end+1) = 0;
    
    return str;
}

int parse_m3u(const char *path, char ***channels, int *channel_count) {
    FILE *fp;
    char buffer[256], *line, **chans = NULL;
    int count = 0, current;
    
    fp = fopen(path, "r");
    if (fp == NULL) return 0;
    
    while (fgets(buffer, sizeof buffer, fp) != NULL) {
        line = trim(buffer);
        if (line[0] == '#') continue;
        
        current = count;
        count++;
        
        if ((chans = realloc(chans, count * sizeof(char*))) == NULL) {
            fprintf(stderr, "realloc failed\n");
            exit(1);
        }
        if ((chans[current] = malloc((strlen(line)+1) * sizeof(char))) == NULL) {
            fprintf(stderr, "malloc failed\n");
            exit(1);
        }
        strcpy(chans[current], line);
    }
    
    fclose(fp);
    
    *channels = chans;
    *channel_count = count;
    
    return 1;
}
