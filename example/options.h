#pragma once

#include <boost/program_options.hpp>

class Options {
public:
    Options();

    void parse(int argc, const char** argv);

    bool help()         const { return _help;   }
    bool inject()       const { return _inject; }
    bool fetch()        const { return _fetch;  }

    const std::string& key()   const { return _key;    }
    const std::string& value() const { return _value;  }
    const std::string& repo()  const { return _repo;   }
    const std::string& ipns()  const { return _ipns;   }

private:
    friend std::ostream& operator<<(std::ostream&, const Options&);

    boost::program_options::options_description _desc;

    std::string _executable;
    bool _help = false;
    bool _inject = false;
    bool _fetch = false;
    std::string _key;
    std::string _value;
    std::string _ipns;
    std::string _repo = "./repo";
};

inline
Options::Options()
    : _desc("Options")
{
    namespace po = boost::program_options;

    _desc.add_options()
        ("help", "Produce this help message")
        ("repo", po::value<std::string>()->default_value(_repo),
         "Path to the IPFS repository")
        ("inject", "Indicate that we'll be injecting")
        ("fetch", "Indicate that we'll be fetching a value")
        ("ipns", po::value<std::string>(), "IPNS of the datase")
        ("key", po::value<std::string>(), "The key to inject or fetch")
        ("value", po::value<std::string>(), "The value to inject")
        ;
}

inline
void Options::parse(int argc, const char** argv)
{
    namespace po = boost::program_options;
    using namespace std;

    if (argc) _executable = argv[0];

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, _desc), vm);
    po::notify(vm); 

    if (vm.count("help"))   { _help   = true; }
    if (vm.count("repo"))   { _repo   = vm["repo"].as<string>(); }
    if (vm.count("fetch"))  { _fetch  = true; }
    if (vm.count("inject")) { _inject = true; }
    if (vm.count("key"))    { _key    = vm["key"].as<string>(); }
    if (vm.count("value"))  { _value  = vm["value"].as<string>(); }
    if (vm.count("ipns"))   { _ipns   = vm["ipns"].as<string>(); }

    if ((_help?1:0) + (_inject?1:0) + (_fetch?1:0) != 1) {
        throw std::runtime_error("Exactly one argument from {help,inject,fetch} is needed");
    }

    if (_inject && (!_key.size() || !_value.size())) {
        throw std::runtime_error("The --inject action needs --key and --value arguments set");
    }

    if (_fetch && !_key.size()) {
        throw std::runtime_error("The --fetch action needs the --key argument set");
    }

    if (_fetch && !_ipns.size()) {
        throw std::runtime_error("The --fetch action needs the --ipns argument set");
    }
}

inline
std::ostream& operator<<(std::ostream& os, const Options& o) {
    auto examples = "Examples:\n"
                    "  " + o._executable + " --inject --key foo --value bar\n"
                  + "  " + o._executable + " --fetch --key foo --ipns <DB-IPNS>\n"
                  + "\n";

    return os << examples << o._desc;
}

