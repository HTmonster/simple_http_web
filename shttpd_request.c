/******************************************************************************
 * Filename: shttpd_request.c      
 *
 * %Description:  shttp 请求的文件的处理
 * %date_created:  2019.6.19 
 * %version:       1 
 * %authors:       Theo_hui
*******************************************************************************/

#include "shttpd.h"

///////////////////////////////////宏定义//////////////////////////////////////

#define JUMPOVER_CHAR(p,over) do{for(;*p== over;p++);}while(0);
#define JUMPTO_CHAR(p,to) do{for(;*p!= to;p++);}while(0);


///////////////////////////////////全局变量////////////////////////////////////

/* http 请求头*/
static struct http_header http_headers[] = {
    {16,    HDR_INT,    OFFSET(cl),         "Content-Length: "      },
    {14,    HDR_STRING, OFFSET(ct),         "Content-Type: "        },
    {12,    HDR_STRING, OFFSET(useragent),  "User-Agent: "          },
    {19,    HDR_DATE,   OFFSET(ims),        "If-Modified-Since: "   },
    {15,    HDR_STRING, OFFSET(auth),       "Authorization: "       },
    {9,     HDR_STRING, OFFSET(referer),    "Referer: "             },
    {8,     HDR_STRING, OFFSET(cookie),     "Cookie: "              },
    {10,    HDR_STRING, OFFSET(location),   "Location: "            },
    {8,     HDR_INT,    OFFSET(status),     "Status: "              },
    {7,     HDR_STRING, OFFSET(range),      "Range: "               },
    {12,    HDR_STRING, OFFSET(connection), "Connection: "          },
    {19,    HDR_STRING, OFFSET(transenc),   "Transfer-Encoding: "   },
    {0,     HDR_INT,    0,                  NULL                    }
};

extern struct vec _shttpd_methods[];
struct conf_opts conf_para;


/////////////////////////////////函数声明/////////////////////////////////////

extern void Error_400(struct worker_ctl* wctl);
extern void Error_403(struct worker_ctl* wctl);
extern void Error_404(struct worker_ctl* wctl);
extern void Error_505(struct worker_ctl* wctl);


/////////////////////////////////函数实现////////////////////////////////////


