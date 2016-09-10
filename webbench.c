/*************************************************************************
	> File Name: webbench.c
	> Author: 
	> Mail: 
	> Created Time: 2016年09月10日 星期六 10时30分09秒
 ************************************************************************/

#include"socket.c"
#include<stdio.h>
#include<getopt.h>
#include<string.h>
#include<sys/param.h>
#include<unistd.h>
#include<time.h>
#include<signal.h>
#include<rpc/types.h>


volatile int timerexpired = 0;
int speed = 0;
int failed = 0;
int bytes = 0;

//支持的HTTP协议　０-0.9,   1-1.0,  2-1.1
int http10 = 1;

//支持HTTP的方法： GET  HEAD  OPTIONS  TRACE
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTIONS 2
#define METHOD_TRACE 3
#define PROGRAM_VERSION "1.5"
int method = METHOD_GET;  //默认为GET方法


int clients = 1;  //默认启动一个客户端(子进程)
int force = 0;    //是否等待响应数据返回，　０等待，１　不等等
int force_reload = 0;  //是否发送Ｐragma:no-cache
int proxyport = 80;  //代理端口
char *proxyhost = NULL; //代理服务器名称
int benchtime = 30; //执行时间，当子进程执行时间超过这个秒数后，
                    //发送SIGALRM信号，将timerexpired设置１,让所有子进程退出
                    
//internal
int mypipe[2];  //管道
char host[MAXHOSTNAMELEN];  //网络地址

#define REQUEST_SIZE 2048
char request[REQUEST_SIZE];  //请求


//命令行参数选项配置表，细节部分查看　man文档　man getopt_long
static const struct option long_options[] = {
    {"force",       no_argument,        &force,         1},
    {"reload",      no_argument,        &force_reload,  1},
    {"time",        required_argument,  NULL,           't'},
    {"help",        no_argument,        NULL,           '?'},
    {"http09",      no_argument,        NULL,           '9'},
    {"http10",      no_argument,        NULL,           '1'},
    {"http11",      no_argument,        NULL,           '2'},
    {"get",         no_argument,        &method,        METHOD_GET},
    {"head",        no_argument,        &method,        METHOD_HEAD},
    {"options",     no_argument,        &method,        METHOD_OPTIONS},
    {"trace",       no_argument,        &method,        METHOD_TRACE},
    {"version",     no_argument,        NULL,           'V'},
    {"proxy",       required_argument,  NULL,           'p'},
    {"clients",     required_argument,  NULL,           'c'},
    {NULL,          0,                  NULL,           0}
};


static void alarm_handler(int signal)
{
    timerexpired = 1;
}

//打印帮助
static void usage(void)
{
    fprintf(stderr, 
            "webbench [option]... URL\n"

            "   -f|--force              Don't wait for reply from server.\n"
            "   -r|--reload             Send reload request -Pragma:no-cache.\n"
            "   -t|--time<sec>          Run benchmark for <sec> seconds. Default 30\n"
            "   -p|--proxy<server:port> Use proxy server for request.\n"
            "   -c|--clients<n>         Run<n>Http clients at once. Default one.\n"
            "   -9|--http09             Use Http/0.9 style requests.\n"
            "   -1|--http10             Use Http/1.0 protocol.\n"
            "   -2|--http11             Use Http/1.1 protocol.\n"
            "   --get                   Use GET request method.\n"
            "   --head                  Use HEAD request method.\n"
            "   --options               Use OPTIONS request method.\n"
            "   --trace                 Use TRACE request method.\n"
            "   -?|-h|--help            This information.\n"
            "   -V|--version            Display program version.\n"
           );
}

