#include "Network.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <climits>
#include <cstdint>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unordered_map>
#include <mutex>

namespace {
    SSL_CTX* g_server_ctx = nullptr;
    SSL_CTX* g_client_ctx = nullptr;
    std::unordered_map<int, SSL*> g_fd_ssl;
    std::mutex g_ssl_mutex;  // Thread safety for g_fd_ssl

    bool is_tls(int fd) { 
        std::lock_guard<std::mutex> lock(g_ssl_mutex);
        return g_fd_ssl.find(fd) != g_fd_ssl.end(); 
    }

    void log_errors(const char* tag) {
        unsigned long e;
        while ((e = ERR_get_error()) != 0) {
            char buf[256];
            ERR_error_string_n(e, buf, sizeof(buf));
            std::cerr << tag << ": " << buf << "\n";
        }
    }

    ssize_t ssl_read_all(SSL* ssl, char* buf, size_t len) {
        size_t total = 0;
        while (total < len) {
            int r = SSL_read(ssl, buf + total, (int)(len - total));
            if (r <= 0) 
                return (total == 0) ? r : (ssize_t)total;
            total += (size_t)r;
        }
        return (ssize_t)total;
    }

    ssize_t ssl_write_all(SSL* ssl, const char* buf, size_t len) {
        size_t total = 0;
        while (total < len) {
            int r = SSL_write(ssl, buf + total, (int)(len - total));
            if (r <= 0) 
                return (total == 0) ? r : (ssize_t)total;
            total += (size_t)r;
        }
        return (ssize_t)total;
    }
}

// TLS init (server)
int Network::init_server_tls(const std::string& cert_path, const std::string& key_path) {
    if (g_server_ctx) return 0;
    
    // Note: SSL_library_init() is deprecated in OpenSSL 1.1.0+
    // OPENSSL_init_ssl() is called automatically in newer versions
    #if OPENSSL_VERSION_NUMBER < 0x10100000L
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    #endif
    
    const SSL_METHOD* method = TLS_server_method();
    g_server_ctx = SSL_CTX_new(method);
    if (!g_server_ctx) { 
        log_errors("SSL_CTX_new server"); 
        return -1; 
    }
    SSL_CTX_set_min_proto_version(g_server_ctx, TLS1_2_VERSION);
    if (SSL_CTX_use_certificate_file(g_server_ctx, cert_path.c_str(), SSL_FILETYPE_PEM) != 1) { 
        log_errors("cert"); 
        return -1; 
    }
    if (SSL_CTX_use_PrivateKey_file(g_server_ctx, key_path.c_str(), SSL_FILETYPE_PEM) != 1) { 
        log_errors("key"); 
        return -1; 
    }
    if (SSL_CTX_check_private_key(g_server_ctx) != 1) { 
        std::cerr << "Cert/key mismatch\n"; 
        return -1; 
    }
    return 0;
}

// TLS init (client)
int Network::init_client_tls(bool verify_peer) {
    if (g_client_ctx) return 0;
    
    #if OPENSSL_VERSION_NUMBER < 0x10100000L
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    #endif
    
    const SSL_METHOD* method = TLS_client_method();
    g_client_ctx = SSL_CTX_new(method);
    if (!g_client_ctx) { log_errors("SSL_CTX_new client"); return -1; }
    
    if (verify_peer) {
        // Production mode: verify certificates
        if (SSL_CTX_set_default_verify_paths(g_client_ctx) != 1) { 
            std::cerr << "CA paths load failed\n"; 
        }
        SSL_CTX_set_verify(g_client_ctx, SSL_VERIFY_PEER, nullptr);
    } else {
        // Development mode: skip verification for self-signed certs
        SSL_CTX_set_verify(g_client_ctx, SSL_VERIFY_NONE, nullptr);
        std::cout << "WARNING: TLS certificate verification disabled (development mode)\n";
    }
    
    return 0;
}

// Wrap accepted server socket
int Network::wrap_server_connection(int fd) {
    if (!g_server_ctx) 
        return -1;

    SSL* ssl = SSL_new(g_server_ctx);
    if (!ssl) 
        return -1;

    SSL_set_fd(ssl, fd);
    if (SSL_accept(ssl) != 1) { 
        log_errors("SSL_accept"); 
        SSL_free(ssl); 
        return -1; 
    }
    std::lock_guard<std::mutex> lock(g_ssl_mutex);
    g_fd_ssl[fd] = ssl;

    return 0;
}

// Wrap connected client socket
int Network::wrap_client_connection(int fd) {
    if (!g_client_ctx) 
        return -1;
    
    SSL* ssl = SSL_new(g_client_ctx);
    
    if (!ssl) 
        return -1;
    
    SSL_set_fd(ssl, fd);
    
    if (SSL_connect(ssl) != 1) { 
        log_errors("SSL_connect"); 
        SSL_free(ssl); 
        return -1; 
    }
    
    // Only check certificate if verification is enabled
    if (SSL_CTX_get_verify_mode(g_client_ctx) != SSL_VERIFY_NONE) {
        X509* cert = SSL_get_peer_certificate(ssl);
        
        if (cert) {
            long vr = SSL_get_verify_result(ssl);
            if (vr != X509_V_OK) 
                std::cerr << "Peer certificate verify failed: " << X509_verify_cert_error_string(vr) << "\n";
            X509_free(cert);
        } else {
            std::cerr << "No peer certificate\n";
        }
    }
    
    std::lock_guard<std::mutex> lock(g_ssl_mutex);
    g_fd_ssl[fd] = ssl;
    return 0;
}

