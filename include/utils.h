//
// Created by akovari on 15.12.24.
//

#pragma once

#include "ast.h"
#include "common.h"

#define PACKAGE_DELIMITER "\\"

namespace yona
{
    using namespace std;
    using namespace ast;
    using namespace compiler::types;
    using namespace antlr4;

    const vector<pair<string, string>> YONA_CTRL_CHARS_UNESCAPE = { { "\\b", "\b" }, { "\\n", "\n" }, { "\\t", "\t" },
                                                                    { "\\f", "\f" }, { "\\r", "\r" }, { "\\0", "\0" } };

    class CharSequenceTranslator
    {
    public:
        CharSequenceTranslator() = default;
        virtual ~CharSequenceTranslator() = default;
        virtual string translate(const string& input) = 0;
    };

    class LookupTranslator final : public CharSequenceTranslator
    {
    private:
        unordered_map<string, string> lookupMap;

    public:
        explicit LookupTranslator(const vector<pair<string, string>>& lookup) :
            lookupMap(lookup.begin(), lookup.end()) {};

        string translate(const string& input) override;
    };

    class AggregateTranslator final : public CharSequenceTranslator
    {
    private:
        vector<shared_ptr<CharSequenceTranslator>> translators;

    public:
        AggregateTranslator(const initializer_list<shared_ptr<CharSequenceTranslator>> translators) :
            translators(translators)
        {
        }

        string translate(const string& input) override;
    };

    auto UNESCAPE_YONA = AggregateTranslator{
        // new OctalUnescaper(), // .between('\1', '\377')
        // new UnicodeUnescaper(),
        make_shared<LookupTranslator>(YONA_CTRL_CHARS_UNESCAPE),
        // make_shared<LookupTranslator>({ { "\\\"", "\"" },
        //                                 { "\\\\", "\\" },
        //                                 { "\\'", "'" },
        //                                 { "{{", "{" },
        //                                 { "}}", "}" },
        //                                 { "\\a" /*Bell (alert)*/, string(1, (char)7) },
        //                                 { "\\v" /*Vertical tab*/, string(1, (char)9) } }
        //                                 )
    };

    string unescapeYonaString(const string& rawString);
}
