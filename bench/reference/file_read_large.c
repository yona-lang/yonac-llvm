/* Read a 50MB file into memory and measure length.
 * Fair comparison: allocates full buffer like Yona's readFile. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
int main(void) {
    const char* path = "bench/data/large_text.txt";
    FILE* f = fopen(path, "r");
    if (!f) return 1;
    struct stat st;
    stat(path, &st);
    size_t size = (size_t)st.st_size;
    char* buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    printf("%ld\n", strlen(buf));
    free(buf);
    return 0;
}
