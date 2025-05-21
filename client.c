#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <time.h>

#define PORT 7777
#define SERVER_IP "127.0.0.1"
#define BUF_SIZE 1024
#define SHM_NAME "/shared_memory"  // name of shared memory
#define SEM_NAME "/client_sync_sem"

// struct for shared memory
typedef struct {
    int client_states[4];  // stav klientov (0 - neaktivny, 1 - aktivny, 2 - skoncil pracu)
} shared_data_t;

// global flag for timer
volatile sig_atomic_t timeout_occurred = 0;

// obsluha signálu časovača
void handle_timer_signal(int sig) {
    timeout_occurred = 1;  // set flag for time-out
}

// function for creating timer
timer_t create_timer(int signal) {
    struct sigevent se;
    se.sigev_notify = SIGEV_SIGNAL;  //  Upozornenie cez signál
    se.sigev_signo = signal;        // Signál na spracovanie udalostí
    se.sigev_value.sival_ptr = NULL;

    timer_t timer_id;
    if (timer_create(CLOCK_REALTIME, &se, &timer_id) == -1) {
        perror("Failed to create timer");
        exit(EXIT_FAILURE);
    }
    return timer_id;
}

// function for starting timer
void start_timer(timer_t timer_id, int seconds) {
    struct itimerspec ts;
    ts.it_value.tv_sec = seconds;  // o kolko sekund zacne fungovat timer
    ts.it_value.tv_nsec = 0;
    ts.it_interval.tv_sec = 0;    // Jednorazový časovač
    ts.it_interval.tv_nsec = 0;

    if (timer_settime(timer_id, 0, &ts, NULL) == -1) {
        perror("Failed to start timer");
        exit(EXIT_FAILURE);
    }
}

// Function which doing main logic for client
void run_client(int client_id, shared_data_t *shared_data) {
    int client_socket;
    struct sockaddr_in server_addr;
    char response[BUF_SIZE];
    int number;
    char choice;

    // nastavenia casovaca
    timer_t timer_id = create_timer(SIGUSR1);  // create timer
    struct sigaction sa;
    sa.sa_handler = handle_timer_signal;  // Zadavame obslužnú jednotku
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);  // Pripojenie signálu k obsluhe(handleru)

    // Konfigurácia adresy servera
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    // create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    // connection to server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // refresh state of client
    shared_data->client_states[client_id - 1] = 1;  // teraz client je aktivny

    // start cycle for sending numbers
    do {
        printf("Client %d: Enter a number (20 seconds to respond): ", client_id);
        fflush(stdout);

        timeout_occurred = 0;  // set flag of time-out on zero (vynulujeme)
        start_timer(timer_id, 20);  // set timer on 20 seconds

        // trying to take number from client
        if (scanf("%d", &number) == EOF || timeout_occurred) {
            printf("\nClient %d: Timeout occurred or no input provided. Disconnecting...\n", client_id);
            break;
        }

        number = htonl(number);  // Konverzia na sietovy poriadok
        send(client_socket, &number, sizeof(number), 0);

        // Getting answer from the server
        int bytes_received;
        printf("\n");
        while ((bytes_received = recv(client_socket, response, BUF_SIZE - 1, 0)) > 0) {
            response[bytes_received] = '\0';
            printf("%s", response);
            if (strstr(response, "not the first") || strstr(response, "first number")) { //strstr hlada ci dal nam server jednu z odpovedej
                break;
            }
        }

        printf("Do you want to send another number? (y/n): ");
        fflush(stdout); // "fflush" guarantees that our input in stdout will be immediately displayed

        timeout_occurred = 0;  //set flag of time-out on zero (vynulujeme) 
       start_timer(timer_id, 20);  // set timer on 20 seconds
        if (scanf(" %c", &choice) == EOF || timeout_occurred) {
            printf("\nClient %d: Timeout occurred or no input provided. Disconnecting...\n", client_id);
            break;
        }

        if (choice != 'y' && choice != 'Y') {
            int end_signal = htonl(-1);
            send(client_socket, &end_signal, sizeof(end_signal), 0);
            break;
        }
    } while (1);

    // refresh state of client
    shared_data->client_states[client_id - 1] = 2;

    close(client_socket);
    printf("Client process %d terminated.\n", client_id);
}

int main() {
    // "shm_open" creates shared memory
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Failed to create shared memory");
        exit(EXIT_FAILURE);
    }

    // "ftruncate" set size of memory
    if (ftruncate(shm_fd, sizeof(shared_data_t)) == -1) {
        perror("Failed to set size of shared memory");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    // "nmap" displays memory in address space
    shared_data_t *shared_data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED) {
        perror("Failed to map shared memory");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    // Initialize shared memory
    for (int i = 0; i < 4; i++) {
        shared_data->client_states[i] = 0;
    }

	// create semaphore for synchronize clients
    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0666, 0);
    if (sem == SEM_FAILED) {
        perror("Failed to create semaphore");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < 4; i++) {  // create 4 clients (processes)
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            if (i < 2) {
                printf("Client process %d created, but didn't connect to the server.\n", i + 1);
            } else if (i == 2) {
                printf("Client process %d connected to the server.\n", i + 1);
                run_client(i + 1, shared_data);
                sem_post(sem); // unlock fourth client
            } else {
                sem_wait(sem); // waiting termination of third client
                printf("Client process %d waited for the third client to finish.\n", i + 1);
                printf("Client process %d connected to the server.\n", i + 1);
                run_client(i + 1, shared_data);
            }
            exit(0);  // stopping a child process
        }
    }

    while (wait(NULL) > 0);

    // outputs states of clients from shared memory
    printf("Final states of clients:\n");
    for (int i = 0; i < 4; i++) {
        printf("Client %d state: %d\n", i + 1, shared_data->client_states[i]);
    }

    // Cleaning resources
    sem_close(sem);
    sem_unlink(SEM_NAME);
    munmap(shared_data, sizeof(shared_data_t)); 
    shm_unlink(SHM_NAME);
    printf("All client processes terminated.\n");

    return 0;
}


