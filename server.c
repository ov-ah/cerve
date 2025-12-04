#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/stat.h>

void main() {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        AF_INET,
        0x901f,
        0
    };

    bind(s, (struct sockaddr *)&addr, sizeof(addr));
   	listen(s, 10);
	
	while(1) {
    	int client_fd = accept(s, 0, 0);
		if(client_fd < 0) continue;

    	char buffer[4096] = {0};
   		recv(client_fd, buffer, 256, 0);

    	// GET /file.html .....
		
		char* f = buffer + 5;
    	*strchr(f, ' ') = 0;
    	int opened_fd = open(f, O_RDONLY);
    	
		struct stat st;
		fstat(opened_fd, &st);
		
		char header[256];
		int header_len = snprintf(header, sizeof(header),
			"HTTP/1.1 200 OK\r\n"
    		"Content-Length: %ld\r\n"
    		"Content-Type: text/html\r\n"
    		"\r\n",
			st.st_size);
		
		send(client_fd, header, header_len, 0);
		sendfile(client_fd, opened_fd, 0, st.st_size);
    	
		close(client_fd);
		close(opened_fd);
	};
	close(s);
}
