#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<stdbool.h>
#include<ctype.h>
#include<netinet/in.h>
#include<sys/stat.h>
#include<sys/socket.h>

#define BACKLOG 5

int init(int port);

void unimplemented(int client_sock_fd);

void not_found(int client_sock_fd);

void bad_request(int client_scok_fd);

void cannot_execute(int client);

void cat(int client_sock_fd, FILE *resource);

int get_line(int client_sock_fd, char *buf, int size);

void header(int client_sock_fd, char *content_type);

void serve_file(char *path, int client_sock_fd);

void execute_cgi(int client_sock_fd, const char *path, const char *method, const char *query_string);

void process_request(int client_sock_fd);

int init(int port) {

    int sock_fd = 0;
    struct sockaddr_in server_addr;

    sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(sock_fd, BACKLOG) < 0) {
        perror("listen");
        exit(1);
    }

    return sock_fd;
}

void process_request(int client_sock_fd) {

    char method[255];
    char url[255];
    char path[255];

    char buf[BUFSIZ];
    int numchars;

    bool cgi = false;
    char *query_string = NULL;

    numchars = get_line(client_sock_fd, buf, sizeof(buf));

    int curr = 0;
    int i = 0;

    while ((!isspace(buf[i])) && (i < sizeof(method) - 1)) {
        method[i] = buf[i];
        i++;
        curr++;
    }
    method[i] = '\0';

    if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
        unimplemented(client_sock_fd);
        close(client_sock_fd);
        return;
    }

    if (strcasecmp(method, "POST") == 0) {
        cgi = true;
    }

    while ((isspace(buf[curr])) && (curr < numchars)) {
        curr++;
    }

    i = 0;
    while ((!isspace(buf[curr])) && (curr < numchars) && (i < sizeof(url) - 1)) {
        url[i] = buf[curr];
        i++;
        curr++;
    }
    url[i] = '\0';

    if (strcasecmp(method, "GET") == 0) {
        query_string = url;

        while ((*query_string != '?') && (*query_string != '\0')) {
            query_string++;
        }

        if (*query_string == '?') {
            cgi = true;
            *query_string = '\0';
            query_string++;
        }
    }

    sprintf(path, "htdoc%s", url);
    if (path[strlen(path) - 1] == '/') {
        strcat(path, "index.html");
    }

    struct stat st;
    if (stat(path, &st) == -1) {
        while ((numchars > 0) && strcmp("\n", buf)) {
            numchars = get_line(client_sock_fd, buf, sizeof(buf));
        }
        not_found(client_sock_fd);
    } else {

        if (S_ISDIR(st.st_mode)) {
            strcat(path, "index.html");
        }

        if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXGRP)) {
            cgi = true;
        }

        if (!cgi) {
            serve_file(path, client_sock_fd);
        } else {
            execute_cgi(client_sock_fd, path, method, query_string);
        }
    }


    close(client_sock_fd);
}


int get_line(int client_sock_fd, char *buf, int size) {
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n')) {
        n = recv(client_sock_fd, &c, 1, 0);
        if (n > 0) {
            if (c == '\r') {
                n = recv(client_sock_fd, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n')) {
                    recv(client_sock_fd, &c, 1, 0);
                } else {
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        } else {
            c = '\n';
        }
    }
    buf[i] = '\0';
    return i;
}

void unimplemented(int client_sock_fd) {
    char buf[BUFSIZ];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
}

void bad_request(int client_sock_fd) {
    char buf[BUFSIZ];
    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client_sock_fd, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client_sock_fd, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client_sock_fd, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client_sock_fd, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client_sock_fd, buf, sizeof(buf), 0);
}

void not_found(int client_sock_fd) {
    char buf[BUFSIZ];
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
}

void header(int client_sock_fd, char *content_type) {
    char buf[BUFSIZ];
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: %s\r\n", content_type);
    send(client_sock_fd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);
}


char *file_type(char *path) {
    char *cp;
    if ((cp = strrchr(path, '.')) != NULL) {
        return cp + 1;
    }
    return "";
}

