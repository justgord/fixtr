//
//  fixtr.cpp - FIX message tracer / logger / filter / validator / generator
//
//      use SAX2 to read into own Node Dom [read in all attribs and nesting]
//      load all attributes and children into generic nodes
//
//      trace & validate FIX messages based on metadata from FIXn.n.xml spec read in
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


string fix_time_now()
{
    char buff[64];

    time_t t = time(NULL);
    struct tm now;
    localtime_r(&t, &now);
    
    strftime(buff, 64, "%Y%m%d-%H:%M:%S", &now);

    return string(buff);
}

string fix_checksum(const char* sz, int len)
{
    int idx;
    unsigned int cks;
    
    for(idx=0, cks=0; idx<len; cks+=(unsigned int)sz[idx++]);
    
    char buf[4];
    sprintf(buf, "%03d", (unsigned int)(cks%256));

    return string(buf);
}

string int_to_string(int n)
{
    char buff[12];
    snprintf(buff, 12, "%d", n);
    return string(buff);
}

void trace_raw_fix(const char* sz, const char* msg="")
{
    const int BUFLEN=4096;

    char buf[BUFLEN];
    strncpy(buf, sz, BUFLEN);
    buf[BUFLEN-1]=0;
    
    int n = strlen(buf);
    for(int i=0;i<n;i++)
        if (buf[i]==0x01)
            buf[i]='|';

    fprintf(stderr, "%s%s\n", msg, buf);
}

void print_node_xml(XNode* N, int nindent)
{
    string sindent(nindent*2, ' ');

    printf("%s<%s", sindent.c_str(), N->elt.c_str());
    N->trace_atts();
    if (N->nods.size())
        printf(">\n");
    else
    {
        printf("/>\n");
        return;
    }

    for(vecx::iterator p=N->nods.begin();p!=N->nods.end();p++)
    {
        print_node_xml(*p, nindent+1);
    }

    printf("%s</%s>\n", sindent.c_str(), N->elt.c_str());
}

///////////////////


struct FieldIdWriter : XNodeVisitor
{
    mapss& fields;

    FieldIdWriter(mapss& fields_by_name)
        : fields(fields_by_name)
    {
    }

    virtual int operator()(XNode* p)
    {
        //printf("visiting elt [%s]\n", p->elt.c_str());    

        if (p->elt.compare("field")==0)
        {
            string field_name = p->atts["name"]; 

            string field_id = fields[field_name];

            p->atts["id"] = field_id; 

            //printf("FieldIdWriter setting id=%s for name %s\n", field_id.c_str(), field_name.c_str());
        }

        return 0;
    }
};


XNode* MsgContext::next_fld()
{
    // step to next spec field [recurse into spec of component or group as needed]    

    int& i = istack.back();
    XNode* spec = xstack.back();

    assert(istack.size()==xstack.size());

    if(i>=(int)spec->nods.size())
    {
        stack_pop();

        if (xstack.size())
            return next_fld();          // continue from previous stack

        return NULL;              
    }

    XNode* field = spec->nods[i++];

    // if its a component, push and recurse

    string field_type = field->elt;

    if (0==field_type.compare("component"))
    {
        string scomponent = field->atts["name"];
        XNode* comp = components[scomponent];

        stack_push(comp);

        return next_fld();                  // eat the component header, return first child instead
    }

    return field;
}

int MsgContext::next_fix()
{
    //printf("MsgContext::next_fix  npos=%d nlen=%d\n", npos, nlen);

    // parse next FIX attribute <fld>=<val>^ [starting from npos'th position in sz]

    fld.clear();
    val.clear();

    if (npos+2 >= len)
        return 0;

    const char* pbeg = sz+npos;
    const char* peqs = strchr(pbeg, '=');
    const char* pval = peqs+1; 
    const char* psoh = strchr(pval, (char)0x01); 
    const char* pnxt = psoh+1;
    const char* pmax = sz+nlen;

    if (pbeg>=pmax)
        return printf("ERROR : MsgContext::next pbeg>=pmax \n"), -1;
    if (pval>=pmax)
        return printf("ERROR : MsgContext::next pval>=pmax \n"), -1;

    if (pbeg>=peqs)
        return printf("ERROR : MsgContext::next pbeg>=peqs \n"), -1;
    if (pval>=psoh)
        return printf("ERROR : MsgContext::next pval>=psoh \n"), -1;

    string fldid(pbeg, peqs);

    fld.assign(pbeg, peqs);
    val.assign(pval, psoh);

    if (0==fld.compare("35"))           // special case : 35 MsgType - remember message type from header
        msgtype = val;

    int nchunk = pnxt-pbeg;
    npos += nchunk;
    return nchunk;
}

