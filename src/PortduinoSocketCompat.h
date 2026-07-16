// BSD sockets to Winsock compatibility for the portduino WiFi shim.
//
// Winsock needs explicit initialization, closes with closesocket(), sets
// non-blocking via ioctlsocket(FIONBIO), cannot read()/write() a socket, reports
// errors through WSAGetLastError(), and has no SIGPIPE.
//
// The descriptor stays an `int`, as the WiFiClient/WiFiServer headers declare it.
// That is safe on Win64 even though SOCKET is a UINT_PTR: Windows documents
// socket handles as fitting in 32 bits, and INVALID_SOCKET narrows to -1, so the
// existing `< 0` checks keep working.
#ifndef PORTDUINO_SOCKET_COMPAT_H
#define PORTDUINO_SOCKET_COMPAT_H

#include <stddef.h>

#ifdef _WIN32

#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>

// The type the platform's socket calls take; the library casts at the boundary.
typedef SOCKET portduino_socket_t;

// Idempotent; call from every entry point that may open a socket. The process
// needs sockets for its whole life, so this is never unwound.
inline void portduinoSocketsInit()
{
    static std::once_flag flag;
    std::call_once(flag, [] {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    });
}

inline int portduino_socket_close(int fd) { return ::closesocket(static_cast<SOCKET>(fd)); }

inline int portduino_socket_set_nonblocking(int fd)
{
    u_long mode = 1;
    return ::ioctlsocket(static_cast<SOCKET>(fd), FIONBIO, &mode);
}

inline int portduino_socket_read(int fd, void *buf, size_t len)
{
    return ::recv(static_cast<SOCKET>(fd), static_cast<char *>(buf), static_cast<int>(len), 0);
}

inline int portduino_socket_write(int fd, const void *buf, size_t len)
{
    return ::send(static_cast<SOCKET>(fd), static_cast<const char *>(buf), static_cast<int>(len), 0);
}

inline int portduino_socket_errno() { return ::WSAGetLastError(); }

inline bool portduino_socket_would_block(int err) { return err == WSAEWOULDBLOCK; }

// accept() failures meaning "this client went away" rather than "the listening
// socket is broken". Mirrors the errno set the POSIX branch tests.
inline bool portduino_socket_accept_transient(int err)
{
    return err == WSAEWOULDBLOCK || err == WSAECONNRESET || err == WSAENETDOWN || err == WSAENETUNREACH ||
           err == WSAEHOSTUNREACH || err == WSAEOPNOTSUPP || err == WSAECONNABORTED;
}

#else // !_WIN32

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

typedef int portduino_socket_t;

inline void portduinoSocketsInit() {}

inline int portduino_socket_close(int fd) { return ::close(fd); }

inline int portduino_socket_set_nonblocking(int fd)
{
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return flags;
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

inline int portduino_socket_read(int fd, void *buf, size_t len) { return static_cast<int>(::read(fd, buf, len)); }

inline int portduino_socket_write(int fd, const void *buf, size_t len) { return static_cast<int>(::write(fd, buf, len)); }

inline int portduino_socket_errno() { return errno; }

inline bool portduino_socket_would_block(int err) { return err == EAGAIN || err == EWOULDBLOCK; }

inline bool portduino_socket_accept_transient(int err)
{
    return err == ENETDOWN || err == EPROTO || err == ENOPROTOOPT || err == EHOSTDOWN || err == EHOSTUNREACH ||
           err == EOPNOTSUPP || err == ENETUNREACH || err == EWOULDBLOCK || err == EAGAIN;
}

#endif // _WIN32

#endif // PORTDUINO_SOCKET_COMPAT_H
