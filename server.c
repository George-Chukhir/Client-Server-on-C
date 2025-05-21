#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 7777
#define BUF_SIZE 1024

typedef struct {
    int client_socket;
} client_data_t;

void process_number(int client_socket, int number, int *is_first_number) {
    char response[BUF_SIZE];

    if (number % 2 == 0) {
        snprintf(response, sizeof(response), "The number %d is even.\n", number);
    } else {
        snprintf(response, sizeof(response), "The number %d is odd.\n", number);
    }
    send(client_socket, response, strlen(response), 0);

    if (number % 3 == 0) {
        snprintf(response, sizeof(response), "The number %d is divisible by 3.\n", number);
    } else {
        snprintf(response, sizeof(response), "The number %d is not divisible by 3.\n", number);
    }
    send(client_socket, response, strlen(response), 0);

    if (*is_first_number) {
        snprintf(response, sizeof(response), "This is the first number sent by this client.\n\n");
        *is_first_number = 0;
    } else {
        snprintf(response, sizeof(response), "This is not the first number sent by this client.\n\n");
    }
    send(client_socket, response, strlen(response), 0);
}

//proccess clients in own thread
void *handle_client(void *arg) {
   //data is a pointer on struct
    client_data_t *data = (client_data_t *)arg;
    //client_socket takes data from data
    int client_socket = data->client_socket;
    int is_first_number = 1;
    int number;

    while (1) {
        // Получение числа от клиента
        if (recv(client_socket, &number, sizeof(number), 0) <= 0) {
            break;
        }
        number = ntohl(number);  // Преобразуем в host order

        // Проверка на сигнал завершения
        if (number == -1) {
            printf("Client requested to terminate the connection.\n");
            break;
        }

        process_number(client_socket, number, &is_first_number);
    }

    close(client_socket);
    free(data);
    return NULL;
}

int main() {
    int server_socket;
    struct sockaddr_in server_addr, client_addr; 
    socklen_t client_addr_len = sizeof(client_addr);

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET; //pouzivame sietovy socket
    server_addr.sin_addr.s_addr = INADDR_ANY; //socket connect anyone who comes
    server_addr.sin_port = htons(PORT); //server port

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
	//bind connects socket and port 
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 4) < 0) {
	//listen puts the socket into mode of listnening for conections
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        client_data_t *data = malloc(sizeof(client_data_t)); //client_data_t is a struct which contain client socket
        data->client_socket = client_socket;

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, data) != 0) {  //create thread
            perror("Failed to create thread");
            close(client_socket);
            free(data);
        }
        pthread_detach(thread_id); //automatically closes thread after execution
    }

    close(server_socket);
    return 0;
}

