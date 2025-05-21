# Client-Server Application in C

This project is a client-server application developed in C to demonstrate various Inter-Process Communication (IPC) and network programming concepts in a Linux/Ubuntu environment.

## Project Structure

* **`server.c`**: Source code for the server part of the application. The server is multi-threaded and handles connections from multiple clients simultaneously.
* **`client.c`**: Source code for the client part of the application. It launches 4 client processes with different behaviors.
* **`Makefile`**: A file to automate the build process for the server and client.

## Technologies Used

The following technologies and concepts were applied in this project, corresponding to the assignment requirements:

1.  **Sockets**:
    * Used for network communication between clients and the server (`AF_INET`, `SOCK_STREAM`).
    * The server listens for incoming connections on port `7777` (constant `PORT`).
    * Clients connect to `127.0.0.1` (constant `SERVER_IP`).
    * Functions: `socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `send()`, `recv()`, `htons()`, `ntohl()`, `inet_pton()`.

2.  **Threads**:
    * The server uses threads (`pthread`) to handle each client connection in a separate thread. This allows the server to serve multiple clients concurrently.
    * Functions: `pthread_create()`, `pthread_detach()`.

3.  **Processes**:
    * The client part creates 4 child processes using `fork()`. Each process simulates a separate client.

4.  **IPC1 - Shared Memory**:
    * Client processes use POSIX shared memory (`shm_open()`, `ftruncate()`, `mmap()`, `munmap()`, `shm_unlink()`) to store and update client states (0 - inactive, 1 - active, 2 - finished work).
    * Shared memory name: `"/shared_memory"` (constant `SHM_NAME`).
    * The `shared_data_t` struct is used to organize data in shared memory.

5.  **IPC2 - Signals & Semaphores**:
    * **Semaphores**: POSIX semaphores (`sem_open()`, `sem_wait()`, `sem_post()`, `sem_close()`, `sem_unlink()`) are used to synchronize the operation of the 3rd and 4th clients. The fourth client waits until the third client finishes its work and releases the semaphore.
        * Semaphore name: `"/client_sync_sem"` (constant `SEM_NAME`).
    * **Signals**: Signals (`SIGUSR1`) are used in client processes солнечwith timers to implement timeouts for user input.
        * Functions: `sigaction()`, `sigemptyset()`, `volatile sig_atomic_t`.

6.  **Timer**:
    * Client processes use POSIX timers (`timer_create()`, `timer_settime()`) to limit the waiting time for user input (10 seconds). When the time expires, the `SIGUSR1` signal is triggered.
    * Uses `CLOCK_REALTIME`.

7.  **Makefile**:
    * A `Makefile` is provided to simplify the compilation of the server and client parts (`make all`, `make server`, `make client`, `make clean`).
    * Uses the `gcc` compiler with flags `-Wall -pthread`.

## Working Logic

### Server:
1.  Creates a socket and listens for incoming connections on port `7777`.
2.  Upon a new client connection, creates a separate thread for it.
3.  In the client's thread:
    * Receives a number from the client.
    * If the number is `-1`, the client requests to terminate the connection.
    * Processes the number: determines if it's even/odd, divisible by 3, and if it was the first number from this client.
    * Sends the processing results back to the client.
4.  Frees resources when the client thread finishes.

### Client:
1.  **Main Process (`main`)**:
    * Creates and initializes shared memory to store client states.
    * Creates a named semaphore.
    * Creates 4 child processes (clients).
    * Waits for all child processes to complete.
    * Prints the final states of the clients from shared memory.
    * Cleans up resources (semaphore, shared memory).

2.  **Child Processes (Clients 1-4)**:
    * **Clients 1 and 2**: Announce their creation but do not connect to the server. Their state in shared memory remains `0` (inactive).
    * **Client 3**:
        * Connects to the server.
        * Updates its state in shared memory to `1` (active).
        * In a loop:
            * Prompts the user for a number (with a 10-second timeout).
            * If timeout or input error, disconnects.
            * Sends the number to the server.
            * Receives and prints the response from the server.
            * Asks if the user wants to send another number (with a 10-second timeout).
            * If not, or if timeout, sends a termination signal (`-1`) to the server and disconnects.
        * Upon completion, updates its state in shared memory to `2` (finished work).
        * **Releases the semaphore** (`sem_post`), allowing the 4th client to start.
    * **Client 4**:
        * **Waits on the semaphore** (`sem_wait`) until the 3rd client finishes its work.
        * After the semaphore is released, connects to the server and performs the same logic as Client 3 (state update, server interaction, timeouts).
        * Upon completion, updates its state in shared memory to `2` (finished work).

## Build and Run

1.  **Clone the repository or create the files**:
    Ensure `server.c`, `client.c`, and `Makefile` are in the same directory.

2.  **Build the project**:
    Open a terminal in the directory with the files and run:
    ```bash
    make
    ```
    Or to build them separately:
    ```bash
    make server
    make client
    ```

3.  **Run the server**:
    In one terminal window, start the server:
    ```bash
    ./server
    ```
    The server will output: `Server listening on port 7777`.

4.  **Run the clients**:
    In another terminal window, start the client application:
    ```bash
    ./client
    ```
    You will see messages from all four client processes. Clients 3 and 4 will prompt for number input.

    Example interaction with Client 3 (or 4):
    ```
    Client process 3 connected to the server.
    Client 3: Enter a number (10 seconds to respond): 10
    The number 10 is even.
    The number 10 is not divisible by 3.
    This is the first number sent by this client.

    Do you want to send another number? (y/n): y
    Client 3: Enter a number (10 seconds to respond): 9
    The number 9 is odd.
    The number 9 is divisible by 3.
    This is not the first number sent by this client.

    Do you want to send another number? (y/n): n
    Client process 3 terminated.
    ```

5.  **Stopping and Cleaning Up**:
    * The server can be stopped by pressing `Ctrl+C` in its terminal.
    * After the client and server have finished, you can remove the compiled files with:
        ```bash
        make clean
        ```
    * **Important**: If the client or server terminates abnormally, named IPC objects (semaphore `/client_sync_sem` and shared memory `/shared_memory`) might remain in the system. They can be found and manually deleted from `/dev/shm/`.

## Potential Issues and Notes
* **Naming of IPC Objects**: Shared memory and semaphore names (`/shared_memory`, `/client_sync_sem`) must be unique in the system. If previous runs terminated incorrectly, manual deletion from `/dev/shm` might be necessary.
* **Permissions**: Appropriate permissions may be required to create named IPC objects.
* **`pthread` Library**: When compiling manually (without Makefile), do not forget to link the thread library: `gcc ... -pthread`. The Makefile handles this automatically.
* **Code Comments**: Some comments in the source code are in Slovak and Russian.

---
This assignment demonstrates the comprehensive use of system calls and libraries to create a distributed application with inter-process communication and synchronization.