void MsgContext::rewind_fix()
{
    // go back to previous chunk in FIX msg [so calling next_fix() will restore current state]

    assert(fld.length()>0 && val.length()>0);               // rewind is one-shot
    if (fld.length()==0)
        return;

    int nchunk = fld.length() + 1 + val.length() + 1;       // rewind "<fld>=<val>|"

    fld.clear();
    val.clear();

    npos -= nchunk;
}

void MsgContext::trace_field()
{
    XNode* spec = xstack.back();
    assert(spec);
    int ispec = istack.back()-1;

    XNode* field = spec->nods[ispec];
    assert(field);

    string field_name = field->atts["name"];

    fprintf(stderr, "%3s %-15s : %s\n", fld.c_str(), field_name.c_str(), val.c_str());
}

// SpecParser


void spec_parser_start_element(void * ctx, const xmlChar *name, const xmlChar **atts)
{
    vecx* pstack = (vecx*)ctx;

    XNode* parent= pstack->back();
    XNode* pnode = new XNode((const char*)name, (const char**)atts, parent);

    pstack->push_back(pnode);
}

void spec_parser_end_element(void * ctx, const xmlChar *name)
{
    vecx* pstack = (vecx*)ctx;

    pstack->pop_back(); 
}

XNode* parse_fix_spec_xml(const char* szfile)
{
    xmlSAXHandlerPtr handler = (xmlSAXHandlerPtr)calloc(1, sizeof(xmlSAXHandler));
    handler->startElement   = spec_parser_start_element;
    handler->endElement     = spec_parser_end_element;

    vecx    stack;
    XNode*  root = new XNode();
    stack.push_back(root);

    if (xmlSAXUserParseFile(handler, &stack, szfile) < 0) 
    {
        fprintf(stderr, "ERROR parsing fix spec : %s\n", szfile);
        delete root;
        free(handler);
        return NULL;
    }

    if (0==root->nods.size())
    {
        fprintf(stderr, "ERROR empty fix spec : %s\n", szfile);
        delete root;
        free(handler);
        return NULL;
    }

    XNode* ndfix = root->nods[0];

    string prelude = "FIX." + ndfix->atts["major"] + "." + ndfix->atts["minor"];
    fprintf(stderr,"%s\n", prelude.c_str());

    // unhook from top node, and cleanup

    ndfix->parent=NULL;
    root->nods.clear();

    xmlCleanupParser(); 
    delete root;
    free(handler);

    return ndfix;
}


// MessageGenerator


MessageGenerator::MessageGenerator(XNode* fix)
    : ndfix(fix)
    , nsent(2000)
    , soh(1, 0x01)
{
    ndheader = ndfix->child("header");
    ndtrailer= ndfix->child("trailer");
    ndmsgs   = ndfix->child("messages");
    ndcomps  = ndfix->child("components");      // FIX 4.2 doesnt have components :]
    ndfields = ndfix->child("fields");

    prelude = "FIX." + ndfix->atts["major"] + "." + ndfix->atts["minor"];


    //ndheader->xtrace();
    //ndtrailer->xtrace();

    // index  messages, components, fields etc into maps for faster lookup

    for (vecx::iterator pc=ndmsgs->nods.begin();pc!=ndmsgs->nods.end();pc++)
        messages[(*pc)->atts["msgtype"]] = *pc;

    XNode* ndD = messages["D"];                             //TEST
    assert(ndD);
    //ndD->xtrace();

    if (ndcomps)
    {
        for (vecx::iterator pc=ndcomps->nods.begin();pc!=ndcomps->nods.end();pc++)
            components[(*pc)->atts["name"]] = *pc;

        //XNode* ndUI = components["UnderlyingInstrument"];       //TEST
        //assert(ndUI);
    }

    //ndUI->xtrace();


    for (vecx::iterator pc=ndfields->nods.begin();pc!=ndfields->nods.end();pc++)
    {
        string id = (*pc)->atts["number"];
        fields[id] = *pc;
        fields_by_name[(*pc)->atts["name"]] = id;
    }

    XNode* nd35 = fields["35"];                             //TEST
    assert(nd35);
    //nd35->xtrace();

    //assert(0==fields_by_name["TradeOriginationDate"].compare("229"));
   
    // set id for each occurrence of field [except under fields]
 
    FieldIdWriter vis_set_id(fields_by_name);
    ndheader->visit(vis_set_id);
    ndtrailer->visit(vis_set_id);
    ndmsgs->visit(vis_set_id);
    if (ndcomps)
        ndcomps->visit(vis_set_id);

    //ndfix->xtrace();                                     //TEST

}


