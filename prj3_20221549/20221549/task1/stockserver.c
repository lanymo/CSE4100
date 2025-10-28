#include "csapp.h"

#define MAX_STOCK 100

typedef struct {
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
}pool;


typedef struct Item{
    int ID;
    int left_stock;
    int price;
    //int readcnt;
    struct Item* left;
    struct Item* right;
    //sem_t mutex;
}Item;

Item *root = NULL; // root of binary tree

Item* insert(Item* root, int ID, int left_stock, int price){
    if(root==NULL){
        root = (Item*)malloc(sizeof(Item));
        root->left = NULL;
        root->right = NULL;
        root->ID = ID;
        root->left_stock = left_stock;
        root->price = price;
        return root;
    }
    else if(root->ID == ID){
        return root;
    }else{
        if(ID < root->ID){
            root->left = insert(root->left, ID, left_stock, price);
        }
        else{
            root->right = insert(root->right, ID, left_stock, price);
        }
    }

    return root;
}

void save_tree(Item* root, FILE *fp){
    if (root == NULL){
        return;
    }
    fprintf(fp, "%d %d %d\n", root->ID, root->left_stock, root->price);
    save_tree(root->left, fp);
    save_tree(root->right, fp);
    //return;
}

Item* Search(Item* root, int ID){
    if(root==NULL){
        return NULL;
    }
    else if(root->ID == ID){
        return root;
    }
    else if(root->ID < ID){
        return Search(root->right, ID);
    }
    else{
        return Search(root->left, ID);
    }
}

void save_stock_table() { 
    FILE *fp;
    fp = fopen("stock.txt", "w");
    save_tree(root, fp);
    fclose(fp);
    //exit(1);
}

void handle_sigint(int sig){
    save_stock_table();
    exit(0);
}

void process_request(int connfd, char* buf, int n, pool* p) {
    int ID, amount;

   // Rio_writen(connfd,buf,MAXLINE);

    if (strcmp(buf, "show\n") == 0) {
        char str[MAXLINE] = {0};
        char tmp[15];
        Item* stack[MAX_STOCK];
        int top = -1;

        Item* cur = root;
        while (cur != NULL || top >= 0) {
            while (cur != NULL) {
                stack[++top] = cur;
                cur = cur->left;
            }
            cur = stack[top--];

            sprintf(tmp, "%d %d %d\n", cur->ID, cur->left_stock, cur->price);
            strcat(str, tmp);

            cur = cur->right;
        }
        //strcat(str, "\n");
        Rio_writen(connfd, str, MAXLINE);
    } else if (strncmp(buf, "buy", 3) == 0) {
        Item* bnode;
        buf[strlen(buf) - 1] = '\0';
		for(int i = 0 ; i < 3 ; i++)buf[i] = ' ';
        sscanf(buf, "%d %d", &ID, &amount);
        bnode = Search(root, ID);
        if (bnode == NULL) {
            strcpy(buf, "Invalid stock ID\n");
            Rio_writen(connfd, buf, MAXLINE);
        } else if (bnode->left_stock < amount) {
            strcpy(buf, "Not enough left stock\n");
           Rio_writen(connfd, buf, MAXLINE);
        } else {
            bnode->left_stock -= amount;
            strcpy(buf, "[buy] success\n");
            Rio_writen(connfd, buf, MAXLINE);
        }
    } else if (strncmp(buf, "sell", 4) == 0) {
        Item* snode;
        buf[strlen(buf) - 1] = '\0';
		for(int i = 0 ; i < 4 ; i++)buf[i] = ' ';
        sscanf(buf, "%d %d", &ID, &amount);
        snode = Search(root, ID);
        if (snode == NULL) {
            strcpy(buf, "Invalid stock ID\n");
            Rio_writen(connfd, buf, MAXLINE);
        } else {
            snode->left_stock += amount;
            strcpy(buf, "[sell] success\n");
            Rio_writen(connfd, buf, MAXLINE);
        }
    } else {
        save_stock_table();
        exit(0);
    }
}

void init_pool(int listenfd, pool *p) { // initialize pool
    int i;
    p->maxi = -1;
    for (i = 0; i < FD_SETSIZE; i++)
        p->clientfd[i] = -1;

    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p) { // add client to the pool
    int i;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++) {
        if (p->clientfd[i] < 0) {
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);
            FD_SET(connfd, &p->read_set);
            if (connfd > p->maxfd) p->maxfd = connfd;
            if (i > p->maxi) p->maxi = i;
            break;
        }
    }
    if (i == FD_SETSIZE)
        app_error("add_client error : Too many clients");
}

void check_clients(pool *p) { // interaction with ready descriptor
    int i, connfd, n;
    char buf[MAXLINE];
    rio_t rio;

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];
        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) { // check ready descriptor
            p->nready--;
            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) { // read text line
                printf("Server received %d bytes\n", n);
                process_request(connfd, buf, n, p);
            } else {
                Close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
            }
        }
    }
}

int main(int argc, char **argv) { 

    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */
    char client_hostname[MAXLINE], client_port[MAXLINE];
    static pool pool;

    FILE *fp = NULL;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    Signal(SIGINT, handle_sigint);

    fp = fopen("stock.txt", "r");
    if (fp == NULL) {
        printf("Error : stock.txt file does not exist.\n");
        return 0;
    } else { 
        int ID, left_stock, price;

         while (fscanf(fp, "%d %d %d\n", &ID, &left_stock, &price) != EOF){
            root = insert(root, ID, left_stock, price);    
        }
        fclose(fp);
    }

    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);

    while (1) {
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);
        if (FD_ISSET(listenfd, &pool.ready_set)) { // add client to the pool
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);
            add_client(connfd, &pool);
        }
        check_clients(&pool);
    }

    save_stock_table();
    exit(0);
}
