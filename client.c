#include "client.h"

char Host[128] = "0.0.0.0";
char IP[256];
int ai_family;
char Port[32] = "80";
char end_line[8] = "\r\n";
int Timeout = 90;

int MaxThreads = 380;
int NumThreads = 1;
int NumRequests = 1;

char method[16];
int connKeepAlive = 1;
//======================================================================
int read_conf_file()
{
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
        else if (sscanf(p1, " EndLine %7s", end_line) == 1)
        {
            printf("end_line: %s\n", end_line);
            if (!strcmp(end_line, "lf"))
                strcpy(end_line, "\n");
            else
                strcpy(end_line, "\r\n");
        }
    }

    fclose(f);

    if (strcmp(end_line, "\n") && strcmp(end_line, "\r\n"))
    {
        fprintf(stderr, " Error end_line ?\n");
        return -1;
    }
    
    printf("\n");

    return 0;
}
//======================================================================
int main(int argc, char *argv[])
{
    int n;
    char s[256], path[512];
    char buf_req[1024];
    int numProc = 1;
printf("*********** %s *************\n", argv[0]);
    int run_ = 1;
    while (run_)
    {
        printf("============================================\n [q: exit] or [Input name request file] >>> ");
        fflush(stdout);
        std_in(s, sizeof(s));
        if (s[0] == 'q')
            break;

        snprintf(path, sizeof(path), "conf/%s", s);

        if (read_conf_file())
            continue;
        
        if ((n = read_req_file(path, buf_req, sizeof(buf_req))) <= 0)
            continue;
        printf("%s", buf_req);
        printf("------------------------------\nPort: ");
        fflush(stdout);
        std_in(Port, sizeof(Port));
        if (Port[0] == 'q')
            break;

        printf("Num proc: ");
        fflush(stdout);
        std_in(s, sizeof(s));
        if (s[0] == 'q')
            break;
        sscanf(s, "%d", &numProc);
        if (numProc > 8)
        {
            printf("! numProc >= 8\n");
            continue;
        }

        printf("NumThread x %d: ", numProc);
        fflush(stdout);
        std_in(s, sizeof(s));
        if (s[0] == 'q')
            break;
        if (sscanf(s, "%d", &NumThreads) != 1)
        {
            printf("<%s:%d>  Error NumThread: %s\n", __func__, __LINE__, s);
            continue;
        }

        printf("NumRequests: ");
        fflush(stdout);
        std_in(s, sizeof(s));
        if (s[0] == 'q')
            break;
        if (sscanf(s, "%d", &NumRequests) != 1)
        {
            printf("<%s:%d>  Error NumRequests: %s\n", __func__, __LINE__, s);
            continue;
        }

        time_t now;
        time(&now);
        printf("%s\n", ctime(&now));

        char buf[512];
        snprintf(buf, sizeof(buf), "HEAD / HTTP/1.1\r\n"
                "Host: %s\r\n"
                "User-Agent: ???\r\n"
                "Connection: close\r\n\r\n", Host);

        printf("----------------------------------\n"
               "%s"
               "----------------------------------\n", buf);
        int servSocket = create_client_socket(Host, Port);
        if (servSocket == -1)
        {
            printf("<%s:%d> Error: create_client_socket(%s, %s)\n", __func__, __LINE__, Host, Port);
            continue;
        }

        if ((ai_family != AF_INET) && (ai_family != AF_INET6))
        {
            printf("<%s:%d> Error: ai_family: %s\n", __func__, __LINE__, get_str_ai_family(ai_family));
            continue;
        }

        printf("IP: %s, FAMILY: %s\n----------------------------------\n", IP, get_str_ai_family(ai_family));

        n = write_timeout(servSocket, buf, strlen(buf), 10);
        if (n < 0)
        {
            printf("<%s:%d> Error send request\n", __func__, __LINE__);
            shutdown(servSocket, SHUT_RDWR);
            close(servSocket);
            continue;
        }

        n = read_timeout(servSocket, buf, sizeof(buf) - 1, 10);
        if (n < 0)
        {
            printf("<%s:%d> Error read response: %s\n   [%s:%s %d]\n", __func__, __LINE__, 
                            strerror(errno), Host, Port, servSocket);
            shutdown(servSocket, SHUT_RDWR);
            close(servSocket);
            continue;
        }

        buf[n] = 0;
        printf("%s\n",buf);
        shutdown(servSocket, SHUT_RDWR);
        close(servSocket);

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
                printf("<%s:%d> Error fork(): %s\n", __func__, __LINE__, strerror(errno));
                exit(1);
            }

            num++;
        }

        while (wait(NULL) != -1);
    }

    return 0;
}
