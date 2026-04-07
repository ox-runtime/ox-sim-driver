#pragma once

#include <atomic>
#include <memory>
#include <thread>

namespace crow {
template <typename... Middlewares>
class Crow;
using SimpleApp = Crow<>;
}  // namespace crow

namespace ox_sim {

inline constexpr int kHttpServerPort = 8765;

class HttpServer {
   public:
    HttpServer();
    ~HttpServer();

    void SetPort(int port);
    bool Start();
    void Stop();
    bool IsRunning() const { return running_.load(); }
    int port() const { return port_; }

   private:
    void ServerThread();

    std::atomic<bool> running_{false};
    std::atomic<bool> should_stop_{false};
    std::unique_ptr<crow::SimpleApp> app_;
    std::thread server_thread_;
    int port_ = kHttpServerPort;
};

HttpServer& GetHttpServer();

}  // namespace ox_sim