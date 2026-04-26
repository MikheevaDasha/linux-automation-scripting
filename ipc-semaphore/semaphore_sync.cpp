#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SEM_KEY 0x1234
#define SEM_PERMS 0666

union semun {
  int val;
  struct semid_ds *buf;
  unsigned short *array;
}

int main() {
    // Создание семафора
    int semid = semget(SEM_KEY, 1, IPC_CREAT | IPC_EXCL | SEM_PERMS);
    if (semid == -1) {
        if (errno == EEXIST) {
            // Если семафор уже существует, подключаемся к нему
            semid = semget(SEM_KEY, 1, SEM_PERMS);
        }
        if (semid == -1) {
            perror("semget");
            return 1;
        }
    }
    // Инициализируем семафор значением 0
    union semun arg;
    arg.val = 0;
    if (semctl(semid, 0, SETVAL, arg) == -1) {
        perror("semctl SETVAL");
        semctl(semid, 0, IPC_RMID);
        return 1;
    }
    // Создание дочернего процесаа
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        semctl(semid, 0, IPC_RMID);
        return 1;
    }
    if (pid == 0) {
        printf("Child: starting work...\n");
        sleep(2); // Имитация какой-то работы
        printf("Child: work is done, sending signal to parent...\n");
        struct sembuf post_op;
        post_op.sem_num = 0;
        post_op.sem_op = 1;  // V-операция: увеличить семафор на 1
        post_op.sem_flg = 0;
        if (semop(semid, &post_op, 1) == -1) {
            perror("semop child");
            exit(1);
        }
        printf("Child: semaphore incremented.\n");
        exit(0); // Завершаем дочерний процесс
    } else {
        // Родительский процесс
        printf("Parent: waiting for child...\n");
        struct sembuf wait_op;
        wait_op.sem_num = 0;
        wait_op.sem_op = -1;  // P-операция: уменьшить семафор на 1
        wait_op.sem_flg = 0;
        if (semop(semid, &wait_op, 1) == -1) {
            perror("semop parent");
            semctl(semid, 0, IPC_RMID);
            return 1;
        }
        printf("Parent: child has finished the required stage.\n");
        waitpid(pid, nullptr, 0); // Ожидание завершения дочернего процесса
        // Удаление семафора
        if (semctl(semid, 0, IPC_RMID) == -1) {
            perror("semctl IPC_RMID");
            return 1;
        }
        printf("Parent: semaphore removed, program finished.\n");
    }
    return 0;
}
