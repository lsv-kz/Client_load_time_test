#include "client.h"

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t waitExitThr = PTHREAD_COND_INITIALIZER;
pthread_cond_t connClose = PTHREAD_COND_INITIALIZER;
static int count_thr, max_thr = 0, all_req = 0;
static long long allRD = 0;
//======================================================================
void init_count_thr(void)
{
    count_thr = max_thr = 0;
    allRD = 0;
}
//======================================================================
void inc_good_req(void)
{
pthread_mutex_lock(&mutex1);
    ++all_req;
pthread_mutex_unlock(&mutex1);
}
//======================================================================
void add_all_read(int n)
{
pthread_mutex_lock(&mutex1);
    allRD += n;
pthread_mutex_unlock(&mutex1);
}
//======================================================================
int get_all_req(void)
{
    return all_req;
}
//======================================================================
int get_max_thr(void)
{
    return max_thr;
}
//======================================================================
long long get_all_read(void)
{
    return allRD;
}
//======================================================================
int get_num_thr(void)
{
pthread_mutex_lock(&mutex1);
    int ret = count_thr;
pthread_mutex_unlock(&mutex1);
    return ret;
}
//======================================================================
void thr_start_(void)
{
pthread_mutex_lock(&mutex1);
    ++count_thr;
    if (count_thr > max_thr)
        max_thr = count_thr;
pthread_mutex_unlock(&mutex1);
}
//======================================================================
void thr_exit_(void)
{
pthread_mutex_lock(&mutex1);
    --count_thr;
pthread_mutex_unlock(&mutex1);
    pthread_cond_broadcast(&waitExitThr);
}
//======================================================================
void conn_close(void)
{
    pthread_cond_signal(&connClose);
}
//======================================================================
void wait_exit_all_thr(void)
{
pthread_mutex_lock(&mutex1);
    while (count_thr)
    {
        pthread_cond_wait(&waitExitThr, &mutex1);
    }
pthread_mutex_unlock(&mutex1);
}
//======================================================================
int wait_thr_exit(int n)
{
pthread_mutex_lock(&mutex1);
    while (n == count_thr)
    {
        pthread_cond_wait(&waitExitThr, &mutex1);
    }
    n = count_thr;
pthread_mutex_unlock(&mutex1);
    return n;
}
//======================================================================
Error err = {0,0,0,0,0,0};
Error get_err()
{
    return err;
}
//======================================================================
void get_request_(const request *req, response *resp)
{
    int num_req = 0, n;

    if (req->connKeepAlive == 1)
    {
        resp->servSocket = req->create_sock(req->ip, req->port);
        if (resp->servSocket < 0)
        {
            err.errno_sock = resp->servSocket;
            err.num_err_sock++;
            return;
        }
    }

    for ( ; (num_req < req->all_requests); ++num_req)
    {
        if (req->connKeepAlive == 0)
        {
            resp->servSocket = req->create_sock(req->ip, req->port);
            if (resp->servSocket < 0)
            {
                err.errno_sock = resp->servSocket;
                err.num_err_sock++;
                break;
            }
        }

        n = write_timeout(resp->servSocket, req->req, strlen(req->req), req->timeout);
        if (n <= 0)
        {
            err.errno_wr = n;
            err.num_err_wr++;
            shutdown(resp->servSocket, SHUT_RDWR);
            close(resp->servSocket);
            if (req->connKeepAlive == 1)
                break;

            continue;
        }

        resp->chunk = 0;
        resp->len = -1;
        n = read_headers(resp);
        if (n <= 0)
        {
            err.errno_rd = n;
            err.num_err_rd++;
            shutdown(resp->servSocket, SHUT_RDWR);
            close(resp->servSocket);
            if (req->connKeepAlive == 1)
                break;

            continue;
        }

        if ((resp->respStatus != 206) && (resp->respStatus != 200))
        {
            fprintf(stderr, "*** Status: %d ***\n", resp->respStatus);
            shutdown(resp->servSocket, SHUT_RDWR);
            close(resp->servSocket);
            break;
        }

        if (!strcmp(Method, "HEAD"))
        {
            inc_good_req();
            
            if ((num_req >= (req->all_requests - 1)) || (req->connKeepAlive == 0))
            {
                shutdown(resp->servSocket, SHUT_RDWR);
                close(resp->servSocket);
            }
            continue;
        }

        if (resp->chunk)
        {
            n = read_chunk(resp);
            if (n <= 0)
            {
                err.errno_rd = n;
                err.num_err_rd++;
                shutdown(resp->servSocket, SHUT_RDWR);
                close(resp->servSocket);
                if (req->connKeepAlive == 1)
                    break;

                continue;
            }
        }
        else
        {
            if (resp->len == -1)
            {
                n = read_to_space2(resp->servSocket, resp->buf, sizeof(resp->buf), req->timeout);
            }
            else
            {
                long read_len = resp->len;
                n = read_to_space(resp->servSocket, resp->buf, sizeof(resp->buf), &read_len, req->timeout);
            }

            if (n < 0)
            {
                err.errno_rd = n;
                err.num_err_rd++;
                shutdown(resp->servSocket, SHUT_RDWR);
                close(resp->servSocket);
                if (req->connKeepAlive == 1)
                    break;

                continue;
            }
        }

        if (n > 0)
        {
            add_all_read(n);
            inc_good_req();
        }

        if ((num_req >= (req->all_requests - 1)) || (req->connKeepAlive == 0))
        {
            shutdown(resp->servSocket, SHUT_RDWR);
            close(resp->servSocket);
        }
    }

    return;
}
//======================================================================
void *get_request(void *arg)
{
    request *req = (request*)arg;
    response resp;

    resp.timeout = req->timeout;
    get_request_(req, &resp);

    thr_exit_();
    return NULL;
}
