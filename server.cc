#include <fstream>
#include <unistd.h>
#include <poll.h> 
#include <fcntl.h>
#include <tuple>
#include <queue>
#include "boilerplate.h"

class Communication {
  public:
    using Client = std::tuple<time_t, sockaddr_in>;
    using Datagram = std::tuple<sockaddr_in, Message>;

    std::vector<Client> clients;
    //was the first message from queue sent to ith client
    std::vector<bool> msent; 
    std::queue<Datagram> messages;

    size_t client_it;
  
    bool prepare_next_message() {
      while (!messages.empty()) {
        for (size_t i = 0; i < clients.size(); i++) {
          if (msent[i]) {
            continue;
          }
          auto client_addr = addr_humanreadable(&std::get<1>(clients[i]));
          auto mess_addr = addr_humanreadable(&std::get<0>(messages.front()));
          if (client_addr.compare(mess_addr) == 0) {
            msent[i] = true;
            continue;
          }
          client_it = i;
          return true;
        }
        for (size_t i = 0; i < clients.size(); i++) {
          msent[i] = false;
        }
        messages.pop();
      }
      return false;
    }

    int find_client(sockaddr_in &addr) {
      for (size_t i = 0; i < clients.size(); i++) {
        auto client_addr = addr_humanreadable(&std::get<1>(clients[i]));
        auto saddr = addr_humanreadable(&addr);
        if (client_addr.compare(saddr) == 0) {
          return i;
        }
      }
      return -1;
    }

    bool add_client(time_t const &t, sockaddr_in const &addr) {
      if (clients.size() < 42) {
        clients.push_back(std::make_tuple(t, addr));
        msent.push_back(false);
        return true;
      }
      return false;
    }

    void remove_obsolete() {
      time_t now = std::time(nullptr);
      for (size_t i = 0; i < clients.size(); i++) {
        if (std::difftime(now, std::get<0>(clients[i])) > 120) {
          if (i < clients.size() - 1) {
            clients[i] = std::move(clients.back());
            msent[i] = std::move(msent.back());
          }
          clients.pop_back();
          msent.pop_back();
        }
      }
    }
};

bool finish = false;

void catch_int (int sig) {
  finish = true;
}

int main(int argc, char *argv[]) {

  if (argc != 3) {
    std::cerr << "Incorrect number of arguments" << std::endl;
    return 1;
  } 

  /* Verify provided port number */
  uint16_t port;
  std::istringstream iss(argv[1]);
  iss >> port;
  if (iss.fail() || std::to_string(port).compare(argv[1]) != 0 || port == 0) {
    std::cerr << "Provided value is not a correct port number" << std::endl;
    return 1;
  }

  /* Read the file and pass to the serializer */
  std::ifstream in(argv[2], std::ios::in | std::ios::binary);
  if (!in) {
    std::cerr << "Eror while opening file: " << strerror(errno) << std::endl;
    return 1;
  }
  std::string contents;
  in.seekg(0, std::ios::end);
  // roughly cut off blatantly oversized files
  if (in.tellg() >= (int) Message::MAX_ENTRAILS_SIZE) {
    std::cerr << "Provided file is too big to fit in UDP datagram" << std::endl;
    return 1;
  }
  contents.resize(in.tellg());
  in.seekg(0, std::ios::beg);
  in.read(&contents[0], contents.size());
  in.close();
  
  Message::initialize_serializer(contents);
  std::vector<char> buffer(Message::MAX_ENTRAILS_SIZE);

  /* Adjust signal handling */
  if (signal(SIGINT, catch_int) == SIG_ERR) {
    std::cerr << "Couldn't change signal handling" << std::endl;
  }

  /* Prepare socket */
	struct sockaddr_in server_addr;

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
  if (sockfd < 0) {
		std::cerr << "Error when calling 'socket': " << strerror(errno) << std::endl;
    return 1;
	}
	if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0) {	
		std::cerr << "Error when calling 'fcntl': " << strerror(errno) << std::endl;
		return 1;
	}

  server_addr.sin_family = AF_INET; // IPv4
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
  server_addr.sin_port = htons(port);

  // bind the socket to a concrete address
  if (bind(sockfd, (struct sockaddr *) &server_addr,
			(socklen_t) sizeof(server_addr)) < 0) {
		std::cerr << "Error when calling 'bind': " << strerror(errno) << std::endl;
    return 1;
	}
	
	/* Start actual communication */
	bool want_to_write = false;
	struct pollfd pollsocket;
	pollsocket.fd = sockfd;
	int ret;
	size_t rcv_len;
	struct sockaddr_in client_addr;
  socklen_t rcva_len = (socklen_t) sizeof(client_addr);
  Communication com;

	while (!finish) {
		pollsocket.events = want_to_write? (POLLIN | POLLOUT) : POLLIN;
		pollsocket.revents = 0;
		ret = poll(&pollsocket, 1, -1);
    com.remove_obsolete();
		if (ret < 0) {
			break; //poll error or signal
		}
		else if (ret > 0) {
      // Reading part first
      if (pollsocket.revents & (POLLIN | POLLERR)) {
        //partial read cannot happen on UDP socket
        rcv_len = recvfrom(sockfd, &buffer[0], Message::MAX_ENTRAILS_SIZE, 0,
          (struct sockaddr *) &client_addr, &rcva_len);
        time_t rec_time = std::time(nullptr);
        if (rcv_len < 0) {
          std::cerr << "Error while reading from socket: " << strerror(errno) << std::endl;
        }
        if (rcv_len == 0) {
          std::cerr << "Received an empty datagram from " 
            << addr_humanreadable(&client_addr) << std::endl;
        }
        else {
          Message received(std::string(buffer.begin(), buffer.begin() + rcv_len));
          if (!received.is_correct()) {
            std::cout << "Incorrect message from " << addr_humanreadable(&client_addr) << std::endl;
          }
          else {
            int ci = com.find_client(client_addr);
            bool added = false;
            if (ci >= 0) {
              // possible starvation of ones that are not connected
              // but we want the ones in queue be privileged
              std::get<0>(com.clients[ci]) = rec_time; 
            }
            else {
              added = com.add_client(rec_time, client_addr);
            }
            if (ci >= 0 || added) {
              auto nmes = std::make_tuple(client_addr, received);
              if (com.messages.size() < 4096) {
                com.messages.push(nmes);
              }
              else {
                com.messages.back() = nmes;
              }
            }
          }
        }
      }
      // And then writing
      if (pollsocket.revents & (POLLOUT)) {
        if (com.prepare_next_message()) { // there is something to be sent 
          Message &tosend = std::get<1>(com.messages.front());
          sockaddr_in tosendaddr = std::get<1>(com.clients[com.client_it]);
          auto ssent = sendto(sockfd, tosend.serialize(), tosend.ssize(), 0, 
            (struct sockaddr*) &tosendaddr, sizeof(tosendaddr));
          if (ssent >= 0) {
            com.msent[com.client_it] = true;
          }
          else {
            // if there was not enough space in kernel buffer just try again next time,
            // otherwise drop particular message
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
              std::cerr << "Error when writing to socket on address "
                << addr_humanreadable(&tosendaddr) << std::endl; 
              com.msent[com.client_it] = true;
            }
          }
        }
      }
      // If there's still something remianing to send
      want_to_write = com.prepare_next_message();
		}
	}

	/* Out of courtesy */
  if (close(sockfd) == -1) { //very rare errors can occur here, but then
    std::cerr << "Error when calling 'close': " << strerror(errno) << std::endl;
    return 1;
  }

  return 0;
}
