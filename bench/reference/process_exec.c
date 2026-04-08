/* Process exec: run 3 echo commands, sum output lengths */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int exec_len(const char* cmd) {
    FILE* f = popen(cmd, "r");
    if (!f) return 0;
    char buf[256];
    int len = 0;
    while (fgets(buf, sizeof(buf), f)) len += strlen(buf);
    pclose(f);
    /* strip trailing newline */
    if (len > 0) len--;
    return len;
}

int main(void) {
    printf("%d\n", exec_len("echo hello") + exec_len("echo world") + exec_len("echo yona"));
    return 0;
}
