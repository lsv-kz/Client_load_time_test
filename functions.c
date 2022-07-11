#include "client.h"

//======================================================================
void std_in(char *s, int len)
{
    char ch;
    
    while ((ch = getchar()) != '\n' && (len - 1))
    {
        *s++ = ch;
        len--;
    }
    *s = '\0';
    if (ch != '\n')
        while ((ch = getchar()) != '\n');
}
//======================================================================
int strcmp_case(const char *s1, const char *s2)
{
    char c1, c2;
    
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;

    for (; ; ++s1, ++s2)
    {
        c1 = *s1;
        c2 = *s2;
        if (!c1 && !c2) return 0;
        if (!c1) return -1;
        if (!c2) return 1;
        
        c1 += (c1 >= 'A') && (c1 <= 'Z') ? ('a' - 'A') : 0;
        c2 += (c2 >= 'A') && (c2 <= 'Z') ? ('a' - 'A') : 0;
        
        if (c1 > c2) return 1;
        if (c1 < c2) return -1;
    }

    return 0;
}
//======================================================================
const char *strstr_case(const char *s1, const char *s2)
{
    const char *p1, *p2;
    char c1, c2;
    
    if (!s1 || !s2) return NULL;
    if (*s2 == 0) return s1;

    for (; ; ++s1)
    {
        c1 = *s1;
        if (!c1) break;
        c2 = *s2;
        c1 += (c1 >= 'A') && (c1 <= 'Z') ? ('a' - 'A') : 0;
        c2 += (c2 >= 'A') && (c2 <= 'Z') ? ('a' - 'A') : 0;
        if (c1 == c2)
        {
            p1 = s1;
            p2 = s2;
            ++s1;
            ++p2;

            for (; ; ++s1, ++p2)
            {
                c2 = *p2;
                if (!c2) return p1;
                
                c1 = *s1;
                if (!c1) return NULL;

                c1 += (c1 >= 'A') && (c1 <= 'Z') ? ('a' - 'A') : 0;
                c2 += (c2 >= 'A') && (c2 <= 'Z') ? ('a' - 'A') : 0;
                if (c1 != c2)
                    break;
            }
        }
    }

    return NULL;
}
//======================================================================
int parse_headers(response *resp)
{
    int n;
    char *pName = resp->buf, *pVal, *p;

    p = strpbrk(pName, "\r\n");
    if (!p) return -200;
    *p = 0;
    
    if (!(p = strchr(pName, ':')))
        return -201;
    *p = 0;

    n = strspn(p + 1, "\x20");
    pVal = p + 1 + n;
    
    if (!strcmp_case(pName, "connection"))
    {
        if (strstr_case(pVal, "keep-alive"))
            resp->connKeepAlive = 1;
        else
            resp->connKeepAlive = 0;
    }
    else if (!strcmp_case(pName, "content-length"))
    {
        sscanf(pVal, "%ld", &resp->len);
    }
    else if (!strcmp_case(pName, "server"))
    {
        snprintf(resp->server, sizeof(resp->server), "%s", pVal);
    }
    else if (!strcmp_case(pName, "Transfer-Encoding"))
    {
        if (strstr_case(pVal, "chunked"))
            resp->chunk = 1;
        else
            resp->chunk = 0;
    }

    return 0;
}
