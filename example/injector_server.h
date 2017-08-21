#pragma once

#include <sstream>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>

#include <event2/keyvalq_struct.h>

#include <string.h>
#include <assert.h>

class InjectorServer {
public:
    InjectorServer( event_base* evbase
                  , uint16_t port
                  , ipfs_cache::Injector& injector);

    InjectorServer(const InjectorServer&) = delete;
    InjectorServer& operator=(const InjectorServer&) = delete;

    uint16_t listening_port() const;

    ~InjectorServer();

private:
    static void handle(struct evhttp_request *req, void *arg);

private:
    event_base* _evbase;
    ipfs_cache::Injector& _injector;
    struct evhttp* _http;
	struct evhttp_bound_socket *_socket;
};

inline
InjectorServer::InjectorServer( event_base* evbase
                              , uint16_t port
                              , ipfs_cache::Injector& injector)
    : _evbase(evbase)
    , _injector(injector)
    , _http(evhttp_new(evbase))
{
    using namespace std;

	if (!_http) {
		throw runtime_error("Couldn't create http server");
	}

	evhttp_set_gencb(_http, InjectorServer::handle, this);

	/* Now we tell the evhttp what port to listen on (0 means random) */
	_socket = evhttp_bind_socket_with_handle(_http, "0.0.0.0", port);

	if (!_socket) {
        stringstream ss;
        ss << "Couldn't bind to port " << port;
        throw runtime_error(ss.str());
	}
}

inline
void
InjectorServer::handle(struct evhttp_request *req, void *arg)
{
    auto self = reinterpret_cast<InjectorServer*>(arg);

    struct evbuffer *in_evb = evhttp_request_get_input_buffer(req);
    size_t len = evbuffer_get_length(in_evb);

    struct evkeyvalq params;
    std::string data(len, 'X');
    evbuffer_copyout(in_evb, (void*) data.data(), data.size());

    // TODO(peterj) How do I destroy the evkeyvalq structure?
    evhttp_parse_query_str(data.data(), &params);

    const char *key = evhttp_find_header(&params, "key");
    const char *value = evhttp_find_header(&params, "value");

    if (!key || !value) {
	    evhttp_send_error(req, 501, "'key' and 'value' parameters must be set");
        return;
    }

    self->_injector.insert_content(key, value, [=](std::string ipfs_id) {
	    auto evb = evbuffer_new();

		evbuffer_add_printf(evb,
                    "<!DOCTYPE html>\n"
                    "<html>\n"
                    " <head><meta charset='utf-8'></head>\n"
		            " <body>%s</body>\n"
                    "</html>\n", ipfs_id.c_str());

	    evhttp_send_reply(req, 200, "OK", evb);

		evbuffer_free(evb);
    });
}

inline
uint16_t
InjectorServer::listening_port() const
{
    // Code taken from libevent/sample/http-server.c

    /* Extract and display the address we're listening on. */
    struct sockaddr_storage ss;
    evutil_socket_t fd;
    ev_socklen_t socklen = sizeof(ss);
    int got_port = -1;
    fd = evhttp_bound_socket_get_fd(_socket);
    memset(&ss, 0, sizeof(ss));
    if (getsockname(fd, (struct sockaddr *)&ss, &socklen)) {
    	perror("getsockname() failed");
        assert(0);
    }
    if (ss.ss_family == AF_INET) {
    	got_port = ntohs(((struct sockaddr_in*)&ss)->sin_port);
    } else if (ss.ss_family == AF_INET6) {
    	got_port = ntohs(((struct sockaddr_in6*)&ss)->sin6_port);
    } else {
        assert(0);
    }

    return got_port;
}

inline
InjectorServer::~InjectorServer()
{
    evhttp_del_accept_socket(_http, _socket);
    evhttp_free(_http);
}
