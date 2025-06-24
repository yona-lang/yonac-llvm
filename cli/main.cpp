// yonac - Yona Compiler
// This will be the future LLVM-based compiler for Yona

#include <iostream>
#include <boost/program_options.hpp>
#include <boost/log/trivial.hpp>
#include "version.h"

using namespace std;

int main(int argc, char* argv[]) {
    namespace po = boost::program_options;
    
    po::options_description desc("Yona Compiler Options");
    desc.add_options()
        ("help,h", "Show help message")
        ("version,v", "Show version information")
        ("output,o", po::value<string>(), "Output file")
        ("optimize,O", po::value<int>()->default_value(0), "Optimization level (0-3)")
        ("emit-llvm", "Emit LLVM IR instead of object code")
        ("emit-asm,S", "Emit assembly instead of object code")
        ;
    
    po::positional_options_description p;
    p.add("input-file", -1);
    
    po::variables_map vm;
    try {
        po::store(po::command_line_parser(argc, argv).options(desc).positional(p).run(), vm);
        po::notify(vm);
    } catch (const po::error& e) {
        BOOST_LOG_TRIVIAL(error) << "Error: " << e.what();
        return 1;
    }
    
    if (vm.count("help")) {
        cout << "Yona Compiler (yonac)" << endl;
        cout << desc << endl;
        return 0;
    }
    
    if (vm.count("version")) {
        cout << "yonac version " << YONA_VERSION_STRING << endl;
        cout << "LLVM support included" << endl;
        return 0;
    }
    
    BOOST_LOG_TRIVIAL(error) << "Yona compiler is not yet implemented. This is a placeholder.";
    return 1;
}