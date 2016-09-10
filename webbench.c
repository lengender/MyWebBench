/*************************************************************************
	> File Name: webbench.c
	> Author: 
	> Mail: 
	> Created Time: 2016年09月10日 星期六 10时30分09秒
 ************************************************************************/

#include<stdio.h>
#include<getopt.h>
#include<string.h>


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
    {"trace",       no_argument,        &method,        METHOD_REACE},
    {"version",     no_argument,        NULL,           'V'},
    {"proxy",       required_argument,  NULL,           'p'},
    {"clients",     required_argument,  NULL,           'c'},
    {NULL,          0,                  NULL,           0}
};


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

        }
    }
}
