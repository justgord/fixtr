//
//  fixtr.cpp - FIX message tracer
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
#include <unistd.h>

#include <libxml/parser.h>
#include "fixcore.h"


///


string fix_sample_sell(MessageGenerator& fixgen)
{
    mapss atts;
    atts["ClOrdID"]         = "CLIENT_MACHINE";
    //Instrument component
        atts["Symbol"]      = "GOOG";
        atts["SecurityId"]  = "GOOG.SKG";
    atts["Side"]            = "2";                      // 2 == SELL
    atts["TransactTime"]    = fix_time_now();
    //OrderQtyData component
        atts["OrderQty"]    = "100";
    atts["OrdType"]         = "1";                      // 1 == MARKET                  // 40 NewOrderSingle.OrdType (reqd)

    string smsg;
    if (fixgen.gen_msg("D", atts, "GORD_CLIENT", "GORD_SERVER", smsg))
    {
        printf("ERROR generating message");
        return "";
    }

    trace_raw_fix(smsg.c_str(), "FIX MSG : ");

    return smsg;
}
    
void test_gen_sell(MessageGenerator& MG)
{
    string sfix = fix_sample_sell(MG);

    FixReader fix(sfix.c_str(), sfix.length());


    XNode* xheader  = MG.load_expanded(MG.ndheader);
    XNode* xmsgs    = MG.load_expanded(MG.ndmsgs);
    XNode* xtrailer = MG.load_expanded(MG.ndtrailer);

    xheader->atts["name"]="StandardHeader";
    xtrailer->atts["name"]="StandardTrailer";

    //

    printf("header\n");
    MG.trace_fix_xspec(fix, xheader);

    XNode* xbody = xmsgs->lookup(fix.msgtype.c_str());
    printf("%s\n", fix.msgtype.c_str());
    MG.trace_fix_xspec(fix, xbody);

    printf("trailer\n");
    MG.trace_fix_xspec(fix, xtrailer);
}

void test_spec_next_fld(MessageGenerator& fixgen, string stype)
{
    // run through the spec fields for a given message type stype [ eg stype=="D" ]

    // shows stack expanded with components etc

    printf("test_spec_next_fld\n");

    XNode* spec = fixgen.messages[stype];
    assert(spec);

    MsgContext ctx(NULL,0, fixgen.components, fixgen.fields);  

    ctx.stack_push(spec);

    XNode* field;
    while(NULL!=(field = ctx.next_fld()))
    {
        ctx.trace_stack();

        field->trace();
    }
}


int trace_expanded(MessageGenerator& MG)
{
    // for each of - header, trailer, and each msg type
    //      run through the nested components and expend into the full list of fields [to support misordering of fields]

    XNode* xheader  = MG.load_expanded(MG.ndheader);
    XNode* xtrailer = MG.load_expanded(MG.ndtrailer);
    XNode* xmsgs    = MG.load_expanded(MG.ndmsgs);

    xheader->atts["name"]="StandardHeader";
    xtrailer->atts["name"]="StandardTrailer";

    // validate

    if (!true)
    {

        xheader->trace(">>> HEADER");
        xheader->trace_nodmap();
    }

    assert(xheader->lookup("8"));
    assert(xheader->lookup("9"));

    assert(xtrailer->lookup("10"));

    XNode* xmsg = xmsgs->lookup("D");
    assert(xmsg);
    assert(xmsg->ismessage());

    XNode* xsym = xmsg->lookup("55");     // "Symbol");
    assert(xsym);
    
    XNode* xqty = xmsg->lookup("38");     // "OrderQty");
    assert(xqty);
    
    XNode* xtyp = xmsg->lookup("40");     // "OrdTyp");
    assert(xtyp);

    if (!true)
    {
        XNode* xnoh = xmsg->lookup("454");     // 454 NoSecurityAltID group 
        assert(xnoh);
        assert(xnoh->isgroup());


        xmsg->trace(">>> D msg");
        //xmsg->trace_nodmap();
        xsym->trace(">>> Symbol");
        xqty->trace(">>> OrderQty");
        xtyp->trace(">>> OrdTyp");
        xnoh->trace(">>> 454 NoSecurityAltID group");
    }

    //xmsg->xtrace();


    if (!true)
    {
        // human readable tracing of sample fix message

        string sfix = fix_sample_sell(MG);

        FixReader fix(sfix.c_str(), sfix.length());

        MG.trace_fix_xspec(fix, xheader);
        MG.trace_fix_xspec(fix, xmsg);
        MG.trace_fix_xspec(fix, xtrailer);
    }

    if (true)
    {
        // read lines from stdin and trace any fix messages we recognize embedded in the text input

        string sline;
        int npos=0;
        while(!cin.eof()) 
        {
            getline(cin, sline); 
            
            const char* p = sline.c_str();
            while(NULL!=(p = strstr(p, "8=FIX")))
            {
                int len = strlen(p);

                if (MG.msg_bad(p, len))
                {
                    p+=5;
                    continue;
                }

                trace_raw_fix(p, "\nMSG = ");

                FixReader fix(p, len);

                printf("\nheader\n");
                MG.trace_fix_xspec(fix, xheader);

                if (!fix.msgtype.empty())
                {
                    XNode* xbody = xmsgs->lookup(fix.msgtype.c_str());

                    printf("\n%s\n", xbody->att("name"));
                    MG.trace_fix_xspec(fix, xbody);

                    printf("\ntrailer\n");
                    MG.trace_fix_xspec(fix, xtrailer);
                }

                npos = fix.npos;

                if (npos>0)
                    p+=npos;
                else
                    p+=5;
            } 
        }
    }
    // cleanup

    delete xheader;
    delete xtrailer;
    delete xmsgs;

    return 0;
}

///

int main(int argc, char *argv[]) 
{
    // handle args

    string spec_path = fix_spec_path("44");

    if (argc==2 || argc==3)
    {
        const char* szopt=argv[1];

        if (0==strncmp(szopt, "-S", 2))
        {
            if (strlen(szopt)>3)
            {
                spec_path = fix_spec_path(szopt+(szopt[2]=='=')+2);
            }
            else if (argc == 3) {
                spec_path = fix_spec_path(argv[2]);
            }
            else
            {
                fprintf(stderr,"Bad option -S\n"), exit(-1);
                fprintf(stderr,"USAGE: fixtr {-S=<FIX_VERSION>} < fix_messages.fix\n");
                exit(-1);
            }
        }
    }
    else if (argc>2)
    {
        fprintf(stderr,"USAGE: fixtr {-S=./spec/FIXnn.xml} < fix_messages.fix\n");
        exit(-1);
    }

    const char *szfile = spec_path.c_str();

    if (access(szfile, R_OK))
    {
        fprintf(stderr,"Cant read file [%s]\n", szfile);
        exit(-1);
    }

    // parse the spec

    XNode* ndfix = parse_fix_spec_xml(szfile); 
    if (!ndfix)
        exit(-1);

    MessageGenerator fixgen(ndfix);

    // dev tests

    if (!true)
        test_spec_next_fld(fixgen, "D"); 

    if (!true)
        test_gen_sell(fixgen); 


    // expand the spec [replacing components inline], and use spec to summarize inbound fix messages as we see them

    trace_expanded(fixgen);

}

