#include <boost/program_options.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/beast.hpp>
#include <ipfs_cache/injector.h>
#include <ipfs_cache/client.h>
#include <iostream>

#include "parse_vars.h"

using namespace std;

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace sys   = boost::system;

using string_view = beast::string_view;
using tcp = asio::ip::tcp;

// -------------------------------------------------------------------
// Report a failure
void fail(sys::error_code ec, char const* what)
{
    cerr << what << ": " << ec.message() << "\n";
}

// Handles an HTTP server connection
void serve( tcp::socket socket
          , ipfs_cache::Injector& injector
          , asio::yield_context yield)
{
    sys::error_code ec;

    boost::beast::flat_buffer buffer;
    http::request<http::string_body> req;

    // Read a request
    http::async_read(socket, buffer, req, yield[ec]);
    if (ec) return fail(ec, "http::async_read");

    auto vars = parse_vars(req.body());

    auto key   = vars["key"]  .to_string();
    auto value = vars["value"].to_string();

    if (key.empty() || value.empty()) {
        http::response<http::string_body> res
            { http::status::bad_request
            , req.version()};

        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/plain");
        res.body() = "FAIL";
        res.prepare_payload();

        http::async_write(socket, res, yield[ec]);
        return;
    }

    string ipfs_id = injector.insert_content(key, value, yield[ec]);
    if (ec) return fail(ec, "insert_content");

    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/plain");
    res.body() = "OK";
    res.keep_alive(req.keep_alive());
    res.prepare_payload();

    http::async_write(socket, res, yield[ec]);
    if (ec) fail(ec, "http::async_write");
}

// Accepts incoming connections and launches the sessions
static
void accept( asio::io_service& ios
           , uint16_t port
           , ipfs_cache::Injector& injector
           , asio::yield_context yield)
{
    tcp::acceptor acceptor{ios};

    sys::error_code ec;

    auto address = asio::ip::address::from_string("0.0.0.0");
    tcp::endpoint endpoint{address, port};

    // Open the acceptor
    acceptor.open(endpoint.protocol(), ec);
    if (ec) return fail(ec, "open");

    // Bind to the server address
    acceptor.bind(endpoint, ec);
    if (ec) return fail(ec, "bind");

    cout << "Serving on " << acceptor.local_endpoint() << endl;

    // Start listening for connections
    acceptor.listen(asio::socket_base::max_connections, ec);
    if (ec) return fail(ec, "listen");

    tcp::socket socket{ios};

    for (;;) {
        acceptor.async_accept(socket, yield[ec]);
        if (ec) return fail(ec, "accept");

        asio::spawn( yield
                   , [s = move(socket), &injector]
                     (auto yield) mutable {
                         serve(move(s), injector, yield);
                     });
    }
}

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

        cout << "IPNS of this database is " << injector.ipns_id() << endl;
        cout << "Starting event loop, press Ctrl-C to exit." << endl;

        asio::spawn( ios
                   , [&](asio::yield_context yield) {
                         accept(ios, port, injector, yield);
                     });

        ios.run();
    }
    catch (const exception& e) {
        cerr << "Exception " << e.what() << endl;
    }

    return 0;
}
