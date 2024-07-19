#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zmq.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <dirent.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include "clist.h"
#include <cjson/cJSON.h>

#if 0
#include "xml.h"
#endif


typedef struct {
    char data[1024];
    int  size;
} msg_t;

typedef struct {
    char               zmq_bind[512];
    void              *zmq_ctx;
    void              *zmq_sck;
    int                exit;
    int                threadNum;
    pthread_t         *threadId;
    clist             *list;
    pthread_mutex_t    mutex;
    pthread_cond_t     wait;
    int                sign;
} demo_ctx;

static demo_ctx   ctx[1] = {0};

void* thread_recv(void *argv) {
    demo_ctx   *ctx = (demo_ctx*) argv;
    ctx->zmq_ctx = zmq_ctx_new();
    ctx->zmq_sck = zmq_socket(ctx->zmq_ctx, ZMQ_REP);
    sprintf(ctx->zmq_bind, "tcp://*:8003");
    int ret = zmq_bind(ctx->zmq_sck, ctx->zmq_bind);
    printf("recv start %d %d!\n", ret, errno);
    while(ctx->exit == 0) {
        msg_t msg;
        memset(&msg, 0, sizeof(msg_t));
        msg.size = zmq_recv(ctx->zmq_sck, msg.data, 1024, 0);
//        printf("11 %s %d\n", msg.data, msg.size);
        pthread_mutex_lock(&(ctx->mutex));
        ctx->list->push_back(ctx->list, &msg);
        ctx->sign = 1;
        pthread_cond_signal(&(ctx->wait));
        pthread_mutex_unlock(&(ctx->mutex));
        char buf[4] = {0};
        zmq_send(ctx->zmq_sck, buf, 4, 0);
    }
    zmq_close(ctx->zmq_sck);
    zmq_ctx_destroy(ctx->zmq_ctx);
    printf("recv exit!\n");
    return NULL;
}

void* thread_proc(void *argv) {
    demo_ctx   *ctx = (demo_ctx*) argv;
    printf("proc start!\n");
    while(ctx->exit == 0) {
        msg_t msg;
        pthread_mutex_lock(&(ctx->mutex));
        if (ctx->list->empty(ctx->list) > 0) {
           if (ctx->sign == 0) {
              pthread_cond_wait(&(ctx->wait), &(ctx->mutex));
           }
           ctx->sign = 0;
           pthread_mutex_unlock(&(ctx->mutex));
           continue;
        }
        msg = *((msg_t *)ctx->list->front(ctx->list));
        ctx->list->pop_front(ctx->list);
        pthread_mutex_unlock(&(ctx->mutex));
//        printf("22 %s %d\n", msg.data, msg.size);
        cJSON* root = cJSON_Parse(msg.data);
        if (root == NULL) {
            continue;
        }
        cJSON* node = cJSON_GetObjectItem(root, "data");
        if (node == NULL) {
            continue;
        }
        char *xname = cJSON_Print(node);
        char name[1024] = {0};
        strncpy(name, xname + 2, strlen(xname) - 4);
        cJSON_Delete(root);
//        printf("%s\n", name);
        remove(name);
#if 0
        struct xml_document *document = xml_parse_document(msg.data, msg.size);
        if (document == NULL) {
           continue;
        }//{"w": 460, "h": 328, "format": 1, "data": ["/tmp/data//460x328_h264_25fps_523f_1.mp4_3.yuv"]}
        struct xml_node *root = xml_document_root(document), *node = NULL;
        const uint8_t *nodename[] = {"w", "h", "format", "data"};
        struct xml_string *xname = NULL, *xcontent = NULL;
        const uint8_t *name, *content;
        int index = 0;
        for (index = 0; index < 4; ++index) {
            size_t length = 0;
            node  =  xml_node_child(root, index);

            xname = xml_node_name(node);
            name = xml_string_buffer(xname);
            length = xml_string_length(xname);
            if (strncmp(name, nodename[index], length) != 0) {
                continue;
            }
            xcontent = xml_node_content(node);
            content = xml_string_buffer(xcontent);
            printf("%d %s: %s\n", index, name, content);
        }
        index = 3;
        node  =  xml_node_child(root, index);
        xcontent = xml_node_content(node);
        content = xml_string_buffer(xcontent);
        remove(content);
        xml_document_free(document, false);
#endif
    }
    printf("proc exit!\n");
    return NULL;
}

void usage(char* cmd) {
    printf("Usage: %s [-h] -t threadNum\n", cmd);
    printf("Options:\n");
    printf("  -h              Print this help message.\n");
    printf("  -t threadNum    process threads default 6.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  shell>  %s -t 12\n", cmd); 
}

int parse_opt(int argc, char* argv[], demo_ctx *ctx) {
    const char* optstr = "ht:";
    int opt;
    ctx->threadNum = 6;
    while( (opt = getopt(argc, argv, optstr)) != -1 ) {
        switch (opt) {
        case 'h':
            usage(argv[0]);
            return 0;
        case 't':// 0x40
            ctx->threadNum = atoi(optarg);
            break;
        default:
            usage(argv[0]);
            return 0;
        }
    }
    return 1;
}

static void signal_handler(int sig) {
    ctx->exit = 1;
    pthread_mutex_lock(&(ctx->mutex));
    ctx->sign = 1;
    pthread_cond_broadcast(&(ctx->wait));
    pthread_mutex_unlock(&(ctx->mutex));
}

int main(int argc, char *argv[]) {
    int i = 0;
    memset(ctx, 0, sizeof(demo_ctx));
    if (parse_opt(argc, argv, ctx) < 1) {
        return 1;
    }
    pthread_mutex_init(&(ctx->mutex),NULL);
    pthread_cond_init(&(ctx->wait),NULL);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    ctx->threadId = (pthread_t *) malloc(ctx->threadNum * sizeof(pthread_t));
    ctx->list = clist_alloc(sizeof(msg_t));

    for (i = 0; i < ctx->threadNum; ++i) {
        pthread_create(&(ctx->threadId[i]), NULL, thread_proc, ctx);
    }
    
    thread_recv(ctx);

    for (i = 0; i < ctx->threadNum; ++i) {
        pthread_join(ctx->threadId[i], NULL);
    }
    return 0;
}