void benchcore(const char *host, const int port, const int *req);
//根据URL，生成HTTP头
void build_request(const char *url)
{
    char tmp[10];
    int i;

    //初始化
    bzero(host, MAXHOSTNAMELEN);
    bzero(request, REQUEST_SIZE);

    //判断应该使用哪种　HTTP 协议
    if(force_reload && proxyhost != NULL && http10 < 1) http10 = 1;
    if(method == METHOD_HEAD && http10 < 1) http10 = 1;
    if(method == METHOD_OPTIONS && http10 < 2) http10 = 2;
    if(method == METHOD_TRACE && http10 < 2)  http10 = 2;

    //填写method方法
    switch(method)
    {
        default:
        case METHOD_GET: strcpy(request, "GET"); break;
        case METHOD_OPTIONS: strcpy(request, "OPTIONS"); break;
        case METHOD_HEAD:   strcpy(request, "HEAD"); break;
        case METHOD_TRACE:  strcpy(request, "TRACE"); break;
    }
    strcat(request, " ");

    //url 合法性判断
    if(strstr(url, "://") == NULL)
    {
        fprintf(stderr, "\n%s: is not a valid URL.\n", url);
        exit(2);
    }

    if(strlen(url) > 1500)
    {
        fprintf(stderr, "URL is too long.\n");
        exit(2);
    }

    if(proxyhost == NULL)
    {
        if(strncasecmp("http://", url, 7) != 0)
        {
            //只支持　HTTP 地址
            fprintf(stderr, "\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
            exit(2);
        }
    }

    //找到主机名开始的地方, i 指向url中http://下一个位置
    i = strstr(url, "http://") - url + 3;

    if(strchr(url + i, '/') == NULL)
    {
        fprintf(stderr, "\nInvald URL syntax-hostname don't end with '/'.\n ");
        exit(2);
    }

    if(proxyhost == NULL)
    {
        //get port from hostname
        //判断url中是否指定了端口号
        if(index(url + i, ':') != NULL && index(url + i , ':') < index(url + i, '/'))
        {
            strncpy(host, url + i, strchr(url + i, ':') - url - i); //取出主机地址
            bzero(tmp, 10);
            strncpy(tmp, index(url + i, ':') + 1, strchr(url + i, '/') - index(url + i, ':') - 1);
            proxyport = atoi(tmp);  //端口号转换为int
            if(proxyport == 0) proxyport = 80;
        }
        else{
            strncpy(host, url + i, strcspn(url + i, "/"));
        }
        strcat(request + strlen(request), url + i + strcspn(url + i, "/"));
    }

    if(http10 == 1)
        strcat(request, " HTTP/1.0");
    else if(http10 == 2)
        strcat(request, " HTTP/1.1");

    strcat(request, "\r\n");
    
    if(http10 > 0)
        strcat(request, "User-Agent: WebBench"PROGRAM_VERSION"\r\n");

    if(proxyhost == NULL && http10 > 0)
    {
        strcat(request, "Host: ");
        strcat(request, host);
        strcat(request, "\r\n");
    }

    if(force_reload && proxyhost != NULL)
        strcat(request, "Pragma: no-cache\r\n");

    if(http10 > 1)
        strcat(request, "Connection: close\r\n");

    if(http10 > 0)
        strcat(request, "\r\n");
}

//创建管道和子进程，对http请求进行测试
static int bench(void)
{
    int i, j, k;
    pid_t pid = 0;
    FILE *f;

    //测试地址是否可用
    i = Socket(proxyhost == NULL ? host : proxyhost, proxyport);
    if(i < 0)
    {
        fprintf(stderr, "\nConnect to server failed. Aborting benchmark.\n");
        return 1;
    }
    close(i);

    if(pipe(mypipe) < 0) //创建管道
    {
        perror("pipe failed.");
        return 3;
    }

    //fork 子进程
    for(i = 0; i < clients; i++)
    {
        pid = fork();
        if(pid <= (pid_t) 0) //子进程或者出错
        {
            sleep(1); 
            break;
        }
    }

    if(pid < (pid_t) 0)
    {
        fprintf(stderr, "fork error no.%d\n", i);
        perror("fork failed");
        return 3;
    }

    if(pid == 0)
    {
        //子进程
        if(proxyhost == NULL)
            benchcore(host, proxyport, request);
        else
            benchcore(proxyhost, proxyport, request);

        f = fdopen(mypipe[1], "w");
        if(f == NULL)
        {
            perror("open pipe for writing failed.");
            return 3;
        }

        fprintf(f,"%d %d %d\n", speed, failed, bytes);
        fclose(f);
        return 0;
    }
    else
    {
        f = fdopen(mypipe[0], "r");
        if(f == NULL)
        {
            perror("open pipe for reading failed.");
            return 3;
        }

        setvbuf(f, NULL, _IONBF, 0);
        speed = 0;
        failed = 0;
        bytes = 0;

        while(1) //父进程读取管道数据
        {
            pid = fscanf(f, "%d %d %d", &i, &j, &k);
            if(pid < 2)
            {
                fprintf(stderr, "Some of our childrens died.\n");
                break;
            }

            speed += i;
            failed += j;
            bytes += k;
            if(--clients == 0) break;
        }

        //输出测试结果
        printf("\nSpeed = %d pages/min, %d bytes/sec.\nRequest: %d succeed, %d failed.\n",
              (int)((speed + failed) / (benchtime / 60.0f)),
              (int)(bytes / (float)benchtime),
              speed, 
              failed);
    }
    return i;
}

//测试核心部分
void benchcore(const char *host, const int port, const int *req)
{
    int rlen;
    char buf[1500];
    int s, i;
    struct sigaction sa;

    sa.sa_handler = alarm_handler;
    sa.sa_flags = 0;
    if(sigaction(SIGALRM, &sa, NULL))
        exit(3);
    alarm(benchtime);

    rlen = strlen(req);
    
    nexttry:
    while(1)
    {        
        if(timerexpired) //定时器到时后，会设定timerexpired=1,函数就会返回
        {
            if(failed > 0)
                failed--;
        
            return;
        }
        s = Socket(host, port); //创建连接
        if(s < 0)
        {
            failed++;
            continue;
        }

        if(rlen != write(s, req, rlen))
        {
            failed++;
            close(s);
            continue;
        }

        if(force == 0) //force 等于０表示要读取http请求数据
        {
            while(1)
            {
                if(timerexpired) 
                    break;

                i = read(s, buf, 1500);
                if(i < 0)
                {
                    failed++;
                    close(s);
                    goto nexttry;
                }
                else
                {
                    if(i == 0) break;
                    else bytes += i;  //读取字节数增加
                }
            }
        }

        if(close(s))
        {
            failed++;
            continue;
        }
        speed++;  //http测试成功一次，speed加１
    }
}

int main(int argc, char *argv[])
{
    int opt = 0;
    int options_index = 0;
    char *tmp = NULL;

    if(argc == 1)
    {
        usage();
        return 2;
    }

    //参数解析，参见　man getopt_long
    while((opt = getopt_long(argc, argv, "912Vfrt:p:c:?h", long_options, &options_index)) != EOF)
    {
        switch(opt)
        {
            case 0: break;
            case 'f':   
                force = 1;  
                break;
            case 'r':
                force_reload = 1;
                break;
            case '9':
                http10 = 0;
                break;
            case '1':
                http10 = 1;
                break;
            case '2':
                http10 = 2;
                break;
            case 'V':
                printf(PROGRAM_VERSION"\n");
                exit(0);
            case 't':
                benchtime = atoi(optarg);
                break;
            case 'p':
                //proxy server parsing server:port
                tmp = strrchr(optarg, ':');
                proxyhost = optarg;
                if(tmp == NULL)
                    break;
                if(tmp == optarg)
                {
                    fprintf(stderr, "Error in option --proxy %s: Missing hostname.\n", optarg);
                    break;
                }
                if(tmp == optarg + strlen(optarg) - 1)
                {
                    fprintf(stderr, "Error in option --proxy %s Port number is missing.\n", optarg);
                    return 2;
                }
                *tmp = '\0';
                proxyport = atoi(tmp + 1); 
                break;
            case ':':
            case 'h':
            case '?':   
                usage();
                return 2;
            case 'c':
                clients = atoi(optarg);
                break;
        }
    }
    
    //optind被getopt_long设置为命令行参数中为读取的下一个元素下标值
    if(optind == argc)
    {
        fprintf(stderr, "webbench: Missing URL!\n");
        usage();
        return 2;
    }

    //不能指定客户端数和请求时间为０
    if(clients == 0)
        clients = 1;
    if(benchtime == 0)  
        benchtime = 60;

    //Copyright
    fprintf(stderr, "Webbench - Simple Web Benchmark "PROGRAM_VERSION"\n"
           "Copyright(c) Radim Kolar 1997-2004, GPL Open Source SoftWare.\n");

    //最后一个参数被视为URL
    build_request(argv[optind]);
    
    //输出测试信息
    printf("\nBenchmarking: ");
    switch(method)
    {
        case METHOD_GET:
        default:
            printf("GET"); break;
        case METHOD_OPTIONS:
            printf("OPTIONS"); break;
        case METHOD_TRACE:
            printf("TRACE"); break;
        case METHOD_HEAD:
            printf("HEAD"); break;
    }
    printf("  %s", argv[optind]);

    switch(http10)
    {
        case 0: printf(" (using HTTP/0.9)"); break;
        case 2: printf(" (using HTTP/1.1)"); break;
    }
    printf("\n");

    printf("%d clients", clients);
    printf(", running %d sec", benchtime);

    if(force)  
        printf(", early socket close");
    if(proxyhost != NULL)
        printf(", via proxy server %s: %d", proxyhost, proxyport);
    if(force_reload)
        printf(", forcing reload");
    printf(".\n");

    //开始压力测试，返回bench函数返回结果
    return bench();
}
