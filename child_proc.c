#include "client.h"

//======================================================================
void get_time_connect(struct timeval *time1, char *buf, int size_buf)
{
    unsigned long ts12, tu12;
    struct timeval time2;

    gettimeofday(&time2, NULL);

    if ((time2.tv_usec-time1->tv_usec) < 0)
    {
        tu12 = (1000000 + time2.tv_usec) - time1->tv_usec;
        ts12 = (time2.tv_sec - time1->tv_sec) - 1;
    }
    else
    {
        tu12 = time2.tv_usec - time1->tv_usec;
        ts12 = time2.tv_sec - time1->tv_sec;
    }

    snprintf(buf, size_buf, "Time: %lu.%06lu sec", ts12, tu12);
}
//======================================================================
int child_proc(int numProc, const char *buf_req)
{
    struct timeval time1;
    int n;
    char s[256];

    struct rlimit lim;
    if (getrlimit(RLIMIT_NOFILE, &lim) == -1)
    {
        fprintf(stderr, "<%s:%d> Error getrlimit(RLIMIT_NOFILE): %s\n", __func__, __LINE__, strerror(errno));
    }
    else
    {
        if ((NumThreads + 5) > (long)lim.rlim_cur)
        {
            if ((NumThreads + 5) <= (long)lim.rlim_max)
            {
                lim.rlim_cur = NumThreads + 5;
                setrlimit(RLIMIT_NOFILE, &lim);
            }
            else
            {
                MaxThreads = (long)lim.rlim_max - 5;
            }
        }
    }

    time_t now;
    time(&now);
    printf("[%d] pid: %d,  %s", numProc, getpid(), ctime(&now));

    init_count_thr();

    int i = 0;
gettimeofday(&time1, NULL);
    while (i < NumThreads)
    {
        request *req;
        req = malloc(sizeof(request));
        if (!req)
        {
            fprintf(stderr, "<%s:%d> Error malloc(): %s\n", __func__, __LINE__, strerror(errno));
            return 1;
        }

        memset(req, 0, sizeof(request));
        req->req = buf_req;
        req->num_proc = numProc;
        req->num_thr = i;
        req->all_requests = NumRequests;
        req->connKeepAlive = ConnKeepAlive;
        req->timeout = Timeout;
    //snprintf(req->host, sizeof(req->host), "%s", Host);
        snprintf(req->ip, sizeof(req->ip), "%s", IP);
        snprintf(req->port, sizeof(req->port), "%s", Port);
        
        if (ai_family == AF_INET)
            req->create_sock = create_client_socket_ip4;
        else if (ai_family == AF_INET6)
            req->create_sock = create_client_socket_ip6;
        else
        {
            return 1;
        }

        int n_thr;
        pthread_t thr;

        n_thr = get_num_thr();
        if (MaxThreads && (n_thr >= MaxThreads))
            wait_thr_exit(n_thr);

        n = pthread_create(&thr, NULL, get_request, req);
        if (n)
        {
            if ((n == EAGAIN) || (n == ENOMEM)) // 11
            {
                if (n_thr > 0)
                    wait_thr_exit(n_thr);
                else
                {
                    fprintf(stderr, "[%d]<%s> MaxThreads=%d, num_thr=%d\n", numProc, __func__, MaxThreads, n_thr);
                    usleep(10000);
                }
                free(req);
                continue;
            }

            fprintf(stderr, "<%s> Error pthread_create(): %d; %s\n", __func__, n, strerror(n));
            free(req);
            return 1;
        }

        n = pthread_detach(thr);
        if (n)
        {
            fprintf(stderr, "<%s> Error pthread_detach(): %d\n", __func__, n);
            free(req);
            return 1;
        }

        thr_start_();
        ++i;
    }

    wait_exit_all_thr();

get_time_connect(&time1, s, sizeof(s));

    Error err = get_err();

    printf("-[%d] all_thr=%d, max_thr=%d, %s, all_request=%d\n"
           "   err_sock=%d(%d), err_wr=%d(%d), err_rd=%d(%d)\n"
           "   all read = %lld\n", numProc, i, get_max_thr(), s, get_all_req(),
                                 err.num_err_sock, err.errno_sock, 
                                 err.num_err_wr, -err.errno_wr, err.num_err_rd, -err.errno_rd,
                                 get_all_read());

    return 0;    
}
