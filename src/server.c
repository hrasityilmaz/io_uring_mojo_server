#include <liburing.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define QUEUE_DEPTH 256
#define BUFFER_SIZE 2048

enum op_type {
    OP_ACCEPT,
    OP_RECV,
    OP_SEND
};

struct conn_info {
    enum op_type type;
    int fd;
    int len;
    char buffer[BUFFER_SIZE];
};

static void submit_accept(struct io_uring *ring, int server_fd) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        fprintf(stderr, "submit_accept: sqe none !\n");
        exit(1);
    }

    struct conn_info *info = malloc(sizeof(*info));
    if (!info) {
        perror("malloc");
        exit(1);
    }

    memset(info, 0, sizeof(*info));
    info->type = OP_ACCEPT;
    info->fd = server_fd;

    io_uring_prep_accept(sqe, server_fd, NULL, NULL, 0);
    io_uring_sqe_set_data(sqe, info);
}

static void submit_recv(struct io_uring *ring, int client_fd) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        fprintf(stderr, "submit_recv: sqe none !\n");
        close(client_fd);
        return;
    }

    struct conn_info *info = malloc(sizeof(*info));
    if (!info) {
        perror("malloc");
        close(client_fd);
        return;
    }

    memset(info, 0, sizeof(*info));
    info->type = OP_RECV;
    info->fd = client_fd;

    io_uring_prep_recv(sqe, client_fd, info->buffer, BUFFER_SIZE, 0);
    io_uring_sqe_set_data(sqe, info);
}

static void submit_send(struct io_uring *ring, int client_fd, const char *buf, int len) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        fprintf(stderr, "submit_send: sqe none!\n");
        close(client_fd);
        return;
    }

    struct conn_info *info = malloc(sizeof(*info));
    if (!info) {
        perror("malloc");
        close(client_fd);
        return;
    }

    memset(info, 0, sizeof(*info));
    info->type = OP_SEND;
    info->fd = client_fd;
    info->len = len;
    memcpy(info->buffer, buf, len);

    io_uring_prep_send(sqe, client_fd, info->buffer, len, 0);
    io_uring_sqe_set_data(sqe, info);
}

// Mojo burayi cagiracak
int start_echo_server(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    int one = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return -2;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return -3;
    }

    if (listen(server_fd, 512) < 0) {
        perror("listen");
        close(server_fd);
        return -4;
    }

    struct io_uring ring;
    int ret = io_uring_queue_init(QUEUE_DEPTH, &ring, 0);
    if (ret < 0) {
        fprintf(stderr, "io_uring_queue_init failed: %d\n", ret);
        close(server_fd);
        return ret;
    }

    printf("server listening on 0.0.0.0:%d\n", port);
    fflush(stdout);

    submit_accept(&ring, server_fd);

    for (;;) {
        ret = io_uring_submit(&ring);
        if (ret < 0) {
            fprintf(stderr, "io_uring_submit failed: %d\n", ret);
            break;
        }

        struct io_uring_cqe *cqe;
        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) {
            fprintf(stderr, "io_uring_wait_cqe failed: %d\n", ret);
            break;
        }

        struct conn_info *info = io_uring_cqe_get_data(cqe);
        int res = cqe->res;

        if (!info) {
            io_uring_cqe_seen(&ring, cqe);
            continue;
        }

        switch (info->type) {
            case OP_ACCEPT: {
                int client_fd = res;

                free(info);
                io_uring_cqe_seen(&ring, cqe);

                submit_accept(&ring, server_fd);

                if (client_fd >= 0) {
                    submit_recv(&ring, client_fd);
                } else {
                    fprintf(stderr, "accept failed: %d\n", client_fd);
                }
                break;
            }

            case OP_RECV: {
                int client_fd = info->fd;

                if (res <= 0) {
                    if (res < 0) {
                        fprintf(stderr, "recv failed fd=%d res=%d\n", client_fd, res);
                    }
                    close(client_fd);
                    free(info);
                    io_uring_cqe_seen(&ring, cqe);
                    break;
                }

                int n = res;
                submit_send(&ring, client_fd, info->buffer, n);

                free(info);
                io_uring_cqe_seen(&ring, cqe);
                break;
            }

            case OP_SEND: {
                int client_fd = info->fd;

                if (res < 0) {
                    fprintf(stderr, "send failed fd=%d res=%d\n", client_fd, res);
                    close(client_fd);
                } else {
                    submit_recv(&ring, client_fd);
                }

                free(info);
                io_uring_cqe_seen(&ring, cqe);
                break;
            }

            default:
                free(info);
                io_uring_cqe_seen(&ring, cqe);
                break;
        }
    }

    close(server_fd);
    io_uring_queue_exit(&ring);
    return 0;
}