#ifndef CLIENT_H
#define CLIENT_H

#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/wait.h>

extern char Host[128];
extern char IP[256];
extern int ai_family;
extern char Port[32];
extern char end_line[8];
extern int MaxThreads;
extern int NumThreads;
extern int NumRequests;
extern int Timeout;
extern char Method[16];
extern char Uri[1024];
extern int ConnKeepAlive;

typedef struct {
    int  num_thr;
    int  num_requests;
    int  connKeepAlive;
    int  timeout;
    char ip[256];
    char port[32];
    const char *req;
    int (*create_sock)(const char*, const char*);
} request;
// 16284 32768 65536 
typedef struct {
    int       servSocket;
    int       timeout;
    char      buf[32768];
    char      server[128];
    int       connKeepAlive;
    int       chunk;
    int       respStatus;
    long len;
} response;

typedef struct {
    int num_err_sock;
    int errno_sock;
    
    int num_err_wr;
    int errno_wr;
    
    int num_err_rd;
    int errno_rd;
} Error;
//----------------------------------------------------------------------
int child_proc(int, const char*);
//----------------------------------------------------------------------
void init_count_thr(void);
int get_good_conn(void);
int get_max_thr(void);
int get_num_thr(void);
void thr_start_(void);
void wait_exit_all_thr(void);
int wait_thr_exit(int n);
Error get_err();
//----------------------------------------------------------------------
int read_headers_to_stdout(response *resp);
int read_headers(response *req);
int read_to_space(int fd_in, char *buf, long size_buf, long *size, int timeout);
int read_to_space2(int fd_in, char *buf, long size, int timeout);
int read_timeout(int fd, char *buf, int len, int timeout);
int write_timeout(int fd, const char *buf, int len, int timeout);
void *get_request(void *arg);
int read_chunk(response *resp);
long long get_all_read(void);
int read_req_file(const char *path, char *req, int size);
//----------------------------------------------------------------------
int create_client_socket(const char * host, const char *port);
int create_client_socket_ip4(const char *ip, const char *port);
int create_client_socket_ip6(const char *ip, const char *port);
int get_ip(int sock, char *ip, int size_ip);
const char *get_str_ai_family(int ai_family);
//----------------------------------------------------------------------
void std_in(char *s, int len);
const char *strstr_case(const char *s1, const char *s2);
int strcmp_case(const char *s1, const char *s2);
int parse_headers(response *resp);

#endif
