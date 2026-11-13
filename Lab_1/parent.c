#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define BUFFER_SIZE 1024

static void print_error(const char *message, size_t length)
{
    (void)write(STDERR_FILENO, message, length);
}

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

static int read_exact(int fd, char *buffer, size_t length)
{
    size_t received = 0;
    while (received < length) {
        ssize_t n = read(fd, buffer + received, length - received);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        received += (size_t)n;
    }
    return 0;
}

int main(void)
{
    int parent_to_child1[2];
    int child1_to_child2[2];
    int child2_to_parent[2];

    if (pipe(parent_to_child1) == -1 ||
        pipe(child1_to_child2) == -1 ||
        pipe(child2_to_parent) == -1) {
        const char msg[] = "Ошибка: не удалось создать каналы\n";
        print_error(msg, sizeof(msg) - 1);
        return 1;
    }

    pid_t child1 = fork();
    if (child1 == -1) {
        const char msg[] = "Ошибка: fork() для Child1 завершился неудачно\n";
        print_error(msg, sizeof(msg) - 1);
        return 1;
    }

    if (child1 == 0) {
        close(parent_to_child1[1]);
        close(child1_to_child2[0]);
        close(child2_to_parent[0]);
        close(child2_to_parent[1]);

        if (dup2(parent_to_child1[0], STDIN_FILENO) == -1 ||
            dup2(child1_to_child2[1], STDOUT_FILENO) == -1) {
            const char msg[] = "Child1: ошибка dup2()\n";
            print_error(msg, sizeof(msg) - 1);
            _exit(1);
        }
        close(parent_to_child1[0]);
        close(child1_to_child2[1]);

        char *const args[] = {"./child1", NULL};
        execv(args[0], args);
        {
            const char msg[] = "Ошибка: не удалось запустить ./child1\n";
            print_error(msg, sizeof(msg) - 1);
        }
        _exit(1);
    }

    pid_t child2 = fork();
    if (child2 == -1) {
        const char msg[] = "Ошибка: fork() для Child2 завершился неудачно\n";
        print_error(msg, sizeof(msg) - 1);
        close(parent_to_child1[1]);
        close(child1_to_child2[0]);
        close(child1_to_child2[1]);
        close(child2_to_parent[0]);
        close(child2_to_parent[1]);
        (void)waitpid(child1, NULL, 0);
        return 1;
    }

    if (child2 == 0) {
        close(parent_to_child1[0]);
        close(parent_to_child1[1]);
        close(child1_to_child2[1]);
        close(child2_to_parent[0]);

        if (dup2(child1_to_child2[0], STDIN_FILENO) == -1 ||
            dup2(child2_to_parent[1], STDOUT_FILENO) == -1) {
            const char msg[] = "Child2: ошибка dup2()\n";
            print_error(msg, sizeof(msg) - 1);
            _exit(1);
        }
        close(child1_to_child2[0]);
        close(child2_to_parent[1]);

        char *const args[] = {"./child2", NULL};
        execv(args[0], args);
        {
            const char msg[] = "Ошибка: не удалось запустить ./child2\n";
            print_error(msg, sizeof(msg) - 1);
        }
        _exit(1);
    }

    close(parent_to_child1[0]);
    close(child1_to_child2[0]);
    close(child1_to_child2[1]);
    close(child2_to_parent[1]);

    const char prompt[] = "Введите текст (Ctrl+D для завершения):\n";
    (void)write_all(STDOUT_FILENO, prompt, sizeof(prompt) - 1);

    char buffer[BUFFER_SIZE];
    int failed = 0;
    for (;;) {
        ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (n < 0) {
            if (errno == EINTR) continue;
            const char msg[] = "Ошибка: чтение ввода завершилось неудачно\n";
            print_error(msg, sizeof(msg) - 1);
            failed = 1;
            break;
        }
        if (n == 0) break;

        if (write_all(parent_to_child1[1], buffer, (size_t)n) == -1) {
            const char msg[] = "Ошибка: передача данных дочернему процессу\n";
            print_error(msg, sizeof(msg) - 1);
            failed = 1;
            break;
        }
        if (read_exact(child2_to_parent[0], buffer, (size_t)n) == -1) {
            const char msg[] = "Ошибка: получение результата от Child2\n";
            print_error(msg, sizeof(msg) - 1);
            failed = 1;
            break;
        }
        if (write_all(STDOUT_FILENO, buffer, (size_t)n) == -1) {
            const char msg[] = "Ошибка: вывод результата\n";
            print_error(msg, sizeof(msg) - 1);
            failed = 1;
            break;
        }
    }

    close(parent_to_child1[1]); 
    close(child2_to_parent[0]);

    int status1 = 0, status2 = 0;
    while (waitpid(child1, &status1, 0) == -1 && errno == EINTR) {}
    while (waitpid(child2, &status2, 0) == -1 && errno == EINTR) {}

    if (!WIFEXITED(status1) || WEXITSTATUS(status1) != 0 ||
        !WIFEXITED(status2) || WEXITSTATUS(status2) != 0) {
        const char msg[] = "Ошибка: один из дочерних процессов завершился неуспешно\n";
        print_error(msg, sizeof(msg) - 1);
        failed = 1;
    }

    return failed ? 1 : 0;
}