void cannot_execute(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

void serve_file(char *path, int client_sock_fd) {
    char *extension = file_type(path);
    char *content_type = "text/plain";
    int c;

    if (strcmp(extension, "html") == 0) {
        content_type = "text/html";
    } else if (strcmp(extension, "pdf") == 0) {
        content_type = "application/pdf";
    } else if (strcmp(extension, "gif") == 0) {
        content_type = "image/gif";
    } else if (strcmp(extension, "jgp") == 0) {
        content_type = "image/jpg";
    } else if (strcmp(extension, "jepg") == 0) {
        content_type = "image/jpeg";
    }

    FILE *resource = NULL;
    int numchars = 1;
    char buf[BUFSIZ];

    buf[0] = 'A';
    buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf)) {
        numchars = get_line(client_sock_fd, buf, sizeof(buf));
    }

    resource = fopen(path, "r");
    if (resource == NULL) {
        not_found(client_sock_fd);
    } else {
        header(client_sock_fd, content_type);
        cat(client_sock_fd, resource);
    }

    fclose(resource);
}

void cat(int client_sock_fd, FILE *resource) {
    char buf[BUFSIZ];

    fgets(buf, sizeof(buf), resource);

    while (!feof(resource)) {
        send(client_sock_fd, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

void execute_cgi(int client_sock_fd, const char *path, const char *method, const char *query_string) {
    char buf[BUFSIZ];
    int cgi_output[2];
    int cgi_input[2];
    pid_t pid;
    int status;
    int i;
    int c;
    int numchars = 1;
    int content_length = -1;

    buf[0] = 'A';
    buf[1] = '\0';
    if (strcasecmp(method, "GET") == 0) {
        while ((numchars > 0) && strcmp("\n", buf))
            numchars = get_line(client_sock_fd, buf, sizeof(buf));
    } else if (strcmp(method, "POST") == 0) {
        numchars = get_line(client_sock_fd, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf)) {
            buf[15] = '\0';
            if (strcmp(buf, "Content-Length:") == 0) {
                content_length = atoi(&(buf[16]));
                numchars = get_line(client_sock_fd, buf, sizeof(buf));
            }
        }
        if (content_length == -1) {
            bad_request(client_sock_fd);
            return;
        }
    } else {

    }
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client_sock_fd, buf, strlen(buf), 0);

    if (pipe(cgi_output) < 0) {
        cannot_execute(client_sock_fd);
        return;
    }
    if (pipe(cgi_input) < 0) {
        cannot_execute(client_sock_fd);
        return;
    }

    if ((pid = fork()) < 0) {
        cannot_execute(client_sock_fd);
        return;
    }

    if (pid == 0) {
        char methd_env[255];
        char query_env[255];
        char length_env[255];


        dup2(cgi_output[1], 1);
        dup2(cgi_input[0], 0);

        close(cgi_output[0]);
        close(cgi_input[1]);

        sprintf(methd_env, "REQUEST_METHOD=%s", method);
        putenv(methd_env);

        if (strcasecmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        } else {
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        printf("start\n");
        execl("/usr/bin/python3","python3", path, NULL);
        printf("end\n");
        exit(0);
    } else {
        close(cgi_output[1]);
        close(cgi_input[0]);

        if (strcasecmp(method, "POST") == 0)
            for (i = 0; i < content_length; i++) {
                recv(client_sock_fd, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }

        while (read(cgi_output[0], &c, 1) > 0) {
            send(client_sock_fd, &c, 1, 0);
        }

        close(cgi_output[0]);
        close(cgi_input[1]);
        waitpid(pid, &status, 0);
    }
}


int main(int argc, char const *argv[]) {

    int server_sock_fd = -1;
    server_sock_fd = init(8080);
    if (server_sock_fd == -1) {
        perror("init");
        exit(1);
    }

    struct sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof(client_addr);
    int client_sock_fd = -1;

    while (true) {

        if ((client_sock_fd = accept(server_sock_fd, (struct sockaddr *) &client_addr, &client_addr_size)) == -1) {
            perror("accpet");
            exit(1);
        }

        process_request(client_sock_fd);
    }

    return 0;
}