// gen5gb.c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

int main(void) {
    const char *filename = "random_digits_5GiB.bin";
    /* 5 GiB = 5 * 1024^3 = 5368709120 bytes */
    const uint64_t total_bytes = 5ULL * 1024ULL * 1024ULL * 1024ULL;
    const size_t BUF_SIZE = 8 * 1024 * 1024; /* 8 MiB buffer */
    unsigned char *buf = malloc(BUF_SIZE);
    if (!buf) {
        perror("malloc");
        return 1;
    }

    FILE *f = fopen(filename, "wb");
    if (!f) {
        perror("fopen");
        free(buf);
        return 1;
    }

    /* Initialize RNG */
    srand((unsigned)time(NULL) ^ (unsigned)(uintptr_t)buf);

    uint64_t bytes_written = 0;
    while (bytes_written < total_bytes) {
        size_t to_fill = BUF_SIZE;
        if (total_bytes - bytes_written < (uint64_t)BUF_SIZE)
            to_fill = (size_t)(total_bytes - bytes_written);

        /* Fill buffer with random digits '0'..'9' */
        for (size_t i = 0; i < to_fill; ++i) {
            buf[i] = (unsigned char)('0' + (rand() % 10));
        }

        size_t written = fwrite(buf, 1, to_fill, f);
        if (written != to_fill) {
            perror("fwrite");
            fclose(f);
            free(buf);
            return 1;
        }
        bytes_written += written;
    }

    fclose(f);
    free(buf);
    return 0;
}
