#include "client.h"

//======================================================================
int wait_read(int fd, int timeout)
{
    struct pollfd fdrd;
    int ret, tm, errno_ = 0;
    
    if (timeout == -1)
        tm = -1;
    else
        tm = timeout * 1000;

    fdrd.fd = fd;
    fdrd.events = POLLIN;
retry:
    ret = poll(&fdrd, 1, tm);
    if (ret == -1)
    {
        errno_ = errno;
        fprintf(stderr, "<%s:%d> Error poll(): %s\n", __func__, __LINE__, strerror(errno));
        if (errno == EINTR)
            goto retry;
        return -errno_;
    }
    else if (!ret)
        return -408;

    if (fdrd.revents & POLLIN)
        return 1;
    else if (fdrd.revents & POLLHUP)
    {
        return 0;
    }

    return -1;
}
//======================================================================
static int wait_write(int fd, int timeout)
{
    int ret, errno_ = 0;
    struct pollfd fdwr;

    fdwr.fd = fd;
    fdwr.events = POLLOUT;
retry:
    ret = poll(&fdwr, 1, timeout * 1000);
    if (ret == -1)
    {
        errno_ = errno;
        //fprintf(stderr, "<%s:%d> Error poll(): %s\n", __func__, __LINE__, strerror(errno));
        if (errno == EINTR)
              goto retry;
        return -errno_;
    }
    else if (!ret)
        return -ETIMEDOUT;

    if (fdwr.revents == POLLOUT)
        return 1;

    //fprintf(stderr, "<%s:%d> Error fdwr.revents = 0x%02x\n", __func__, __LINE__, fdwr.revents);
    return -ECONNRESET;
}
//======================================================================
int read_timeout(int fd, char *buf, int len, int timeout)
{
    int read_bytes = 0, ret, tm, errno_ = 0;
    struct pollfd fdrd;
    char *p;
    
    tm = (timeout == -1) ? -1 : (timeout * 1000);

    fdrd.fd = fd;
    fdrd.events = POLLIN;
    p = buf;
    while (len > 0)
    {
        ret = poll(&fdrd, 1, tm);
        if (ret == -1)
        {
            errno_ = errno;
            fprintf(stderr, "<%s:%d> Error poll(): %s\n", __func__, __LINE__, strerror(errno));
            if (errno == EINTR)
                continue;
            return -errno_;
        }
        else if (!ret)
            return -ETIMEDOUT;

        if (fdrd.revents & POLLERR)
        {
            fprintf(stderr, "<%s:%d> POLLERR fdrd.revents = 0x%02x\n", __func__, __LINE__, fdrd.revents);
            return -__LINE__;
        }
        else if (fdrd.revents & POLLIN)
        {
            ret = read(fd, p, len);
            if (ret == -1)
            {
                errno_ = errno;
                fprintf(stderr, "<%s:%d> Error read(): %s\n", __func__, __LINE__, strerror(errno));
                return -errno_;
            }
            else if (ret == 0)
                break;
            else
            {
                p += ret;
                len -= ret;
                read_bytes += ret;
            }
        }
        else if (fdrd.revents & POLLHUP)
        {
            break;
        }
    }

    return read_bytes;
}
//======================================================================
int write_timeout(int fd, const char *buf, int len, int timeout)
{
    int write_bytes = 0, ret, errno_ = 0;

    while (len > 0)
    {
        ret = wait_write(fd, timeout);
        if (ret <= 0)
            return ret;

        ret = write(fd, buf, len);
        if (ret == -1)
        {
            errno_ = errno;
            //fprintf(stderr, "<%s:%d> Error write(): %s\n", __func__, __LINE__, strerror(errno));
            if (errno == EINTR)
                continue;
            return -errno_;
        }

        write_bytes += ret;
        len -= ret;
        buf += ret;
    }

    return write_bytes;
}
//======================================================================
int read_to_space(int fd_in, char *buf, long sizeBuf, long *size, int timeout)
{
    int rd, allRD = 0, errno_ = 0;

    for ( ; *size > 0; )
    {
        rd = wait_read(fd_in, timeout);
        if (rd < 0)
        {
            return rd;
        }
        
        if (*size > sizeBuf)
            rd = read(fd_in, buf, sizeBuf);
        else
            rd = read(fd_in, buf, *size);
        if (rd == -1)
        {
            errno_ = errno;
            if (errno == EINTR)
                continue;
            fprintf(stderr, "   Error read_to_space(%d,): %s\n", fd_in, strerror(errno));
            return -errno_;
        }
        else if (rd == 0)
        {
            return 0;
        }

        *size -= rd;
        allRD += rd;
    }

    return allRD;
}
//======================================================================
int read_to_space2(int fd_in, char *buf, long size, int timeout)
{
    int rd, allRD = 0, errno_ = 0;

    for ( ; ; )
    {
        rd = wait_read(fd_in, timeout);
        if (rd < 0)
        {
            return rd;
        }
        
        rd = read(fd_in, buf, size);
        if (rd == -1)
        {
            errno_ = errno;
            if (errno == EINTR)
                continue;
            fprintf(stderr, "   Error read_to_space2(%d,): %s\n", fd_in, strerror(errno));
            return -errno_;
        }
        else if (rd == 0)
        {
            break;
        }

        allRD += rd;
    }

    return allRD;
}
//======================================================================
int read_line_sock(int fd, char *buf, int size, int timeout)
{   
    int ret, n, read_bytes = 0;
    char *p = NULL;

    for ( ; size > 0; )
    {
        ret = wait_read(fd, timeout);
        if (ret < 0)
            return ret;

        ret = recv(fd, buf, size, MSG_PEEK);
        if (ret > 0)
        {
            if ((p = memchr(buf, '\n', ret)))
            {
                n = p - buf + 1;
                ret = recv(fd, buf, n, 0);
                if (ret <= 0)
                {
                    return -errno;
                }
                return read_bytes + ret;
            }
            n = recv(fd, buf, ret, 0);
            if (n != ret)
            {
                return -__LINE__;
            }
            buf += n;
            size -= n;
            read_bytes += n;
        }
        else // ret <= 0
        {
            return -errno;
        }
    }

    return -414;
}
//======================================================================
int read_headers_to_stdout(response *resp)
{
    int ret, read_bytes = 0, i = 0;

    for ( ; ; ++i)
    {
        ret = read_line_sock(resp->servSocket, 
                            resp->buf, 
                            sizeof(resp->buf) - 1,
                            resp->timeout);
        if (ret <= 0)
            goto exit_;

        read_bytes += ret;
        *(resp->buf + ret) = 0;
        printf("%s", resp->buf);
        if (i == 0)
            sscanf(resp->buf, "%*s %d %*s", &resp->respStatus);
        
        ret = strcspn(resp->buf, "\r\n");
        if (ret == 0)
        {
            ret = read_bytes;
            goto exit_;
        }
    }
    fprintf(stderr, "<%s:%d>  Error read_headers(): ?\n", __func__, __LINE__);

exit_:
    return ret;
}
//======================================================================
int read_headers(response *resp)
{
    int ret, read_bytes = 0;

    ret = read_line_sock(resp->servSocket, 
                            resp->buf, 
                            sizeof(resp->buf) - 1,
                            resp->timeout);
    if (ret <= 0)
    {
        goto exit_;
    }

    resp->buf[ret] = 0;
    sscanf(resp->buf, "%*s %d %*s", &resp->respStatus);
    
    for (; ; )
    {
        ret = read_line_sock(resp->servSocket, 
                            resp->buf, 
                            sizeof(resp->buf) - 1,
                            resp->timeout);

        if (ret <= 0)
        {
            goto exit_;
        }

        read_bytes += ret;
        *(resp->buf + ret) = 0;

        ret = strcspn(resp->buf, "\r\n");
        if (ret == 0)
        {
            ret = read_bytes;
            goto exit_;
        }

        ret = parse_headers(resp);
        if (ret < 0)
        {
            fprintf(stderr, "<%s:%d>  Error parse_headers(): %d\n", __func__, __LINE__, ret);
            goto exit_;
        }
    }
    fprintf(stderr, "<%s:%d>  Error read_headers(): ?\n", __func__, __LINE__);

exit_:

    return ret;
}
//======================================================================
int read_chunk(response *resp)
{
    int allRD = 0;

    while (1)
    {
        long len_chunk;
        int ret = read_line_sock(resp->servSocket, 
                            resp->buf, 
                            sizeof(resp->buf) - 1,
                            resp->timeout);
        if (ret <= 0)
        {
            return ret;
        }

        resp->buf[ret] = 0;
        allRD += ret;
        
        if (sscanf(resp->buf, "%lx", &len_chunk) != 1)
        {
            return -__LINE__;
        }

        if (len_chunk == 0)
        {
            ret = read_timeout(resp->servSocket, resp->buf, 2, resp->timeout);
            if (ret <= 0)
            {
                return ret;
            }
            allRD += ret;
            break;
        }

        ret = read_to_space(resp->servSocket, resp->buf, sizeof(resp->buf), &len_chunk, resp->timeout);
        if (ret <= 0)
        {
            return ret;
        }
        allRD += ret;
        
        ret = read_timeout(resp->servSocket, resp->buf, 2, resp->timeout);
        if (ret <= 0)
        {
            return ret;
        }
        allRD += ret;
    }
    
    return allRD;
}
//======================================================================
int read_line(FILE *f, char *s, int size)
{
    char *p = s;
    int ch, len = 0, wr = 1;

    while (((ch = getc(f)) != EOF) && (len < size))
    {
        if (ch == '\n')
        {
            *p = 0;
            if (wr == 0)
            {
                wr = 1;
                continue;
            }
            return len;
        }
        else if (wr == 0)
            continue;
        else if (ch == '#')
        {
            wr = 0;
        }
        else if (ch != '\r')
        {
            *(p++) = (char)ch;
            ++len;
        }
    }
    *p = 0;

    return len;
}
//======================================================================
int read_req_file_(FILE *f, char *req, int size)
{
    *req = 0;
    char *p = req;
    int len = 0, read_startline = 0;

    while (len < size)
    {
        int n = read_line(f, p, size - len);
        if (n > 0)
        {
            len += n;
            int m = strlen(end_line);
            if ((len + m) < size)
            {
                if (read_startline == 0)
                {
                    if (sscanf(p, "%15s %1023s %*s", Method, Uri) != 2)
                        return -4;
                    read_startline = 1;
                }
                else
                {
                    if (strstr_case(p, "Connection"))
                    {
                        if (strstr_case(p + 11, "close"))
                            ConnKeepAlive = 0;
                        else
                            ConnKeepAlive = 1;
                    }
                    else if (strstr_case(p, "Host:"))
                    {
                        if (sscanf(p, "Host: %127s", Host) != 1)
                        {
                            fprintf(stderr, "Error: [%s]\n", p);
                            return -1;
                        }
                    }
                }

                strcat(p, end_line);
                len += m;
                p += (n + m);
            }
            else
                return -1;
        }
        else if (n == 0)
        {
            if (feof(f))
                return len;

            if (read_startline == 0)
            {
                int m = strlen(end_line);
                if ((len + m) < size)
                {
                    memcpy(p, end_line, m + 1);
                    len += m;
                    p += m;
                }
                else
                    return -3;
            }
            else
            {
                int m = strlen(end_line);
                if ((len + m) < size)
                {
                    memcpy(p, end_line, m + 1);
                    len += m;
                    p += m;
                    if (strcmp(Method, "POST"))
                        return len;
                    else
                    {
                        int ret;
                        while (len < size)
                        {
                            if ((ret = fread(p, 1, size - len - 1, f)) <= 0)
                                return len;
                            len += ret;
                            p += ret;
                        }
                        *p = 0;
                        if (feof(f))
                            return len;
                        else
                            return -1;
                    }
                }
            }
        }
        else
            return n;
    }
    
    if (feof(f))
        return len;
    return -1;
}
//======================================================================
int read_req_file(const char *path, char *req, int size)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, " Error open request file(%s): %s\n", path, strerror(errno));
        return -1;
    }

    int n = read_req_file_(f, req, size);
    if (n <= 0)
    {
        fprintf(stderr, "<%s> Error read_req_file()=%d\n", __func__, n);
        fclose(f);
        return -1;
    }

    fclose(f);
    return n;
}
    
    
