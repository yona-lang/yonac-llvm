// yonac-llvm.cpp : Defines the entry point for the application.
//

#include <iostream>
#include <antlr4-runtime.h>
#include <boost/program_options.hpp>

#include "YonaLexer.h"
#include "YonaParser.h"
#include "main.h"

namespace po = boost::program_options;
using namespace antlr4;
using namespace yonac;
using namespace std;

void process_program_options(const int argc, const char* const argv[]) {
    po::options_description desc("Allowed options");
    desc.add_options()("help", "Show brief usage message");

    po::variables_map args;

    try {
        po::store(
            po::parse_command_line(argc, argv, desc),
            args);
    }
    catch (po::error const& e) {
        cerr << e.what() << '\n';
        exit(EXIT_FAILURE);
    }
    po::notify(args);
    return;
}

int main(const int argc, const char* argv[]) {
    process_program_options(argc, argv);

    ifstream stream;
    string fname;
#ifndef NDEBUG
    fname = argc > 1 ? argv[1] : "test.yona";
#else
    fname = argv[1];
#endif
    stream.open(fname);

    ANTLRInputStream input(stream);
    YonaLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    YonaParser parser(&tokens);

    lexer.addErrorListener(&ConsoleErrorListener::INSTANCE);
    parser.addErrorListener(&ConsoleErrorListener::INSTANCE);

    YonaParser::InputContext* tree = parser.input();

    stream.close();
    return 0;
}
