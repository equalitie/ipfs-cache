#include <boost/program_options.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/beast.hpp>
#include <ipfs_cache/injector.h>
#include <ipfs_cache/client.h>
#include <iostream>

using namespace std;

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace sys   = boost::system;

using string_view = beast::string_view;
using tcp = asio::ip::tcp;

// -------------------------------------------------------------------
// TODO: Seems Boost.Beast doesn't support url query decoding?
namespace _parse_detail {
    static string_view make_view(const char* b, const char* e)
    {
        return string_view(b, e - b);
    }

    static
    const char* find(char c, const char* b, const char* e)
    {
        for (auto i = b; i != e; ++i) if (*i == c) return i;
        return e;
    }

    static
    pair<string_view, string_view>
    parse_keyval(const char* b, const char* e)
    {
        auto p = find('=', b, e);
        auto v = (p == e) ? make_view(e, e) : make_view(p + 1, e);
        return make_pair(make_view(b, p), v);
    }
}

static
map<string_view, string_view> parse_vars(const string& vars)
{
    using namespace _parse_detail;

    map<string_view, string_view> ret;

    const char* b = vars.c_str();
    const char* e = vars.c_str() + vars.size();

    for (;;) {
        auto p = find('&', b, e);
        auto kv = parse_keyval(b, p);
        ret.insert(kv);
        if (p == e) break;
        b = p + 1;
    }

    return ret;
}

// -------------------------------------------------------------------
// Report a failure
void fail(sys::error_code ec, char const* what)
{
    cerr << what << ": " << ec.message() << "\n";
}

// Handles an HTTP server connection
class session : public enable_shared_from_this<session>
{
public:
    // Take ownership of the socket
    explicit session( tcp::socket socket
                    , ipfs_cache::Injector& injector)
        : _socket(move(socket))
        , _injector(injector)
    { }

    // Start the asynchronous operation
    void run()
    {
        do_read();
    }

    void do_read()
    {
        // Read a request
        http::async_read(_socket, _buffer, _req,
            [this, s = shared_from_this()]
            (sys::error_code ec, size_t) {
                on_read(ec);
            });
    }

    void on_read(boost::system::error_code ec)
    {
        // This means they closed the connection
        if (ec == http::error::end_of_stream) return do_close();
        if (ec) return fail(ec, "read");

        auto vars = parse_vars(_req.body());

        auto key   = vars["key"];
        auto value = vars["value"];

        if (key.empty() || value.empty()) {
            http::response<http::string_body> res
                { http::status::bad_request
                , _req.version()};

            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/plain");
            res.body() = "FAIL";
            res.prepare_payload();

            return send(move(res));
        }

        _injector.insert_content( key.to_string()
                                , value.to_string(),
                                 [=, s = shared_from_this()]
                                 (std::string ipfs_id) {
            http::response<http::string_body> res{http::status::ok, _req.version()};
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, "text/plain");
            res.body() = "OK";
            res.keep_alive(_req.keep_alive());
            res.prepare_payload();

            send(move(res));
        });

    }

    template<class Res> void send(Res res)
    {
        auto sp = make_shared<Res>(move(res));

        http::async_write(
            _socket,
            *sp,
            [this, s = shared_from_this(), sp]
            (sys::error_code ec, size_t) {
                if (ec == http::error::end_of_stream) return do_close();
                if (ec) return fail(ec, "write");

                do_read();
            });
    }

    void do_close()
    {
        // Send a TCP shutdown
        boost::system::error_code ec;
        _socket.shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }

private:
    tcp::socket _socket;
    boost::beast::flat_buffer _buffer;
    http::request<http::string_body> _req;
    ipfs_cache::Injector& _injector;
};

// Accepts incoming connections and launches the sessions
class listener : public enable_shared_from_this<listener> {
public:
    listener( boost::asio::io_service& ios
            , uint16_t port
            , ipfs_cache::Injector& injector)
        : _acceptor(ios)
        , _socket(ios)
        , _injector(injector)
    {
        sys::error_code ec;

        auto address = asio::ip::address::from_string("0.0.0.0");
        tcp::endpoint endpoint{address, port};

        // Open the acceptor
        _acceptor.open(endpoint.protocol(), ec);
        if (ec) {
            fail(ec, "open");
            return;
        }

        // Bind to the server address
        _acceptor.bind(endpoint, ec);
        if (ec) {
            fail(ec, "bind");
            return;
        }

        cout << "Listening on port " << _acceptor.local_endpoint().port() << endl;

        // Start listening for connections
        _acceptor.listen(asio::socket_base::max_connections, ec);
        if (ec) {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void run()
    {
        if (!_acceptor.is_open()) return;
        do_accept();
    }

    void do_accept()
    {
        _acceptor.async_accept(
            _socket,
            [this, self = shared_from_this()] (sys::error_code ec) {
                if (ec) return fail(ec, "accept");

                // Create the session and run it
                make_shared<session>(move(_socket), _injector)->run();

                // Accept another connection
                do_accept();
            });
    }

private:
    tcp::acceptor _acceptor;
    tcp::socket _socket;
    ipfs_cache::Injector& _injector;
};

int main(int argc, const char** argv)
{
    /*
     * Parse command line options.
     */
    namespace po = boost::program_options;

    po::options_description desc("Options");

    desc.add_options()
        ("help", "Produce this help message")
        ("repo", po::value<std::string>()->default_value("./repo"),
         "Path to the IPFS repository")
        ("port,p", po::value<uint16_t>()->default_value(0),
         "Port the server will listen on (use 0 for random)")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help")) {
        cout << desc << endl;
        return 0;
    }

    string repo   = vm["repo"].as<string>();
    uint16_t port = vm["port"].as<uint16_t>();

    asio::io_service ios;

    /*
     * Create the injector and the server and start the main loop.
     */
    try {
        ipfs_cache::Injector injector(ios, repo);
        make_shared<listener>(ios, port, injector)->run();

        cout << "IPNS of this database is " << injector.ipns_id() << endl;
        cout << "Starting event loop, press Ctrl-C to exit." << endl;

        ios.run();
    }
    catch (const exception& e) {
        cerr << "Exception " << e.what() << endl;
    }

    return 0;
}
