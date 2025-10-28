#include "csapp.h"

#define MAX_STOCK 100
#define NUM_THREAD 30
#define SBUFSIZE 200

/* Thread */

typedef struct {
    int *buf;
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
} sbuf_t;

sbuf_t sbuf;

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

void *thread(void *vargp);
void handle_sigint(int sig);
void process_request(int connfd);

typedef struct Item {
    int ID;
    int left_stock;
    int price;
    int readcnt;
    struct Item* left;
    struct Item* right;
    sem_t mutex;
    sem_t w;
} Item;

Item *root = NULL; // root of binary tree

Item* insert(Item* root, int ID, int left_stock, int price);
void save_tree(Item* root, FILE *fp);
Item* Search(Item* root, int ID);
void Release(Item* node);
void save_stock_table();

void sbuf_init(sbuf_t *sp, int n){
    sp->buf = Calloc(n, sizeof(int));
    sp->n = n;
    sp->front = sp->rear = 0;
    Sem_init(&sp->mutex, 0, 1);
    Sem_init(&sp->slots, 0, n);
    Sem_init(&sp->items, 0, 0);
}

void sbuf_deinit(sbuf_t *sp){
    Free(sp->buf);
}

void sbuf_insert(sbuf_t *sp, int item){
    P(&sp->slots);
    P(&sp->mutex);
    sp->buf[(++sp->rear) % (sp->n)] = item;
    V(&sp->mutex);
    V(&sp->items);
}

int sbuf_remove(sbuf_t *sp){
    int item;
    P(&sp->items);
    P(&sp->mutex);
    item = sp->buf[(++sp->front) % (sp->n)];
    V(&sp->mutex);
    V(&sp->slots);
    return item;
}

Item* insert(Item* root, int ID, int left_stock, int price){
    if (root == NULL) {
        root = (Item*)malloc(sizeof(Item));
        root->left = NULL;
        root->right = NULL;
        root->ID = ID;
        root->left_stock = left_stock;
        root->price = price;
        root->readcnt = 0;
        Sem_init(&(root->mutex), 0, 1);
        Sem_init(&(root->w), 0, 1);
        return root;
    } else if (root->ID == ID) {
        // ID가 중복되었을 때 처리할 로직
    } else {
        if (ID < root->ID) {
            root->left = insert(root->left, ID, left_stock, price);
        } else {
            root->right = insert(root->right, ID, left_stock, price);
        }
    }
    return root;
}

void save_tree(Item* root, FILE *fp){
    if (root == NULL) {
        return;
    }
    fprintf(fp, "%d %d %d\n", root->ID, root->left_stock, root->price);
    save_tree(root->left, fp);
    save_tree(root->right, fp);
}

Item* Search(Item* root, int ID){
    if (root == NULL) {
        return NULL;
    }

    Item *cur = root;

    while (cur) {
        if (ID == cur->ID) {
            P(&(cur->mutex)); 
            cur->readcnt++; 
            if (cur->readcnt == 1) {
                P(&(cur->w));
            }
            V(&(cur->mutex)); 
            return cur;
        } else if (ID < cur->ID) {
            cur = cur->left;
        } else {
            cur = cur->right;
        }
    }
    return NULL;
}

void Release(Item* node) {
    P(&(node->mutex)); 
    node->readcnt--; 
    if (node->readcnt == 0) {
        V(&(node->w)); 
    }
    V(&(node->mutex)); 
}

void save_stock_table() { 
    FILE *fp = fopen("stock.txt", "w");
    save_tree(root, fp);
    fclose(fp);
}

void handle_sigint(int sig){
    save_stock_table();
    sbuf_deinit(&sbuf);
    exit(0);
}