int MessageGenerator::gen_spec(XNode* spec, mapss& msg_atts, string& result)
{
    //TODO //validate according to spec, appending to result

    if (!spec)
        return -1;

    // for each field of spec, if mandatory field, check its present

    for (vecx::iterator pc=spec->nods.begin();pc!=spec->nods.end();pc++)
    {   
        XNode* field = *pc;

        mapss& fatts = field->atts;
        string id   = fatts["id"];
        string name = fatts["name"];

        string typ = field->elt;
        if (0==typ.compare("field"))
        {
            //printf("checking for spec field %s\n", name.c_str());

            if (msg_atts.find(name)!=msg_atts.end())
            {
                // field is allowed by the spec
                // put field in the fix message

                string val = msg_atts[name];

                //printf("appending fix %s\n", id.c_str());
                   
                result += id + "=" + val + soh;
            }
        } 
        else if (0==typ.compare("component"))
        {
            //printf("component %s\n", name.c_str());

            XNode* component = components[name];

            int ret = gen_spec(component, msg_atts, result);
            assert(0==ret);
            if (ret<0)
                return ret;
        }
    }

    return 0;
}

int MessageGenerator::gen_msg(string msg_type, mapss& body_atts, string ssource, string starget, string& result)
{
    // body first

    XNode* msg = messages[msg_type];

    string sbody;
    if(gen_spec(msg, body_atts, sbody))
        return -1;

    //printf("SBODY [%s]\n", sbody.c_str());

    // header

    mapss head_atts;
    head_atts["BeginString"]    = prelude;
    head_atts["BodyLength"]     = int_to_string((int)sbody.length());
    head_atts["MsgType"]        = msg_type;
    head_atts["SenderCompID"]   = ssource;
    head_atts["TargetCompID"]   = starget;
    head_atts["MsgSeqNum"]      = int_to_string(nsent++);
    head_atts["SendingTime"]    = fix_time_now();

    string shead;
    if (gen_spec(ndheader, head_atts, shead))
        return -1;

    //printf("SHEAD [%s]\n", shead.c_str());

    string smsg = shead + sbody;

    // trailer checksum

    mapss foot_atts;
    foot_atts["CheckSum"]       = fix_checksum(smsg.c_str(), smsg.length());

    string sfoot;
    if(gen_spec(ndtrailer, foot_atts, sfoot))
        return -1;

    //printf("SFOOT [%s]\n", sfoot.c_str());

    result.append( smsg + sfoot );

    return 0;
}


int MessageGenerator::msg_bad(const char* sz, int len)
{
    string lhs = "8=" + prelude;
    if (0!=strncmp(sz, lhs.c_str(), lhs.length()))
        return printf("FIX msg, but bad FIX version : expecting %s\n", lhs.c_str()), -1;

    lhs = "8=" + prelude + soh;
    if (0!=strncmp(sz, lhs.c_str(), lhs.length()))
        return printf("FIX msg, but bad delimiter\n"), -1;

    string ssum = fix_checksum(sz, len-7);

    string rhs = "10=" + ssum + soh;
    if (0!=strncmp(sz+len-rhs.length(), rhs.c_str(), rhs.length()))
        return printf("FIX msg, but bad checksum : expecting %s\n", ssum.c_str()), -1;

    return 0;
}


