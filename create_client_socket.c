#include "client.h"

//======================================================================
int connect_timeout(int sock, struct sockaddr *addr, int size_)
{
    if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
    {
        fprintf(stderr, "   <%s> Error fcntl(fd, F_SETFL, O_NONBLOCK): %s\n", __func__, strerror(errno));
        close(sock);
        return -1;
    }

    if (connect(sock, addr, size_) != 0)
    {
        if (errno == EINPROGRESS)
        {
            if (ConnTimeout == 0)
                return 0;

            struct pollfd pfds[] = { { .fd = sock, .events = POLLOUT } };
            int ret = poll(pfds, 1, ConnTimeout);
            if (ret == -1)
                return -errno;
            else if (ret == 0)
            {
                return -ETIMEDOUT;
            }

            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len))
            {
                fprintf(stderr, "   <%s> Error getsockopt(): %s\n", __func__, strerror(errno));
                return -errno;
            }

            if(error != 0)
            {
                fprintf(stderr, "   <%s> Error connect(): %s\n", __func__, strerror(error));
                return -error;
            }
        }
        else
        {
            return -errno;
        }
    }

    return 0;
}
//======================================================================
int create_client_socket(const char *host, const char *port)
{
    int sockfd, n;
    struct addrinfo hints, *res = NULL, *p;
    const int sock_opt = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ((n = getaddrinfo(host, port, &hints, &res)) != 0)
    {
        fprintf(stderr, "Error getaddrinfo(%s:%s): %s\n", host, port, gai_strerror(n));
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0)
            break;

        close(sockfd);
    }

    if (p == NULL)
    {
        fprintf(stderr, "Error connect[%s:%s]\n" , host, port);
        freeaddrinfo(res);
        return -1;
    }

    if ((n = getnameinfo((struct sockaddr *)p->ai_addr, 
                p->ai_addrlen, 
                IP, 
                sizeof(IP),
                NULL, 
                0, 
                NI_NUMERICHOST)))
    {
        fprintf(stderr, "<%s> Error getnameinfo(): %s\n", __func__, gai_strerror(n));
        return -1;
    }

    ai_family = p->ai_family;

    freeaddrinfo(res);

    ioctl(sockfd, FIONBIO, &sock_opt);

    return sockfd;
}
//======================================================================
int create_client_socket_ip4(const char *ip, const char *port)
{
    int sockfd, n;
    struct sockaddr_in sin4;
    int sock_opt = 1;

    memset(&sin4, 0, sizeof(sin4));

    sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd == -1)
    {
        fprintf(stderr, "<%s> Error socket(): %s\n", __func__, strerror(errno));
        return -1;
    }

    sin4.sin_port = htons(atoi(port));
    sin4.sin_family = AF_INET;

    if (inet_pton(PF_INET, ip, &(sin4.sin_addr)) < 1)
    {
        fprintf(stderr, "<%s> Error inet_pton [%s]: %s\n", __func__, ip, strerror(errno));
        return -1;
    }

    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void *)&sock_opt, sizeof(sock_opt));

    if ((n = connect_timeout(sockfd, (struct sockaddr *)(&sin4), sizeof(sin4))))
    {
        close(sockfd);
        return n;
    }

    return sockfd;
}
//======================================================================
int create_client_socket_ip6(const char * host, const char *port)
{
    int sockfd, n;
    struct sockaddr_in6 sin6;
    const int sock_opt = 1;

    memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = PF_INET6;
    inet_pton (PF_INET6, host, sin6.sin6_addr.s6_addr);
    sin6.sin6_port = htons(atoi(port));

    sockfd = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd == -1) 
    {
        fprintf(stderr, "<%s> Error socket(): %s\n", __func__, strerror(errno));
        return -1;
    }

    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,(void *) &sock_opt, sizeof(int));

    if ((n = connect_timeout(sockfd, (struct sockaddr *)(&sin6), sizeof(sin6))))
    {
        close(sockfd);
        return n;
    }

    return sockfd;
}
//======================================================================
const char *get_str_ai_family(int ai_family)
{
    switch (ai_family)
    {
        case AF_UNSPEC:
            return "AF_UNSPEC";
        case AF_UNIX:
            return "AF_UNIX";
        case AF_INET:
            return "AF_INET";
        case AF_INET6:
            return "AF_INET6";
        default:
            return "?";
    }
    return "?";
}
//======================================================================
int get_int_ai_family(const char *str_family)
{
    if (!strcmp(str_family, "AF_UNSPEC"))
        return AF_UNSPEC;
    else if (!strcmp(str_family, "AF_UNIX"))
        return AF_UNIX;
    else if (!strcmp(str_family, "AF_INET"))
        return AF_INET;
    else if (!strcmp(str_family, "AF_INET6"))
        return AF_INET6;
    else
        return -1;
}
