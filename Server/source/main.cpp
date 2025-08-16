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
#include <switch/services/pm.h>
#include <switch/services/clkrst.h>
#include <switch/services/pcv.h>
#include <switch/services/set.h>

// Size of the inner heap, adjusted for stability.
#define INNER_HEAP_SIZE 0xA7000

// Port to listen on
#define PORT 1234

// Debug log file
#define LOG_FILE "sdmc:/server_log.txt"

// Custom assert macro for cleaner error handling during initialization
#define R_CHECK(res_expr)             \
    ({                                \
        const Result rc = (res_expr); \
        if (R_FAILED(rc))             \
        {                             \
            char buf[64];             \
            snprintf(buf, sizeof(buf), "Failed: %s (0x%x)", #res_expr, rc); \
            log_message(buf);         \
        }                             \
    })

bool use_clkrst = false;

#ifdef __cplusplus
extern "C" {
#endif

// --- libnx sysmodule setup ---

u32 __nx_applet_type = AppletType_None;
u32 __nx_fs_num_sessions = 1;

void __libnx_initheap(void)
{
    static u8 inner_heap[INNER_HEAP_SIZE];
    extern void* fake_heap_start;
    extern void* fake_heap_end;

    fake_heap_start = inner_heap;
    fake_heap_end   = inner_heap + sizeof(inner_heap);
}

void log_message(const char* msg) {
    FILE* f = fopen(LOG_FILE, "a");
    if (f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

// Service initialization.
void __appInit(void)
{
    R_CHECK(smInitialize());
    R_CHECK(fsInitialize());
    R_CHECK(fsdevMountSdmc());
    log_message("FS mounted successfully");

    R_CHECK(setsysInitialize());
    log_message("Set system initialized successfully");

    R_CHECK(timeInitialize());
    log_message("time initialized successfully");
    R_CHECK(hidInitialize());
    log_message("HID initialized successfully");
    R_CHECK(hidsysInitialize());
    log_message("HID system initialized successfully");
    R_CHECK(apmInitialize());
    log_message("APM initialized successfully");



    R_CHECK(pmshellInitialize());
    log_message("PM initialized successfully");
    R_CHECK(pminfoInitialize());
    log_message("PM info initialized successfully");

    SetSysFirmwareVersion fw;
    R_CHECK(setsysGetFirmwareVersion(&fw));
    char log_buf[64];
    snprintf(log_buf, sizeof(log_buf), "fw version is %d.%d.%d", fw.major, fw.minor, fw.micro);
    log_message(log_buf);
    hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));

    if (hosversionAtLeast(8, 0, 0)) {
        use_clkrst = true;
        R_CHECK(clkrstInitialize());
        log_message("clkrst initialized successfully");
    } else {
        use_clkrst = false;
        R_CHECK(pcvInitialize());
        log_message("pcv initialized successfully");
    }

    R_CHECK(nsInitialize());
    log_message("NS initialized successfully");

    static const SocketInitConfig socketInitConfig = {
        .tcp_tx_buf_size = 0x800,
        .tcp_rx_buf_size = 0x800,
        .tcp_tx_buf_max_size = 0x25000,
        .tcp_rx_buf_max_size = 0x25000,
        .udp_tx_buf_size = 0,
        .udp_rx_buf_size = 0,
        .sb_efficiency = 1,
    };
    log_message("begin socket initializing initialized successfully");
    R_CHECK(socketInitialize(&socketInitConfig));
    log_message("Socket initialized successfully");

    smExit();
    log_message("sm exited");
}

// Service deinitialization.
void __appExit(void)
{
    socketExit();
    if (use_clkrst) {
        clkrstExit();
    } else {
        pcvExit();
    }
    setsysExit();
    pminfoExit();
    pmshellExit();
    nsExit();
    apmExit();
    hidsysExit();
    hidExit();
    timeExit();
    fsdevUnmountAll();
    fsExit();
}

#ifdef __cplusplus
}
#endif



// Main program entrypoint
int main(int argc, char* argv[])
{
    log_message("Starting sysmodule");

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_message("Failed to create socket");
        return 1;
    }
    log_message("Socket created");

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        log_message("Failed to bind");
        close(server_fd);
        return 1;
    }
    log_message("Bind successful");

    if (listen(server_fd, 3) < 0) {
        log_message("Failed to listen");
        close(server_fd);
        return 1;
    }
    log_message("Listening on port 1234");

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

        char json_buffer[512];

        bool is_sleeping = false;
        Result rc;

        if (use_clkrst) {
            ClkrstSession gpu_session;
            rc = clkrstOpenSession(&gpu_session, PcvModuleId_GPU, 3);
            if (R_FAILED(rc)) {
                // Handle error, assume not sleeping
            } else {
                u32 clock_rate = 0;
                rc = clkrstGetClockRate(&gpu_session, &clock_rate);
                clkrstCloseSession(&gpu_session);

                if (R_SUCCEEDED(rc) && clock_rate == 0) {
                    is_sleeping = true;
                }
            }
        } else {
            u32 clock_rate = 0;
            rc = pcvGetClockRate(PcvModule_GPU, &clock_rate);
            if (R_SUCCEEDED(rc) && clock_rate == 0) {
                is_sleeping = true;
            }
        }

        if (is_sleeping) {
            snprintf(json_buffer, sizeof(json_buffer), "{ \"game_title_id\": null, \"game_name\": \"sleep\" }\n");
        } else {
            u64 pid = 0;
            rc = pmshellGetApplicationProcessIdForShell(&pid);

            if (R_SUCCEEDED(rc) && pid != 0) {
                u64 title_id = 0;
                rc = pminfoGetProgramId(&title_id, pid);
                if (R_SUCCEEDED(rc)) {
                    NsApplicationControlData control_data;
                    size_t bytes_read = 0;
                    rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, title_id, &control_data, sizeof(control_data), &bytes_read);

                    if (R_SUCCEEDED(rc) && bytes_read > 0) {
                        char name_buffer[256] = "Unknown";
                        for (int i = 0; i < 16; i++) {
                            if (control_data.nacp.lang[i].name[0] != '\0') {
                                strncpy(name_buffer, control_data.nacp.lang[i].name, sizeof(name_buffer) - 1);
                                break;
                            }
                        }
                        snprintf(json_buffer, sizeof(json_buffer), "{ \"game_title_id\": \"%016lX\", \"game_name\": \"%s\" }\n",
                                 title_id, name_buffer);
                    } else {
                         snprintf(json_buffer, sizeof(json_buffer), "{ \"game_title_id\": null, \"game_name\": \"On Home Menu\" }\n");
                    }
                } else {
                    snprintf(json_buffer, sizeof(json_buffer), "{ \"game_title_id\": null, \"game_name\": \"On Home Menu\" }\n");
                }
            } else {
                snprintf(json_buffer, sizeof(json_buffer), "{ \"game_title_id\": null, \"game_name\": \"On Home Menu\" }\n");
            }
        }
        send(client_fd, json_buffer, strlen(json_buffer), 0);
        close(client_fd);
    }
    close(server_fd);
    return 0;
}