void Network::close_tls(int fd) {
    std::lock_guard<std::mutex> lock(g_ssl_mutex);
    auto it = g_fd_ssl.find(fd);
    if (it != g_fd_ssl.end()) {
        // Don't wait for bidirectional shutdown, just send close_notify
        SSL_set_shutdown(it->second, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
        SSL_shutdown(it->second);
        SSL_free(it->second);
        g_fd_ssl.erase(it);
    }
}

void Network::close_connection(int fd) {
    close_tls(fd);
    close(fd);
}

void Network::cleanup_tls() {
    if (g_server_ctx) {
        SSL_CTX_free(g_server_ctx);
        g_server_ctx = nullptr;
    }
    if (g_client_ctx) {
        SSL_CTX_free(g_client_ctx);
        g_client_ctx = nullptr;
    }
}

// TLS-aware raw send
int Network::send_raw(int fd, const void* data, size_t len) {
    if (is_tls(fd)) {
        std::lock_guard<std::mutex> lock(g_ssl_mutex);
        return ssl_write_all(g_fd_ssl[fd], (const char*)data, len) == (ssize_t)len ? 0 : -1;
    }
    size_t sent = 0;
    const char* p = (const char*)data;
    while (sent < len) {
        ssize_t r = send(fd, p + sent, len - sent, 0);
        if (r <= 0) return -1;
        sent += (size_t)r;
    }
    return 0;
}

// TLS-aware partial read
ssize_t Network::read_some(int fd, void* buf, size_t len) {
    if (is_tls(fd)) {
        std::lock_guard<std::mutex> lock(g_ssl_mutex);
        return SSL_read(g_fd_ssl[fd], buf, (int)len);
    }
    return recv(fd, buf, len, 0);
}

ssize_t Network::recv_all(int sockfd, char *buf, size_t len) {
    if (is_tls(sockfd)) {
        std::lock_guard<std::mutex> lock(g_ssl_mutex);
        return ssl_read_all(g_fd_ssl[sockfd], buf, len);
    }
    size_t total = 0; ssize_t n;
    while (total < len) {
        n = recv(sockfd, buf + total, len - total, 0);
        if (n <= 0) return n;
        total += (size_t)n;
    }
    return (ssize_t)total;
}

int Network::send_bytes(int client_fd, const void* data, size_t size, const std::string& debug_name) {
    if (size > UINT32_MAX) { 
        std::cerr << debug_name << " too large\n"; 
        return -1; 
    }
    uint32_t len_net = htonl((uint32_t)size);
    if (is_tls(client_fd)) {
        std::lock_guard<std::mutex> lock(g_ssl_mutex);
        if (ssl_write_all(g_fd_ssl[client_fd], (char*)&len_net, sizeof(len_net)) <= 0) 
            return -1;
        const char* p = (const char*)data;
        size_t sent = 0;
        while (sent < size) {
            ssize_t r = ssl_write_all(g_fd_ssl[client_fd], p + sent, size - sent);
            if (r <= 0) 
                return -1;
            sent += (size_t)r;
        }
        return 0;
    }
    // Non-TLS path
    if (send(client_fd, &len_net, sizeof(len_net), 0) == -1) { 
        perror(("send " + debug_name + " length failed").c_str()); 
        return -1; 
    }
    const char* p = (const char*)data; size_t sent = 0;
    while (sent < size) {
        ssize_t n = send(client_fd, p + sent, size - sent, 0);
        if (n <= 0) { 
            perror(("send " + debug_name + " data failed").c_str()); 
            return -1; 
        }
        sent += (size_t)n;
    }
    return 0;
}

int Network::recv_bytes(int client_fd, std::vector<char>& buffer, const std::string& debug_name) {
    uint32_t len_net = 0;
    if (is_tls(client_fd)) {
        std::lock_guard<std::mutex> lock(g_ssl_mutex);
        if (ssl_read_all(g_fd_ssl[client_fd], (char*)&len_net, sizeof(len_net)) <= 0) 
            return -1;
    } else {
        if (recv_all(client_fd, (char*)&len_net, sizeof(len_net)) <= 0) { 
            perror(("recv " + debug_name + " length failed").c_str()); 
            return -1; 
        }
    }
    uint32_t len = ntohl(len_net);
    const uint32_t MAX = 10u * 1024u * 1024u;
    if (len > MAX) { 
        std::cerr << debug_name << " too large: " << len << "\n"; 
        return -1; 
    }
    buffer.resize(len);
    if (len == 0) return 0;
    if (is_tls(client_fd)) {
        std::lock_guard<std::mutex> lock(g_ssl_mutex);
        if (ssl_read_all(g_fd_ssl[client_fd], buffer.data(), len) <= 0) 
        return -1;
    } else {
        if (recv_all(client_fd, buffer.data(), len) <= 0) { 
            perror(("recv " + debug_name + " data failed").c_str()); 
            return -1; 
        }
    }
    return 0;
}

int Network::send_string(int client_fd, const std::string& str, const std::string& debug_name) {
    return send_bytes(client_fd, str.data(), str.size(), debug_name);
}

int Network::recv_string(int client_fd, std::string& str, const std::string& debug_name) {
    std::vector<char> buffer;
    if (recv_bytes(client_fd, buffer, debug_name) != 0) 
        return -1;
    str.assign(buffer.begin(), buffer.end());
    return 0;
}

int Network::get_file(int) { return 0; }
int Network::send_file(int) { return 0; }