XNode* MessageGenerator::load_expanded(XNode* src_spec)
{
    // walk the spec fields, expanding components and remember them for later lookup by id within scope

    // set XNode.bexpanded and nodmap, as we copy across

    if (!src_spec)
        return NULL;

    // copy node itself

    XNode* spec = new XNode();

    spec->elt     = src_spec->elt;
    spec->atts    = src_spec->atts;

    spec->bexpanded = true;

    if (src_spec->nods.empty())
        return spec;                    //no children, were done

    // copy expanded children

    MsgContext ctx(NULL,0, components, fields);  
    ctx.stack_push(src_spec);

    XNode* src_ch;
    while(NULL!=(src_ch = ctx.next_fld()))
    {
        // add expanded child to spec

        XNode* ch = load_expanded(src_ch);
        spec->nods.push_back(ch);

        if (ch->ismessage())
        {
            spec->nodmap[ch->atts["msgtype"]] = ch;
            ch->atts["id"] = ch->atts["msgtype"];           // useful to mark the id as msgtype
        }
        else if (ch->isfield())
        {
            spec->nodmap[ch->atts["id"]] = ch;
        }
        else if (ch->isgroup())
        {
            // we want the id of the first expanded child! [so we can find the group given the fix chunk of repeat]

            assert(ch->att("name"));
            string sid = fields_by_name[ch->att("name")];

            spec->nodmap[sid] = ch;         // need to lookup by id
            ch->atts["id"] = sid;           // useful to mark the id also in the node
        }
        else
            assert(false);
    }

    return spec;
}


void expand_components(mapsx& components, XNode* xmsg)
{
    // walk the tree, inserting children for field values

    struct Visitor : XNodeVisitor
    {
        mapsx& components;

        Visitor(mapsx& map)
            : components(map)
        {
        }

        virtual void post(XNode* nd)
        {
            // expand component node contents [after the node has been visited]

            if (!nd->iscomponent() || nd->bexpanded)
                return;

            XNode* src = components[nd->atts["name"]];
            assert(src);

            for(vecx::iterator p=src->nods.begin();p!=src->nods.end();p++)
            {
                XNode* xcopy = (*p)->copy();
                xcopy->parent = nd;

                // expand nested components, within components

                expand_components(components, xcopy);

                nd->nods.push_back(xcopy);
            }
            nd->bexpanded=1;
        }
    };

    Visitor V(components);
    xmsg->visit(V);
}

void expand_field_enums(mapsx& fields, XNode* xmsg)
{
    // walk the tree, inserting children for field values

    struct EnumsVisitor : XNodeVisitor
    {
        mapsx& fields;
        
        EnumsVisitor(mapsx& field_map)
            : fields(field_map)
        {
        }

        
        virtual int operator()(XNode* nd)
        {
            if (!nd->isfield() || nd->bexpanded)
                return 0;

            // after weve visited the nodes children, append value enum 

            //printf("expanding fields for [%s]\n", nd->id());

            XNode* enums = fields[nd->id()];

            assert(enums);
            for(vecx::iterator p=enums->nods.begin();p!=enums->nods.end();p++)
            {
                XNode* xenum = (*p)->copy();
                xenum->parent = nd;

                nd->nods.push_back(xenum);
            }
            nd->bexpanded=1;
            return 0;
        }
    };

    EnumsVisitor EV(fields);
    xmsg->visit(EV);
}


int MessageGenerator::show_expanded_spec(const char* szmsgtype, mapss& options)
{
    // for each of - header, trailer, and msg type
    // run through the nested components and expand
    // show as xml

    XNode* xmsg = NULL;
    if (0==strcasecmp(szmsgtype, "header"))
        xmsg = ndheader;
    else if (0==strcasecmp(szmsgtype, "trailer"))
        xmsg = ndtrailer;
    else
        xmsg = ndmsgs->lookup(szmsgtype);

    if (!xmsg)
    {
        fprintf(stderr, "unknown message type [%s]\n", szmsgtype);
        exit(-1);
    }

    expand_components(components, xmsg);

    if (!options["enums"].empty())
        expand_field_enums(fields, xmsg);         // assign string from NULL

    XMLPrintVisitor V;
    xmsg->visit(V);

    return 0;
}


