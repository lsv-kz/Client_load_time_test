#include "client.h"

char Host[128] = "0.0.0.0";
char IP[256];
int ai_family;
char Port[32] = "80";
int Timeout = 90;

int MaxThreads = 380;
int NumThreads = 1;
int NumRequests = 1;

char Method[16];
char Uri[1024];
int ConnKeepAlive = 1;

int ConnTimeout = 1000;

const char *end_line = "\r\n";
//======================================================================
int is_number(const char *s)
{
    if (!s)
        return 0;
    int n = isdigit((int)*(s++));
    while (*s && n)
        n = isdigit((int)*(s++));
    return n;
}
//======================================================================
int read_conf_file()
{
    int ret = 0;
    char *p1, *p2, s[256];
    FILE *f = fopen("conf/config.txt", "r");
    if (!f)
    {
        printf(" Error open config file: %s\n", strerror(errno));
        return -1;
    }

    while (fgets(s,sizeof(s), f))
    {
        if ((p1 = strpbrk(s, "\r\n")))
            *p1 = 0;
        p1 = s;
        while ((*p1 == ' ') || (*p1 == '\t'))
            p1++;

        if (*p1 == '#' || *p1 == 0)
            continue;
        else
        {
            if ((p2 = strchr(s, '#')))
                *p2 = 0;
        }

        if (sscanf(p1, " MaxThreads %d", &MaxThreads) == 1)
        {
            printf("MaxThreads: %d\n", MaxThreads);
            continue;
        }
        else if (sscanf(p1, " Timeout %d", &Timeout) == 1)
        {
            printf("Timeout: %d\n", Timeout);
            continue;
        }
        else if (sscanf(p1, " ConnTimeout %d", &ConnTimeout) == 1)
        {
            printf("ConnTimeout: %d\n", ConnTimeout);
            continue;
        }
        else
        {
            printf("Error unknown directive: %s\n", p1);
            ret = 1;
            break;
        }
    }

    fclose(f);

    return ret;
}
//======================================================================
int main(int argc, char *argv[])
{
    int n;
    char s[256], path[512];
    char buf_req[1024], first_req[1500];
    int numProc = 1;
    int run_ = 1;

    printf(" %s\n\n", argv[0]);
    signal(SIGPIPE, SIG_IGN);

    while (run_)
    {
        printf("============================================\n"
               "Input [Name request file] or [q: Exit]\n>>> conf/");
        fflush(stdout);
        std_in(s, sizeof(s));
        if (s[0] == 'q')
            break;

        snprintf(path, sizeof(path), "conf/%s", s);

        printf("------------- conf/config.txt --------------\n");
        if (read_conf_file())
            continue;

        if ((n = read_req_file(path, buf_req, sizeof(buf_req))) <= 0)
            continue;

        printf("-------------- %s ------------------\n%s", path, buf_req);
        printf("\n--------------------------------------------\nServer Port: ");
        fflush(stdout);
        std_in(Port, sizeof(Port));
        if (Port[0] == 'q')
            break;
        if (Port[0] == 'c')
            continue;

        if (is_number(Port) == 0)
        {
            fprintf(stderr, "!!!   Error [Server Port: %s]\n", Port);
            continue;
        }
        //--------------------------------------------------------------
        printf("Num Processes: ");
        fflush(stdout);
        std_in(s, sizeof(s));
        if (s[0] == 'q')
            break;
        if (s[0] == 'c')
            continue;
        if (sscanf(s, "%d", &numProc) != 1)
        {
            fprintf(stderr, "!!!   Error [Num Processes: %s]\n", s);
            continue;
        }

        if (numProc > 16)
        {
            fprintf(stderr, "!!!   Error [Num Processes > 16]\n");
            continue;
        }
        //--------------------------------------------------------------
        printf("Num Threads: ");
        fflush(stdout);
        std_in(s, sizeof(s));
        if (s[0] == 'q')
            break;
        if (s[0] == 'c')
            continue;
        if (sscanf(s, "%d", &NumThreads) != 1)
        {
            fprintf(stderr, "!!!  Error [Num Threads: %s]\n", s);
            continue;
        }
        //--------------------------------------------------------------
        printf("Num Requests: ");
        fflush(stdout);
        std_in(s, sizeof(s));
        if (s[0] == 'q')
            break;
        if (s[0] == 'c')
            continue;
        if (sscanf(s, "%d", &NumRequests) != 1)
        {
            fprintf(stderr, "!!!   Error [Num Requests: %s]\n", s);
            continue;
        }
        //--------------------------------------------------------------
        time_t now;
        time(&now);
        printf("%s\n", ctime(&now));

        int servSocket = create_client_socket(Host, Port);
        if (servSocket == -1)
        {
            fprintf(stderr, "<%s:%d> Error: create_client_socket(%s:%s)\n", __func__, __LINE__, Host, Port);
            continue;
        }

        if ((ai_family != AF_INET) && (ai_family != AF_INET6))
        {
            fprintf(stderr, "<%s:%d> Error: ai_family: %s\n", __func__, __LINE__, get_str_ai_family(ai_family));
            continue;
        }

        printf("IP: %s, FAMILY: %s\n", IP, get_str_ai_family(ai_family));

        snprintf(first_req, sizeof(first_req), "HEAD %s HTTP/1.1\r\n"
                                                "Host: %s\r\n"
                                                "User-Agent: ???\r\n"
                                                "Connection: close\r\n"
                                                "\r\n", Uri, Host);
        n = write_timeout(servSocket, first_req, strlen(first_req), Timeout);
        if (n < 0)
        {
            fprintf(stderr, "<%s:%d> Error send request\n", __func__, __LINE__);
            shutdown(servSocket, SHUT_RDWR);
            close(servSocket);
            continue;
        }
        printf("--------------------------------------------\n"
               "%s"
               "--------------------------------------------\n", first_req);
        response resp;
        resp.servSocket = servSocket;
        resp.timeout = Timeout;

        n = read_headers_to_stdout(&resp);
        shutdown(servSocket, SHUT_RDWR);
        close(servSocket);
        if (n <= 0)
        {
            fprintf(stderr, "<%s:%d> Error read headers: %d\n", __func__, __LINE__, n);
            continue;
        }

        printf("*************** Status: %d ****************\n", resp.respStatus);
        if (resp.respStatus >= 300)
            continue;
        //--------------------------------------------------------------
        int num = 0;

        while (num < numProc)
        {
            pid_t chld;
            chld = fork();
            if (chld == 0)
            {
                child_proc(num, buf_req);
                exit(0);
            }
            else if (chld < 0)
            {
                fprintf(stderr, "<%s:%d> Error fork(): %s\n", __func__, __LINE__, strerror(errno));
                exit(1);
            }

            num++;
        }

        while (wait(NULL) != -1);
        time(&now);
        printf("\n%s", ctime(&now));
    }

    return 0;
}
