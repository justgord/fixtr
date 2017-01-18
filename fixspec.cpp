//
//  fixspec.cpp - trace the fix fields for a given msg type
//
//      USAGE fixspec <msgtype>
//
//
#include <stdlib.h>
#include <cstring>
#include <cassert>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>

#include <libxml/parser.h>
#include "fixcore.h"


int main(int argc, char *argv[]) 
{
    // handle args

    string spec_path = fix_spec_path("44");

    if (argc<2)
    {
        fprintf(stderr, "USAGE: fixspec <msgtype>\n");
        fprintf(stderr, "  option -E                    : show field allowed enum values\n");
        fprintf(stderr, "  option -S=<FIX_VERSION>      : use spec file FIXspec.xml\n");
        exit(-1);
    }

    const char* sztype = argv[1];

    mapss options;
    for (int i=2;i<argc;i++)
    {
        const char* szopt=argv[i];

        if (0==strcmp(szopt,"-E"))
            options["enums"]="Y";

        if (0==strncmp(szopt, "-S", 2))
        {
            if (strlen(szopt)>=3) {
                spec_path = fix_spec_path(szopt+(szopt[2]=='=')+2);
            }
            else if (i < argc-1) {
                spec_path = fix_spec_path(argv[++i]);
            }
            else
            {
                fprintf(stderr,"Bad option -S\n"), exit(-1);
                fprintf(stderr,"USAGE: fixspec {-S=<FIX_VERSION>}\n");
                exit(-1);
            }
        }
    }

    const char *szspecfile = spec_path.c_str();
    fprintf(stderr,"Using spec : %s\n", szspecfile);

    // parse the spec, expand the components, show the spec for the message type

    XNode* ndfix = parse_fix_spec_xml(szspecfile); 
    if (!ndfix)
        exit(-1);

    MessageGenerator fixgen(ndfix);
    fixgen.show_expanded_spec(sztype, options);

    // cleanup

    delete ndfix;
}

