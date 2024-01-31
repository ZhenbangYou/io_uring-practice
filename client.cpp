/* Async client that fetches multiple Google pages simultaneously */

#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <liburing.h>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <vector>

constexpr std::string
    IP_ADDR("142.251.46.228"); // IP address of `www.google.com`
constexpr uint16_t PORT = 80;
std::string GET_REQUEST(
    "GET / HTTP/1.0\r\n\r\n"); // Ask the other side to close the connection,
                               // otherwise there is no way to tell the message
                               // is complete
constexpr int BUFFER_SIZE = 1024 - 1;
constexpr std::string OUTPUT_DIR("./out/");

enum Status {
  CONNECTING,
  WRITING,
  READING,
};

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "expect 2 arguments" << std::endl;
    exit(EXIT_FAILURE);
  }
  const int num_pages = std::stoi(argv[1]);
  const int queue_depth = num_pages;

  std::system((std::string("rm -rf ") + OUTPUT_DIR).c_str());
  std::system((std::string("mkdir ") + OUTPUT_DIR).c_str());

  const auto start_time = std::chrono::system_clock::now();

  io_uring ring;
  if (io_uring_queue_init(queue_depth, &ring, 0) <
      0) { // Not using kernel polling to prevent race condition
    perror("io_uring_queue_init");
    exit(EXIT_FAILURE);
  }

  std::vector<int> socket_fds(num_pages);
  for (int i = 0; i < int(socket_fds.size()); i++) {
    socket_fds[i] = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    assert(socket_fds[i] >= 0);
  }

  // make sure they are valid until all `connect` are complete
  std::vector<sockaddr_in> server_addrs(num_pages);

  for (int i = 0; i < int(socket_fds.size()); i++) {
    auto sqe = io_uring_get_sqe(&ring);
    assert(sqe != nullptr);

    server_addrs[i].sin_family = AF_INET;
    server_addrs[i].sin_port = htons(PORT);
    if (inet_pton(AF_INET, IP_ADDR.c_str(), &server_addrs[i].sin_addr) <= 0) {
      perror("Invalid address/Address not supported");
      exit(EXIT_FAILURE);
    }
    io_uring_prep_connect(sqe, socket_fds[i],
                          reinterpret_cast<sockaddr *>(&server_addrs[i]),
                          sizeof(server_addrs[i]));
    sqe->user_data =
        i; // `user_data` is used to record the ID, i.e., index of `socket_fds`
  }
  io_uring_submit(&ring);

  int remaining_tasks = num_pages;
  std::vector<Status> id_status(num_pages, CONNECTING);
  std::vector<char *> buffers(num_pages);
  for (int i = 0; i < num_pages; i++) {
    buffers[i] = new char[BUFFER_SIZE + 1];
  }
  std::vector<std::string> cumulative_messages(num_pages);

  while (remaining_tasks > 0) {
    io_uring_cqe *cqe;
    auto cq_wait_res = io_uring_wait_cqe(&ring, &cqe);
    assert(cq_wait_res == 0);
    assert(cqe != nullptr);
    io_uring_cqe_seen(&ring, cqe);

    int id = int(cqe->user_data);
    int fd = socket_fds.at(id);
    int status = id_status.at(id);
    assert(id >= 0);

    // Mustn't get a new sqe when not needed
    // Super caution

    if (status == CONNECTING) {
      assert(cqe->res == 0); // return value of `connect`

      id_status[id] = WRITING;

      auto sqe = io_uring_get_sqe(&ring);
      io_uring_prep_send(sqe, fd, GET_REQUEST.c_str(), GET_REQUEST.length(), 0);
      sqe->user_data = id;
      io_uring_submit(&ring);
    } else if (status == WRITING) {
      assert(cqe->res == GET_REQUEST.length()); // the request is short enough
                                                // to be sent in one shot

      id_status[id] = READING;

      auto sqe = io_uring_get_sqe(&ring);
      io_uring_prep_recv(sqe, fd, buffers[id], BUFFER_SIZE, 0);
      sqe->user_data = id;
      io_uring_submit(&ring);
    } else if (status == READING) {
      assert(cqe->res >= 0); // return value of `recv`

      if (cqe->res > 0) {
        buffers[id][cqe->res] =
            '\0'; // `\0` won't be appended by `recv` automatically
        cumulative_messages.at(id) += buffers[id];
      }

      // Detecting short counts doesn't work, since the message can come in
      // multiple batches. That `recv` returns 0 means EOF, which means the
      // other side closes the connection,
      if (cqe->res == 0) {
        // CAUTION!! We don't need to submit a new task, so DON'T get a new
        // sqe!!!
        close(fd);
        remaining_tasks--;
      } else {
        auto sqe = io_uring_get_sqe(&ring);
        io_uring_prep_recv(sqe, fd, buffers[id], BUFFER_SIZE, 0);
        sqe->user_data = id;
        io_uring_submit(&ring);
      }
    } else {
      perror("Unrecognized case returned be cqe");
      exit(EXIT_FAILURE);
    }
  }

  for (auto buf_ptr : buffers) {
    delete[] buf_ptr;
  }

  const auto end_time = std::chrono::system_clock::now();
  const auto time_elapsed_millis =
      std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                            start_time);
  std::cout << "Time elapsed: " << time_elapsed_millis.count() << "ms"
            << std::endl;

  for (int i = 0; i < num_pages; i++) {
    if (cumulative_messages[i].empty()) {
      continue;
    }
    std::ofstream fout(OUTPUT_DIR + std::to_string(i) + ".html",
                       std::ios::binary | std::ios::trunc);
    fout << cumulative_messages[i];
  }
}
