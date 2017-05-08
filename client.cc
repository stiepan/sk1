#include "boilerplate.h"

class AddrInfo {
  private:
    bool initialized;
  
  public:
    struct addrinfo hints, *info;
    
    AddrInfo(char const *hname, char const *sport) : initialized{false} {
      memset(&hints, 0, sizeof(struct addrinfo));
      hints.ai_family = AF_INET; // IPv4
      hints.ai_socktype = SOCK_DGRAM;
      hints.ai_protocol = IPPROTO_UDP;
      hints.ai_flags = AI_PASSIVE;
      int ret;
      if ((ret = getaddrinfo(hname, sport, &hints, &info)) != 0) { 
        std::cerr << "Error when calling getaddrinfo: " << gai_strerror(ret) << std::endl;
        exit(1);
      }
      initialized = true;
    }

    ~AddrInfo() {
      if (initialized) {
        freeaddrinfo(info);
      }
    }
};

int main(int argc, char *argv[]) {
  Message::initialize_serializer(std::string{});
  std::vector<char> buffer(Message::MAX_ENTRAILS_SIZE);

  /*Basic parsing of non-network arguments*/
  if (argc < 4 || argc > 5) {
    std::cerr << "Incorrect number of arguments" << std::endl;
    return 1;
  }
  if (std::string(argv[2]).length() != 1) {
    std::cerr << "Second argument ought to be a single character" << std::endl;
    return 1;
  }

  Message message(argv[1], *argv[2]);
  if (message.failed()) {
    std::cerr << "Incorrect timestamp - expected uint64_t" << std::endl;
    return 1;
  }
  if (!message.is_correct()) {
    std::cerr << "Incorrect timestamp, possibly out of years bound" << std::endl;
    return 1;
  }
  
  /*Parse networing arguments*/
  char const *hname = argv[3];
  std::string sport = "20160";

  if (argc == 5) {
    uint16_t port;
    sport = std::string(argv[4]);
    std::istringstream iss(argv[4]);
    iss >> port;
    if (iss.fail() || std::to_string(port).compare(argv[4]) != 0 || port == 0) {
      std::cerr << "Provided value is not a correct port number" << std::endl;
      return 1;
    }
  }

  /* Get host's IP address and prepare socket */
  AddrInfo addrinfo(hname, sport.c_str());

  int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    std::cerr << "Error when calling 'socket': " << strerror(errno) << std::endl;
    return 1;
  }

  /* Send message to the server */
  auto ssent = sendto(sockfd, message.serialize(), message.ssize(), 0,
    addrinfo.info->ai_addr, addrinfo.info->ai_addrlen);
  if (ssent != (int) message.ssize()) {
    std::cerr << "Partial write on UDP blocking socket. " 
      << "A real turn-up for the books." << std::endl;
  }

  /* Wait for messages from the server */
  struct sockaddr_in srvr_address;
  socklen_t rcva_len = (socklen_t) sizeof(srvr_address);
  size_t rcv_len;
  while (true) {
    rcv_len = recvfrom(sockfd, &buffer[0], Message::MAX_ENTRAILS_SIZE, 0,
      (struct sockaddr *) &srvr_address, &rcva_len);
    if (rcv_len < 0) {
      std::cerr << "Error while reading from socket: " << strerror(errno) << std::endl;
    }
    if (rcv_len == 0) {
      std::cerr << "Received an empty datagram from " 
        << addr_humanreadable(&srvr_address) << std::endl;
    }
    Message received(std::string(buffer.begin(), buffer.begin() + rcv_len));
    if (!received.is_correct()) {
      std::cerr << "Received an incorrect datagram from server "
        << addr_humanreadable(&srvr_address) << std::endl;
    }
    std::cout << received;
  }

  if (close(sockfd) == -1) { //very rare errors can occur here, but then
    std::cerr << "Error when calling 'close': " << strerror(errno) << std::endl;
    return 1;
  }

  return 0;
}
