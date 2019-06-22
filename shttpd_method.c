/******************************************************************************
 * Filename: shttpd_method.c      
 *
 * %Description:  shttp 对各种方法的响应
 * %date_created:  2019.6.19 
 * %version:       1 
 * %authors:       Theo_hui
*******************************************************************************/

#include "shttpd.h"

////////////////////////////////////////////////////////////////////////////////////////////
///
///  [HTTP/1.1 200 OK
///  Date: Thu, 11 Dec 2008 11:25:33 GMT
///  Last-Modified: Wed, 12 Nov 2008 09:00:01 GMT
///  Etag: "491a2a91.2afe"
///  Content-Type: text/plain
///  Content-Length: 11006
///  Accept-Ranges: bytes]                                                             /////
///////////////////////////////////////////////////////////////////////////////////////////

/**
 * GET 方法的响应
 * @param  wctl 线程控制结构
 * @return      [description]
 */
static int Method_DoGet(struct worker_ctl *wctl)
{

#ifdef DEBUG
    printf(">>>>>>>>> Method_DoGet 该请求为GET请求，正在处理响应......\n");
#endif

    struct conn_response *res = &wctl->conn.con_res;//连接的响应
    struct conn_request *req = &wctl->conn.con_req; //连接的请求
    
    char path[URI_MAX];                             //URI路径
    memset(path, 0, URI_MAX);

    size_t      n;
    unsigned long   r1, r2;
    char    *fmt = "%a, %d %b %Y %H:%M:%S GMT";      //时间格式

    /*******************响应报文的参数*****************************/
    size_t status = 200;        /*状态值,已确定*/
    char *msg = "OK";           /*状态信息,已确定*/
    char  date[64] = "";        /*时间*/
    char  lm[64] = "";          /*请求文件最后修改信息*/
    char  etag[64] = "";        /*etag信息*/
    big_int_t cl;               /*内容长度*/
    char  range[64] = "";       /*范围*/  
    struct mine_type *mine = NULL;



    /*当前时间*/
    time_t t = time(NULL);  
    (void) strftime(date, 
                sizeof(date), 
                fmt, 
                localtime(&t));

    /*最后修改时间*/
    (void) strftime(lm, 
                sizeof(lm), 
                fmt, 
                localtime(&res->fsate.st_mtime));

    /*ETAG*/
    (void) snprintf(etag, 
                sizeof(etag), 
                "%lx.%lx",
                (unsigned long) res->fsate.st_mtime,
                (unsigned long) res->fsate.st_size);


    /*发送的MIME类型*/
    mine = Mine_Type(req->rpath, strlen(req->rpath), wctl);

    /*内容长度*/
    cl = (big_int_t) res->fsate.st_size;

    /*范围range*/
    memset(range, 0, sizeof(range));
    n = -1;
    if (req->ch.range.v_vec.len > 0 )/*取出请求范围*/
    {
        printf("request range:%d\n",req->ch.range.v_vec.len);
        n = sscanf(req->ch.range.v_vec.ptr,"bytes=%lu-%lu",&r1, &r2);
    }

    printf("n:%d\n",n);
    if(n   > 0) 
    {
        status = 206;
        lseek(res->fd, r1, SEEK_SET);
        cl = n == 2 ? r2 - r1 + 1: cl - r1;
        (void) snprintf(range, 
                    sizeof(range),
                    "Content-Range: bytes %lu-%lu/%lu\r\n",
                    r1, 
                    r1 + cl - 1, 
                    (unsigned long) res->fsate.st_size);
        msg = "Partial Content";
    }

    memset(res->res.ptr, 0, sizeof(wctl->conn.dres));
    snprintf(
        res->res.ptr,
        sizeof(wctl->conn.dres),
        "HTTP/1.1 %d %s\r\n" 
        "Date: %s\r\n"
        "Last-Modified: %s\r\n"
        "Etag: \"%s\"\r\n"
        "Content-Type: %.*s\r\n"
        "Content-Length: %lu\r\n"
        //"Connection:close\r\n"
        "Accept-Ranges: bytes\r\n"      
        "%s\r\n",
        status,
        msg,
        date,
        lm,
        etag,
        strlen(mine->mime_type),
        mine->mime_type,
        cl,
        range);
    res->cl = cl;
    res->status = status;
    printf("content length:%d, status:%d\n",res->cl, res->status);

    return 0;
}

static int Method_DoPost(struct worker_ctl *wctl)
{
# ifdef DEBUG
    printf(">>>>>>>>>> Method正在处理POST请求\n");
# endif

    struct conn_response *res = &wctl->conn.con_res;//连接的响应
    struct conn_request *req = &wctl->conn.con_req; //连接的请求

    size_t      n;
    unsigned long   r1, r2;
    char    *fmt = "%a, %d %b %Y %H:%M:%S GMT";      //时间格式

    /*******************响应报文的参数*****************************/
    size_t status = 200;        /*状态值,已确定*/
    char *msg = "OK";           /*状态信息,已确定*/
    char  date[64] = "";        /*时间*/
    char  etag[64] = "";        /*etag信息*/

    /*当前时间*/
    time_t t = time(NULL);  
    (void) strftime(date, 
                sizeof(date), 
                fmt, 
                localtime(&t));

    /*ETAG*/
    (void) snprintf(etag, 
                sizeof(etag), 
                "%lx.%lx",
                (unsigned long) res->fsate.st_mtime,
                (unsigned long) res->fsate.st_size);

    memset(res->res.ptr, 0, sizeof(wctl->conn.dres));
    snprintf(
        res->res.ptr,
        sizeof(wctl->conn.dres),
        "HTTP/1.1 %d %s\r\n" 
        "Date: %s\r\n"
        "Etag: \"%s\"\r\n"     
        "\r\n",
        status,
        msg,
        date,
        etag);
    res->status = status;
    printf("status:%d\n",res->cl, res->status);

    return 0;
}

static int Method_DoHead(struct worker_ctl *wctl)
{
    Method_DoGet(wctl);
    close(wctl->conn.con_res.fd);
    wctl->conn.con_res.fd = -1;

    return 0;
}

static int Method_DoPut(struct worker_ctl *wctl)
{
    return 0;
}

static int Method_DoDelete(struct worker_ctl *wctl)
{
    return 0;
}

static int Method_DoCGI(struct worker_ctl *wctl)
{
    return 0;   
}

static int Method_DoList(struct worker_ctl *wctl)
{
    return 0;
}

/*************************************************************
 * 总的请求响应 依据不同的请求方法来响应
 * @param wctl 请求控制结构
 */
void Method_Do(struct worker_ctl *wctl)
{
#ifdef DEBUG
    printf(">>>>>>> Method_Do 正在处理响应......\n");
#endif

    if(0)
        Method_DoCGI(wctl);//CGI没有实现

    /*依据不同的请求方法来具体响应*/
#ifdef DEBUG
    printf("[*]请求报文的方法代号 %d\n",wctl->conn.con_req.method);
#endif
    switch(wctl->conn.con_req.method)
    {
        case METHOD_PUT:
            Method_DoPut(wctl);
            break;

        case METHOD_DELETE:
            Method_DoDelete(wctl);
            break;
        case METHOD_GET:
            Method_DoGet(wctl);
            break;
        case METHOD_POST:
            Method_DoPost(wctl);
            break;
        case METHOD_HEAD:
            Method_DoHead(wctl);
            break;
        default:
            Method_DoList(wctl);
    }

#ifdef DEBUG
    printf("[-]响应处理完毕\n");
#endif

}

