#include <base/base_io.h>
#include <base/base_types.h>
#include <base/mem.h>
#include <platform/platform.h>
#include <base/numconv.h>

uint32_t write_all(int fd, ciovec_t* iovs, size_t iovs_len) {
    size_t i;
    size_t nwritten;
    uint32_t ret;

    for (i = 0; i < iovs_len; ) {
        ret = platform_fd_write(fd, &iovs[i], iovs_len - i, &nwritten);
        if (ret != 0) {
            return ret; // Return error code
        }

        // Advance through the iovecs based on how much was written
        while (nwritten > 0 && i < iovs_len) {
            if (nwritten >= iovs[i].buf_len) {
                nwritten -= iovs[i].buf_len;
                i++;
            } else {
                iovs[i].buf = (const uint8_t*)iovs[i].buf + nwritten;
                iovs[i].buf_len -= nwritten;
                nwritten = 0;
            }
        }
    }
    return 0; // Success
}

void writeln(int fd, char* text) {
    const char *msg1 = text;
    const char *msg2 = "\n";

    ciovec_t iovs[2];
    iovs[0].buf = msg1;
    iovs[0].buf_len = base_strlen(msg1);
    iovs[1].buf = msg2;
    iovs[1].buf_len = base_strlen(msg2);

    write_all(fd, iovs, 2);
}

void writeln_int(int fd, char* text, int n) {
    const char *msg1 = text;
    const char *msg2 = " ";
    char p[32]; size_t p_len = int_to_str(n, p); p[p_len] = '\0';
    const char *msg3 = "\n";

    ciovec_t iovs[4];
    iovs[0].buf = msg1;
    iovs[0].buf_len = base_strlen(msg1);
    iovs[1].buf = msg2;
    iovs[1].buf_len = base_strlen(msg2);
    iovs[2].buf = p;
    iovs[2].buf_len = base_strlen(p);
    iovs[3].buf = msg3;
    iovs[3].buf_len = base_strlen(msg3);

    write_all(PLATFORM_STDERR_FD, iovs, 4);
}

void writeln_loc(int fd, const char *text, const char *file, unsigned int line, const char *function) {
    char line_str[32]; size_t p_len = int_to_str(line, line_str);
    line_str[p_len] = '\0';

    const char *msg[] = {file, ":", line_str, " in ",
        function, "(): ", text, "\n"};

    ciovec_t iovs[array_size(msg)];
    for (int i=0; i<array_size(msg); i++) {
        iovs[i].buf = msg[i];
        iovs[i].buf_len = base_strlen(msg[i]);
    }

    write_all(fd, iovs, array_size(msg));
}