void process_request(int connfd) {
    int n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        printf("Server received %d bytes\n", n);

		//Rio_writen(connfd,buf,MAXLINE);

        if (strcmp(buf, "show\n") == 0) {
            char str[MAXLINE] = {0};
            char tmp[50];
            Item* stack[MAX_STOCK];
            int top = -1;

            Item* cur = root;
            while (cur != NULL || top >= 0) {
                while (cur != NULL) {
                    P(&(cur->mutex));
                    cur->readcnt++;
                    if (cur->readcnt == 1) {
                        P(&(cur->w));
                    }
                    V(&(cur->mutex));

                    stack[++top] = cur;
                    cur = cur->left;
                }
                cur = stack[top--];

                sprintf(tmp, "%d %d %d\n", cur->ID, cur->left_stock, cur->price);
                strcat(str, tmp);

                P(&(cur->mutex));
                cur->readcnt--;
                if (cur->readcnt == 0) {
                    V(&(cur->w));
                }
                V(&(cur->mutex));

                cur = cur->right;
            }
            //strcat(str, "\n");
            Rio_writen(connfd, str, MAXLINE);
       } else if (strncmp(buf, "buy", 3) == 0) {
            int ID, amount;
            Item* bnode;
            buf[strlen(buf) - 1] = '\0';
    		for(int i = 0 ; i < 3 ; i++)buf[i] = ' ';
            sscanf(buf, "%d %d", &ID, &amount);
            bnode = Search(root, ID);
       
            if (bnode == NULL) {
                strcpy(buf, "Invalid stock ID\n");
                Rio_writen(connfd, buf, MAXLINE);
            } else if (bnode->left_stock < amount) {
                P(&(bnode->mutex));
                bnode->readcnt--;
                if((bnode->readcnt) == 0){
                    V(&(bnode->w));
                }
                V(&(bnode->mutex));
                strcpy(buf, "Not enough left stock\n");
                Rio_writen(connfd, buf, MAXLINE);
            } else {
                P(&(bnode->mutex));
                bnode->readcnt--;
                if((bnode->readcnt) == 0){
                    V(&(bnode->w));
                }
                V(&(bnode->mutex));
                P(&(bnode->w));
                bnode->left_stock -= amount;
                V(&(bnode->w));
                strcpy(buf, "[buy] success\n");
                Rio_writen(connfd, buf, MAXLINE);
            }   
            //save_stock_table();
        } else if (strncmp(buf, "sell", 4) == 0) {
            Item* snode;
            int ID, amount;
            buf[strlen(buf) - 1] = '\0';
	    	for(int i = 0 ; i < 4 ; i++)buf[i] = ' ';
            sscanf(buf, "%d %d", &ID, &amount);
            snode = Search(root, ID);
            if (snode == NULL) {
                strcpy(buf, "Invalid stock ID\n");
                Rio_writen(connfd, buf, MAXLINE);
            } else {
                P(&(snode->mutex)); 
				snode->readcnt--;
				if((snode->readcnt) == 0)V(&(snode->w)); 
				V(&(snode->mutex)); 
				P(&(snode->w)); 
                snode->left_stock += amount;
                V(&(snode->w));
                strcpy(buf, "[sell] success\n");
                Rio_writen(connfd, buf, MAXLINE);
            }
            //save_stock_table();
        } else {
            save_stock_table();
            exit(0);
        }
    }
    //save_stock_table();
}

void *thread(void *vargp){
    Pthread_detach(pthread_self());
    while (1) {
        int connfd = sbuf_remove(&sbuf);
        process_request(connfd);
        Close(connfd);
    }
}



int main(int argc, char **argv) { 
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;

    FILE *fp = NULL;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    Signal(SIGINT, handle_sigint);

    fp = fopen("stock.txt", "r");
    if (fp == NULL) {
        printf("Error: stock.txt file does not exist.\n");
        return 0;
    } else { 
        int ID, left_stock, price;
        while (fscanf(fp, "%d %d %d\n", &ID, &left_stock, &price) != EOF) {
            root = insert(root, ID, left_stock, price);
        }
        fclose(fp);
    }

    listenfd = Open_listenfd(argv[1]);
    sbuf_init(&sbuf, SBUFSIZE);

    for (int i = 0; i < NUM_THREAD; i++) {
        Pthread_create(&tid, NULL, thread, NULL);
    }

    while (1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);
        sbuf_insert(&sbuf, connfd);
    }

    save_stock_table();
    sbuf_deinit(&sbuf);
    exit(0);
}
