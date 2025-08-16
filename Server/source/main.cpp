// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// Include the main libnx system header, for Switch development
#include <switch.h>

// Size of the inner heap, adjusted to match sys-ftpd for stability.
#define INNER_HEAP_SIZE 0xA7000

// Port to listen on
#define PORT 1234

// Debug log file
#define LOG_FILE "sdmc:/server_log.txt"

// Custom assert macro for cleaner error handling during initialization
#define R_ASSERT(res_expr)            \
    ({                                \
        const Result rc = (res_expr); \
        if (R_FAILED(rc))             \
        {                             \
            diagAbortWithResult(rc);  \
        }                             \
    })

#ifdef __cplusplus
extern "C" {
#endif

// --- libnx sysmodule setup ---

// Sysmodules should not use applet*.
u32 __nx_applet_type = AppletType_None;

// Sysmodules will normally only want to use one FS session.
u32 __nx_fs_num_sessions = 1;

// Newlib heap configuration function (makes malloc/free work).
void __libnx_initheap(void)
{
    static u8 inner_heap[INNER_HEAP_SIZE];
    extern void* fake_heap_start;
    extern void* fake_heap_end;

    // Configure the newlib heap.
    fake_heap_start = inner_heap;
    fake_heap_end   = inner_heap + sizeof(inner_heap);
}

// Service initialization.
void __appInit(void)
{
    // Initialize the service manager
    R_ASSERT(smInitialize());

    // Initialize services
    R_ASSERT(fsInitialize());
    R_ASSERT(fsdevMountSdmc());
    R_ASSERT(timeInitialize());
    R_ASSERT(hidInitialize());
    R_ASSERT(hidsysInitialize());

    // Configure socket services for a sysmodule
    static const SocketInitConfig socketInitConfig = {
        .tcp_tx_buf_size = 0x800,
        .tcp_rx_buf_size = 0x800,
        .tcp_tx_buf_max_size = 0x25000,
        .tcp_rx_buf_max_size = 0x25000,
        .udp_tx_buf_size = 0,
        .udp_rx_buf_size = 0,
        .sb_efficiency = 1,
    };
    R_ASSERT(socketInitialize(&socketInitConfig));

    // Close the service manager session
    smExit();
}

// Service deinitialization.
void __appExit(void)
{
    // Clean up services in reverse order
    socketExit();
    hidsysExit();
    hidExit();
    timeExit();
    fsdevUnmountAll();
    fsExit();
}

#ifdef __cplusplus
}
#endif

void log_message(const char* msg) {
    FILE* f = fopen(LOG_FILE, "a");
    if (f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

// Main program entrypoint
int main(int argc, char* argv[])
{
    log_message("Starting sysmodule");

    // Sockets are now initialized in __appInit, so we can use them directly.

    // Create server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_message("Failed to create socket");
        return 1;
    }
    log_message("Socket created");

    // Set up address
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    // Bind
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        log_message("Failed to bind");
        close(server_fd);
        return 1;
    }
    log_message("Bind successful");

    // Listen
    if (listen(server_fd, 3) < 0) {
        log_message("Failed to listen");
        close(server_fd);
        return 1;
    }
    log_message("Listening on port 1234");

    // Accept loop
    while (true) {
        socklen_t addrlen = sizeof(address);
        int client_fd = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (client_fd < 0) {
            log_message("Accept failed, continuing");
            continue;
        }
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
        char log_buf[64];
        snprintf(log_buf, sizeof(log_buf), "Client accepted from %s", client_ip);
        log_message(log_buf);


        // Send "hello world"
        const char* msg = "hello world\n";
        if (send(client_fd, msg, strlen(msg), 0) < 0) {
            log_message("Send failed");
        } else {
            log_message("Message sent");
        }

        // Close client socket
        close(client_fd);
        log_message("Client disconnected");
    }

    // Cleanup (unreachable, but for completeness)
    close(server_fd);

    return 0;
}