void MessageGenerator::trace_fix_xspec(FixReader& fix, XNode* xspec)
{
    // trace through the fix fields, comparing with the spec as we go
    // recurse down through groups and handle group repeats

    assert(xspec && xspec->bexpanded);

    mapsi seen;   // we mark each field as seen when it appears in the message [used for checking repeats and missing reqd fields]

    // if a group, we need first group field at start of each repeat

    string sfirst_in_group;
    if (xspec->isgroup())
    {
        sfirst_in_group = xspec->nods[0]->att("id");         // this groups first child has to come first on the fix group repeat

        //printf("group starter field %s\n", sfirst_in_group.c_str());

        fix.next();

        if (0!=fix.fld.compare(sfirst_in_group))
        {
            // expecting a repeat, saw sthing else

            printf("bailing... no group starter field %s\n", sfirst_in_group.c_str());
            fix.rewind();
            return;
        }

        printf("\n%s\n", xspec->att("name"));

        XNode* xfield = xspec->lookup(fix.fld.c_str());
        trace_field_value(xfield, fix.val);

        seen[fix.fld]++;
        //printf("seen %s n=%d\n", fix.fld.c_str(), seen[fix.fld]);
    }

    // trace all fields from this xspec as they are read from the fix message [in any order]

    while(fix.next())
    {
        XNode* xfield = xspec->lookup(fix.fld.c_str());

        if (!xfield)
        {
            // unrecognised field - in the fix msg, not in the current spec / schema
            
            if ( 0!=xspec->atts["name"].compare("StandardTrailer") &&
                 (0==fix.fld.compare("93") || 0==fix.fld.compare("89") || 0==fix.fld.compare("10")) )
            {
                // not in trailer, but we hit a trailer field, then exit this scope

                //printf("bailing... hit a trailer field  in spec [%s]\n", xspec->att("name"));
                
                fix.rewind();
                check_seen(seen, xspec);
                return;
            }

            if (0==xspec->atts["name"].compare("StandardHeader") || xspec->isgroup())
            {
                // if in a group, we exit the group, its probably a field in an enclosing block

                //printf("bailing... no field in spec [%s] for [%s]\n", xspec->att("name"), fix.fld.c_str());
                fix.rewind();
                check_seen(seen, xspec);
                return;
            }

            // just a bad field, skip it

            printf("%3s                           << bad field, not in spec\n", fix.fld.c_str());

            continue;       // skip this one
        }

        // special case : first field in group means repeat

        if (!sfirst_in_group.empty() && 0==fix.fld.compare(sfirst_in_group))
        {
            //printf("bailing... seen start of next group repeat\n");
            fix.rewind();
            check_seen(seen, xspec);
            return;
        }
            
        seen[fix.fld]++;
        //printf("seen %s n=%d\n", fix.fld.c_str(), seen[fix.fld]);

        // normal handling

        if (xfield->isfield())
        {
            trace_field_value(xfield, fix.val);
        }
        else if (xfield->isgroup())
        {

            int nreps = atoi(fix.val.c_str());
            //printf("group %s expecting %d repeats\n", xfield->att("name"), nreps);

            while(nreps--)
            {
                //printf("recurse into group %s nrep=%d\n", xfield->att("name"), nreps);
    
                trace_fix_xspec(fix, xfield);
            }
        }
    }
            
    check_seen(seen, xspec);
}

void MessageGenerator::check_seen(mapsi& seen, XNode* xspec)
{
    for (int i=0;i<(int)xspec->nods.size();i++)
    {
        XNode* xfield = xspec->nods[i];
        string sid = xfield->att("id");
        int nseen = seen[sid];

        //if (nseen)
        //    printf("seen : %3d %s\n", nseen, sid.c_str());

        if (xfield->isrequired() && nseen<1)
        {
            xfield->trace("<< missing field");
            //xspec->trace("  within spec");
        }

        if (nseen>1)
            xfield->trace("<< repeated field");
    }
}


void MessageGenerator::trace_field_value(XNode* xfield, string val)
{
    // trace value as human readable   

    assert(xfield);

    string field_id     = xfield->att("id");
    string field_name   = xfield->att("name");

    string slongval;
    XNode* field_spec = fields[field_id];

    if (field_spec && field_spec->nods.size())
    {
        // its an enum, so find the long form of its value

        for (vecx::iterator pc=field_spec->nods.begin();pc!=field_spec->nods.end();pc++)
        {
            // <value enum=B description=BUY >

            XNode* xval = *pc;
            string senum = xval->atts["enum"];
            if (0==senum.compare(val))
            {
                slongval = xval->atts["description"];
                break;
            }
        }
    }

    fprintf(stderr, "%3s %-15s : %s             %s\n", field_id.c_str(), field_name.c_str(), val.c_str(), slongval.c_str());
}


///////////////////

