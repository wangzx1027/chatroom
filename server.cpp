#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <set>

std::set<int> clients;

void boardcast(int clintfd, const char* msg, size_t len) {
	for (auto it = clients.begin(); it != clients.end(); ++it) {
		if (*it != clintfd) {
			write(*it, msg, len);
		}
	}
}

void server() {
    int serv_sock, clnt_sock;
	sockaddr_in serv_adr, clnt_adr;
	socklen_t clnt_sz;
	char buf[5] = "";
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(9527);
    if (bind(serv_sock, (sockaddr*)&serv_adr, sizeof(serv_adr)) == -1) {
        printf("bind error %d %s", errno, strerror(errno));
        close(serv_sock);
        return;
    }
    if (listen(serv_sock, 5) == -1) {
		printf("listen error %d %s", errno, strerror(errno));
		close(serv_sock);
		return;
	}
    epoll_event event;
	int epfd, event_cnt;
	epfd = epoll_create(1);
	if (epfd == -1) {
		printf("epoll_create error %d %s", errno, strerror(errno));
		close(serv_sock);
		return;
	}
    epoll_event* all_events = new epoll_event[100];
    event.events = EPOLLIN;
	event.data.fd = serv_sock;
	epoll_ctl(epfd, EPOLL_CTL_ADD, serv_sock, &event);
    while (true) {
        event_cnt = epoll_wait(epfd, all_events, 100, 1000);
        if (event_cnt == -1) {
            printf("epoll_wait error %d %s", errno, strerror(errno));
			break;
        }
        if (event_cnt == 0) continue;
        for (int i = 0; i < event_cnt; i++) {
            if (all_events[i].data.fd == serv_sock) {
                clnt_sz = sizeof(clnt_adr);
                clnt_sock = accept(serv_sock, (sockaddr*)&clnt_adr, &clnt_sz);
                event.events = EPOLLIN | EPOLLET;
				event.data.fd = clnt_sock;
				int flag = fcntl(clnt_sock, F_GETFL, 0);
				clients.insert(clnt_sock);
				fcntl(clnt_sock, F_SETFL, flag | O_NONBLOCK);
				epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event);
				printf("client is connected!%d\n", clnt_sock);
            } else {
                while (true) {
                    ssize_t len = read(all_events[i].data.fd, buf, sizeof(buf));
					if (len < 0) {
						if (errno == EAGAIN)
							break;
						printf("epoll_wait error %d %s", errno, strerror(errno));
						close(all_events[i].data.fd);
						clients.erase(all_events[i].data.fd);
						break;
					}
					else if (len == 0) {
						epoll_ctl(epfd, EPOLL_CTL_DEL, all_events[i].data.fd, NULL);
						close(all_events[i].data.fd);
						clients.erase(all_events[i].data.fd);
						printf("client is closed!%d\n", all_events[i].data.fd);
						break;
					}
					else {
						printf("%s(%d):%s %c\n", __FILE__, __LINE__, __FUNCTION__, buf[0]);
						boardcast(all_events[i].data.fd, buf, len);
						// write(all_events[i].data.fd, buf, len);
					}
                }
            }
        }
        printf("%s(%d):%s \n", __FILE__, __LINE__, __FUNCTION__);
    }
    delete[]all_events;
	close(serv_sock);
	close(epfd);
}

int main() {
    server();
    return 0;
}