static int
montoi(char *s)
{
    DBGPRINT("==>montoi\n");
    int retval = -1;
    static char *ar[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    size_t  i;

    for (i = 0; i < sizeof(ar) / sizeof(ar[0]); i++)
        if (!strcmp(s, ar[i])){
            retval = i;
            goto EXITmontoi;
        }
    DBGPRINT("<==montoi\n");
EXITmontoi:
    return retval;
}

/**
 * 解析时间格式字符串 返回对应的时间
 * @param  s 时间格式的字符串
 * @return   对应的时间结构
 */
static time_t date_to_epoch(char *s)
{
#ifdef DEBUG
    printf("正在进行时间转换\n");
#endif

    struct tm   tm;
    char        mon[32];
    int     sec, min, hour, mday, month, year;

    (void) memset(&tm, 0, sizeof(tm));
    sec = min = hour = mday = month = year = 0;

    if (((sscanf(s, "%d/%3s/%d %d:%d:%d",
        &mday, mon, &year, &hour, &min, &sec) == 6) ||
        (sscanf(s, "%d %3s %d %d:%d:%d",
        &mday, mon, &year, &hour, &min, &sec) == 6) ||
        (sscanf(s, "%*3s, %d %3s %d %d:%d:%d",
        &mday, mon, &year, &hour, &min, &sec) == 6) ||
        (sscanf(s, "%d-%3s-%d %d:%d:%d",
        &mday, mon, &year, &hour, &min, &sec) == 6)) &&
        (month = montoi(mon)) != -1) {
        tm.tm_mday  = mday;
        tm.tm_mon   = month;
        tm.tm_year  = year;
        tm.tm_hour  = hour;
        tm.tm_min   = min;
        tm.tm_sec   = sec;
    }

    if (tm.tm_year > 1900)
        tm.tm_year -= 1900;
    else if (tm.tm_year < 70)
        tm.tm_year += 100;

    DBGPRINT("<==date_to_epoch\n");
    return (mktime(&tm));
}



/**
 * 解析剩余的请求头
 * @param s      字符串
 * @param len    字符串长度
 * @param parsed 头部结构
 * @return       如果是post请求则会返回剩余的数据段，否则返回NULL
 */
char* Request_HeaderParse(char *s, int len, struct headers *parsed)
{
# ifdef DEBUG
    printf("正在解析剩余的请求头 Request_HeaderParse\n");
# endif

    struct http_header  *h; /*结构struct http_header指针*/
    union variant *v;/*通用参数*/
    char *p, *e = s + len;/*p当前位置,e尾部*/


    /* 查找请求字符串中的头部关键字*/
    while (s < e) 
    {

        /* 查找一行末尾*/
        for (p = s; p < e && *p != '\n'; ) 
        {   
            p++;
        }
        /* 已知方法*/
#ifdef DEBUG_DETAIL
        printf("[=]*************现在************\n%s \n[=]*************下一个************\n %s\n",s,p);
#endif
        for (h = http_headers; h->len != 0; h++)
        {
            if (e - s > h->len &&    /*字符串匹配*/
                !strncasecmp(s, h->name, h->len))
            {   
#ifdef DEBUG
                printf("[=]匹配到头部项%s\n",h->name);
#endif
                break;
            }
        }
        
        /* 将此方法放入*/
        if (h->len != 0) 
        {

            /* 请求字符串中值的位置*/
            s += h->len;

            /* 将值存放到参数parsed中 */
            v = (union variant *) ((char *) parsed + h->offset);

            /* 根据头部选项不同,计算不同的值*/
            if (h->type == HDR_STRING) /*字符串类型*/
            {
                v->v_vec.ptr = s;/*字符串开始*/
                v->v_vec.len = p - s;/*字符串长度*/
                if (p[-1] == '\r' && v->v_vec.len > 0)
                {
                    v->v_vec.len--;
                }
            } 
            else if (h->type == HDR_INT) /*INT类型*/
            {
                v->v_big_int = strtoul(s, NULL, 10);
            } 
            else if (h->type == HDR_DATE) /*时间格式*/
            {
                v->v_time = date_to_epoch(s);
            }
        }


        /**********************************************
        *  判断是不是body的data数据段
        *  遇到两个 \r\n
        **************/
        if(p+2<e&&
            *(p-1)=='\r'&&*p=='\n'&&
            *(p+1)=='\r'&&*(p+2)=='\n'){
# ifdef DEBUG
            printf("[-]检测到双间隔，退出头部解析 %s\n",p+2);
# endif     
            return p+2; //退出解析body数据
        }


        s = p + 1;  /* Shift to the next header */
    }

    return NULL;

}

/**
 * 解析请求报文的主体
 * @param s      主体字符串
 * @param header 解析好了的头部信息
 * @param body   要解析的主体信息
 */
void Request_BodyParse(char *s,struct headers *header,struct body *body){

    body->len=header->cl.v_int;/*长度*/
    body->type=header->ct.v_str;/*类型*/

    body->data=s;/*数据内容*/

#ifdef DEBUG
    printf("[=]POST数据：%s len:%d type:%s\n",body->data,body->len,body->type);
#endif

    return;
}



/**************************************************************
 * 用来解析请求
 * @param  wctl worker控制
 * @return      [description]
 */
int  Request_Parse(struct worker_ctl *wctl)
{

# ifdef DEBUG
    printf("Request_Parse 正在解析请求......\n");
# endif

    struct worker_conn *c = &wctl->conn;    //客户端的请求连接信息
    struct conn_request *req = &c->con_req; //客户端请求结构
    struct conn_response *res = &c->con_res;//客户端响应结构
    int retval = 200;
    
    char *p = req->req.ptr;//请求的字符串
    int len = req->req.len;//请求的长度
    char *pos = NULL;

/////////////////////////////////////////////////////
///    [GET /root/default.html HTTP/1.1\r\n]    /////
/////////////////////////////////////////////////////

    /*处理第一行*/
    pos = memchr(p, '\n', len);//第一行的末尾
    if(*(pos-1) == '\r')
    {
        *(pos-1) = '\0';
    }
    *pos = '\0';

    pos = p;

    /***********************方法*********************/    
    int found = 0;
    
    JUMPOVER_CHAR(pos,' ');/*跳过空格*/

    struct vec *m= NULL;
    /*请求类型匹配 根据设置好的一一匹配*/
    for(m = &_shttpd_methods[0];m->ptr!=NULL;m++)
    {
        if(!strncmp(m->ptr, pos, m->len))/*匹配到头部方法*/
        {
            req->method = m->type;/*写入头部*/
#ifdef DEBUG
            printf("[*]匹配到头部方法%s %d\n",m->ptr,req->method);
#endif
            found = 1;            /*找到标记*/
            break;
        }
    }
    if(!found)
    {
        retval = 400;
        goto EXITRequest_Parse;
    }

    /*****************************URI分析*****************************/
    pos += m->len;/*跳过方法  例如GET POST*/
    JUMPOVER_CHAR(pos,' ');/*跳过空格*/

    len -= pos -p;
    p = pos;
    JUMPTO_CHAR(pos, ' ');/*跳到空格*/
    *pos = '\0';

    req->uri = (char*)p;  //解析uri
    /*URI文件路径*/
    snprintf(req->rpath, URI_MAX, "%s%s",conf_para.DocumentRoot, req->uri);

    /*默认的文件路径*/
    if (req->uri[strlen(req->uri) - 1] == '/') {
         strcat(req->rpath, conf_para.DefaultFile);
    }

    printf("%s\n",req->rpath);
        
    res->fd = open(req->rpath, O_RDONLY , 0644);

    /*uri文件处理*/
    if(res->fd != -1)
    {
        fstat(res->fd, &res->fsate);//取文件的状态
        if(S_ISDIR(res->fsate.st_mode))
        {   
            /*是一个目录*/
            retval = 403;
            goto EXITRequest_Parse;
        }
    }
    else
    {   /*打开失败*/
        
        retval = 404;
        goto EXITRequest_Parse;
    }
        
    /**************************HTTP版本****************************/
    /*   HTTP/[1|0].[1|0|9] */

    pos += 1;
    JUMPOVER_CHAR(pos,' ');/*跳过空格*/

    len -= pos -p;
    p = pos;
    sscanf(p,
        "HTTP/%lu.%lu",
        &req->major, 
        &req->minor);
    if(!((req->major == 0 && req->minor == 9)||
        (req->major == 1 && req->minor == 0)||
        (req->major == 1 && req->minor == 1)))
    {
        retval = 505;
        goto EXITRequest_Parse;
    }

#ifdef  DEBUG
    printf("[*]解析头部第一行\n");
#endif

    /*************************其他头部信息************************/
    JUMPTO_CHAR(pos, '\0');
    JUMPOVER_CHAR(pos,'\0');/*跳过空字符*/
    len -= pos - p;
    p = pos;

#ifdef  DEBUG
    printf("[*]解析头部剩余行\n");
#endif

    char* post_data=Request_HeaderParse(p, len, & req->ch);

    /*************************POST数据解析***********************/
    if(req->method==METHOD_POST&&post_data!=NULL){

#ifdef DEBUG
        printf("[*]解析POST数据\n");
        Request_BodyParse(post_data,&req->ch,&req->bd);
#endif
    }


# ifdef DEBUG
    printf("解析到的URI:'%s',patch:'%s' err:%d\n",req->uri,req->rpath,retval);
# endif

EXITRequest_Parse:
    return retval;
}

/*****************************************************************
 * 响应客户端的请求
 * @param  wctl 线程控制结构
 * @return      [description]
 */
int Request_Handle(struct worker_ctl* wctl)
{
# ifdef DEBUG
    printf("Request_Handle 正在响应请求\n");
# endif

    int err = wctl->conn.con_req.err;//请求的错误代码
    int cs = wctl->conn.cs;          //客户端套接字文件描述符
    int cl = -1;                     
    char *ptr = wctl->conn.con_res.res.ptr;//响应向量的指针
    int len = -1;
    int n = -1;

    /*事件响应*/
# ifdef DEBUG
    printf("[*] 请求解析情况 %d\n",err);
# endif

    switch(err)
    {   
        /*正常响应*/
        case 200:
            Method_Do(wctl);     //  响应方法的实现:只实现了GET POST,具体的方法在shttpd_method.c中
            int fd = wctl->conn.con_res.fd;
            cl = wctl->conn.con_res.cl;
            len = strlen(wctl->conn.con_res.res.ptr);

            printf("%s\n",ptr);
            
            n = write(cs, ptr, len);
            printf("echo header:%s, write to client %d bytes, status:%d\n",ptr,n,wctl->conn.con_res.status);

            if(fd != -1)
            {       
                lseek(fd, 0, SEEK_SET);
                len = sizeof(wctl->conn.dres);
                printf("response len:%d, content length:%d\n",len,wctl->conn.con_res.cl);
                for(n = 0; cl>0; cl -= n)
                {
                    n = read(fd,ptr,len>cl?cl:sizeof(wctl->conn.dres));
                    printf("read %d bytes,",n);
                    if(n > 0)
                    {
                        n =write(cs, ptr, n);
                        printf("write %d bytes\n",n);
                    }
                }
                close(fd);
                wctl->conn.con_res.fd = -1;
            }

            break;
        default:
        case 400:
            Error_400(wctl);
            cl = wctl->conn.con_res.cl;
            len = strlen(wctl->conn.con_res.res.ptr);
            
            n = write(cs, ptr, len);            
            break;  
        case 403:
            Error_403(wctl);
            cl = wctl->conn.con_res.cl;
            len = strlen(wctl->conn.con_res.res.ptr);
            
            n = write(cs, ptr, len);            
            break;  
        case 404:
            Error_404(wctl);
            cl = wctl->conn.con_res.cl;
            len = strlen(wctl->conn.con_res.res.ptr);
            
            n = write(cs, ptr, len);            
            break;  
        case 505:
            Error_505(wctl);
            cl = wctl->conn.con_res.cl;
            len = strlen(wctl->conn.con_res.res.ptr);
            
            n = write(cs, ptr, len);            
            break;  
        
    }

    return 0;
}