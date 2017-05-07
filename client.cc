#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "boilerplate.h"

int main(int argc, char *argv[]) {

  /*Basic parsing of non-network arguments*/
  if (argc < 4 || argc > 5) {
    std::cerr << "Incorrect number of arguments" << std::endl;
    return 1;
  }
  if (std::string(argv[2]).length() != 1) {
    std::cerr << "Second argument ought to be a single character" << std::endl;
    return 1;
  }

  Message m(argv[1], *argv[2]);
  if (m.failed()) {
    std::cerr << "Incorrect timestamp - expected uint64_t" << std::endl;
    return 1;
  }
  if (!m.is_correct()) {
    std::cerr << "Incorrect timestamp, possibly out of years bound" << std::endl;
    return 1;
  }
  
  /*Prepare socket in accordance with passed arguments*/

  char const *hname = argv[3];
  uint16_t port = 20160;

  if (argc == 5) {
    std::istringstream iss(argv[4]);
    iss >> port;
    if (iss.fail() || port == 0) {
      std::cerr << "Provided value is not a correct port number" << std::endl;
    }
  }

  int sock;
  struct addrinfo addr_hints;
  struct addrinfo *addr_result;

  //int i, flags, sflags;
  //size_t len;
  //ssize_t snd_len, rcv_len;
  struct sockaddr_in my_address;
  struct sockaddr_in srvr_address;
  //socklen_t rcva_len;

  // 'converting' host/port in string to struct addrinfo
  (void) memset(&addr_hints, 0, sizeof(struct addrinfo));
  addr_hints.ai_family = AF_INET; // IPv4
  addr_hints.ai_socktype = SOCK_DGRAM;
  addr_hints.ai_protocol = IPPROTO_UDP;
  addr_hints.ai_flags = 0;
  addr_hints.ai_addrlen = 0;
  addr_hints.ai_addr = NULL;
  addr_hints.ai_canonname = NULL;
  addr_hints.ai_next = NULL;
  if (getaddrinfo(argv[1], NULL, &addr_hints, &addr_result) != 0) {
    syserr("getaddrinfo");
  }

  my_address.sin_family = AF_INET; // IPv4
  my_address.sin_addr.s_addr =
      ((struct sockaddr_in*) (addr_result->ai_addr))->sin_addr.s_addr; // address IP
  my_address.sin_port = htons((uint16_t) atoi(argv[2])); // port from the command line

  freeaddrinfo(addr_result);

  sock = socket(PF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
    syserr("socket");

  for (i = 3; i < argc; i++) {
    len = strnlen(argv[i], BUFFER_SIZE);
    if (len == BUFFER_SIZE) {
      (void) fprintf(stderr, "ignoring long parameter %d\n", i);
      continue;
    }
    (void) printf("sending to socket: %s\n", argv[i]);
    sflags = 0;
    rcva_len = (socklen_t) sizeof(my_address);
    snd_len = sendto(sock, argv[i], len, sflags,
        (struct sockaddr *) &my_address, rcva_len);
    if (snd_len != (ssize_t) len) {
      syserr("partial / failed write");
    }

    (void) memset(buffer, 0, sizeof(buffer));
    flags = 0;
    len = (size_t) sizeof(buffer) - 1;
    rcva_len = (socklen_t) sizeof(srvr_address);
    rcv_len = recvfrom(sock, buffer, len, flags,
        (struct sockaddr *) &srvr_address, &rcva_len);

    if (rcv_len < 0) {
      syserr("read");
    }
    (void) printf("read from socket: %zd bytes: %s\n", rcv_len, buffer);
  }

  if (close(sock) == -1) { //very rare errors can occur here, but then
    syserr("close"); //it's healthy to do the check
  }*/

  return 0;
}
