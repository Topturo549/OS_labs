#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <errno.h>

#define BUFFER_SIZE 1024

static int write_all(int fd, const char *buffer, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        ssize_t n = write(fd, buffer + sent, length - sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static int is_whitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' ||
           c == '\v' || c == '\f' || c == '\r';
}

int main(void)
{
    char buffer[BUFFER_SIZE];
    for (;;) {
        ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (n < 0) {
            if (errno == EINTR) continue;
            const char msg[] = "Child2: ошибка read()\n";
            (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
            return 1;
        }
        if (n == 0) break;

        for (ssize_t i = 0; i < n; ++i) {
            if (is_whitespace(buffer[i])) buffer[i] = '_';
        }

        if (write_all(STDOUT_FILENO, buffer, (size_t)n) == -1) {
            const char msg[] = "Child2: ошибка write()\n";
            (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
            return 1;
        }
    }
    return 0;
}
