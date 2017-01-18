//
//  fixtr.h -  IX message tracer
//
//      use SAX2 to read into own Node Dom [read in all attribs and nesting]
//      load all attributes and children into generic nodes
//
//      trace & validate FIX messages based on metadata from FIXn.n.xml spec read in
//
#ifndef _FIXTR_H_
#define _FIXTR_H_
using namespace std;

#define FIXTR_JOIN_(a, b) a ## b
#define FIXTR_JOIN(a, b) FIXTR_JOIN_(a, b)
#define FIXTR_STRINGIFY_(a) #a
#define FIXTR_STRINGIFY(a) FIXTR_STRINGIFY_(a)

inline
string fix_spec_path(const char *vers) {
    return std::string(FIXTR_STRINGIFY(FIXTR_SPEC_DIR)) + "/FIX" + vers + ".xml";
}

struct XNode;

typedef map< string, int >          mapsi;
typedef map< string, string >       mapss;
typedef map< string, XNode* >       mapsx;
typedef vector< int >               veci;
typedef vector< XNode* >            vecx;


XNode*      parse_fix_spec_xml(const char* szfile); 
string      fix_time_now();
string      fix_checksum(const char* sz, int len);
string      int_to_string(int n);
void        trace_raw_fix(const char* sz, const char* msg);
void        print_node_xml(XNode* N, int nindent=0);


///


struct XNodeVisitor
{
    virtual int operator()(XNode*) {return 0;}

    virtual void pre(XNode*) {}
    virtual void post(XNode*) {}

    virtual void descend(XNode*) {}
    virtual void ascend(XNode*) {}
};


struct XNode
{
    string                  elt;        // xml element name
    mapss                   atts;       // all attributes
    vecx                    nods;       // child nodes              // memory owned by this, children freed on dtor
    XNode*                  parent;     

    bool                    bexpanded;  // if true the components are inserted inline [groups remain as nested Nodes]
    mapsx                   nodmap;     // for expanded nodes, map fld id => child node

    XNode()
        : parent(NULL)
        , bexpanded(false)
    {
    }

    XNode(const char* name, const char** zatts, XNode* par=NULL)
        : elt(name) 
        , parent(par)
        , bexpanded(false)
    {
        while(zatts && *zatts)
        {
            const char* att = *zatts++;
            const char* val = *zatts++;
            atts[att] = val;
        }

        if (parent) 
            parent->nods.push_back(this);               // add to parents children
    }

    XNode* copy()
    {
        // deep copy of XNode

        XNode* nd = new XNode();
        if (!nd)
            return NULL;

        nd->elt=elt;
        nd->atts=atts;

        for (vecx::iterator pc=nods.begin();pc!=nods.end();pc++)
            nd->nods.push_back((*pc)->copy());

        return nd;
    }

    virtual ~XNode()
    {
        for (vecx::iterator pc=nods.begin();pc!=nods.end();pc++)
            delete *pc;
    }

    XNode* child(const char* szchild)
    {
        for (vecx::iterator pc=nods.begin();pc!=nods.end();pc++)
            if (0==(*pc)->elt.compare(szchild))
                return *pc;     

        return NULL;
    }

    const char* att(const char* szatt)
    {
        mapss::iterator p = atts.find(szatt);
        if (p!=atts.end())
            return p->second.c_str(); 
        return NULL;
    }

    XNode* lookup(const char* szid)
    {
        if (!szid)   
            return NULL;

        // if we have quick index, use that, else scan nods

        if (nodmap.size())
        {
            mapsx::iterator p = nodmap.find(szid); 
            if (p!=nodmap.end())
                return p->second;
            return NULL;
        }

        for (vecx::iterator pc=nods.begin();pc!=nods.end();pc++)
        {
            const char* id = (*pc)->id();
            if (id && 0==strcmp(id, szid))
                return (*pc);
        }
        return NULL;
    }

    const char* id()
    {
        if (att("id"))
            return att("id");
        if (ismessage())
            return att("msgtype");
        if (isvalue())
            return att("enum");
        return att("name");
    }

    int visit(XNodeVisitor& V)
    {
        // depth first visit - run V on children then self
        // if visitor returns <0 on we abort returning that error

        V.pre(this);
        int ret = V(this);
        V.post(this);

        if (ret<0 || nods.size()==0)
            return ret;

        V.descend(this);
        for (vecx::iterator pc=nods.begin();pc!=nods.end();pc++)
        {
            XNode* xch=*pc;

            ret = xch->visit(V);
            if (ret<0)
            {
                V.ascend(this);
                return ret;
            }
        }

        V.ascend(this);
        return ret;
    }

    bool match(const char* zatt, const char* zval)
    {
        return ( att(zatt) && 0==strcmp(att(zatt),zval) );
    }

    bool iselt(const char* szelt)
    {
        return 0==elt.compare(szelt);
    }

    bool isfield() { return iselt("field"); }

    bool isgroup() { return iselt("group"); }

    bool ismessage() { return iselt("message"); }

    bool iscomponent() { return iselt("component"); }

    bool isvalue() { return iselt("value"); }

    bool isrequired() { return match("required", "Y"); }

    void trace(const char* msg="")
    {   
        printf("%3s %-25s", att("id") ? att("id") : "", att("name") ? att("name") : "");

        printf(" %s\n", msg);
    }

    void trace_atts(const char* msg=NULL)
    {
        // att1=val1 att2=val2 ...

        printf("%s", msg ? msg : "");

        for (mapss::iterator p=atts.begin();p!=atts.end();p++)
            printf(" %s=\"%s\"", p->first.c_str(), p->second.c_str());
    }

    void trace_nodmap()
    {
        printf(">>> %s nodmap\n", att("name"));

        for (mapsx::iterator p=nodmap.begin();p!=nodmap.end();p++)
            printf("  %s => %s\n", p->first.c_str(), p->second->att("name"));
    }

    bool depth_match(const char* zatt, const char* zval)
    {
        if (match(zatt, zval))
            return true;

        for (vecx::iterator pc=nods.begin();pc!=nods.end();pc++)
        {
            if ((*pc)->depth_match(zatt, zval))
                return true;
        }
        
        return false;
    }
};


struct XMLPrintVisitor : XNodeVisitor
{
    int nindent;

    XMLPrintVisitor(const char* szmsg=NULL)
    : nindent(0)
    {
        printf("%s", szmsg ? szmsg : "");
    }

    virtual int operator()(XNode* nd) 
    {
        if (!nd)
            return 0;
        string sindent(nindent*2, ' ');

        printf("%s<%s", sindent.c_str(), nd->elt.c_str()); 

        nd->trace_atts();
        
        printf("%s\n", nd->nods.size() ? ">" : "/>");

        return 0;
    }

    virtual void descend(XNode* nd) 
    {
        if (!nd)
            return;
        nindent++;
    }

    virtual void ascend(XNode* nd)
    {
        if (!nd)
            return;
        nindent--;
        string sindent(nindent*2, ' ');

        printf("%s</%s>\n", sindent.c_str(), nd->elt.c_str());
    }
};


struct MsgContext
{
    // context for parsing fix message chunks

    MsgContext(const char* _sz, int _len, mapsx& _components, mapsx& _fields, bool _recurse_groups=false)
        : sz(_sz)
        , len(_len)
        , npos(0)
        , nlen(_len)
        , components(_components)
        , fields(_fields)
    {
    }

    const char* sz;                     // input message buffer
    int         len;

    int         npos;                   // current pointer offset, during scan
    int         nlen;                   // len of current chunk to parse

    mapsx&      components;             // needed to expand components recursively
    mapsx&      fields;                 // needed to lookup field enum values

    string      msgtype;             

    vecx        xstack;                 // recursed components XNodes eg. msg or msg > component > group > field
    veci        istack;                 // recursed components iterator index [for the field in the XNode above]

    string      fld;                    // latest chunk parsed : fld=val^
    string      val;

    int         next_fix();             // parse next fix chunk in FIX msg
    void        rewind_fix();           // go back to previous chunk in FIX msg [so calling next_fix() will restore current state]

    XNode*      next_fld();             // step to next spec field [recurse into spec of component recursively] 

    void        trace_field();

    void stack_push(XNode* nd)
    {
        assert(nd);
        xstack.push_back(nd);
        istack.push_back(0);
    }

    void stack_pop()
    {
        xstack.pop_back();
        istack.pop_back();
    }

    void trace_stack()
    {
        for(int i=0;i!=(int)xstack.size();i++)
        {
            XNode* spec = xstack[i];

            printf("> %s ", spec->att("name") ? spec->att("name") : "");
        }
        printf("\n");
    }
};


struct FixReader
{
    // class to step through each chunk "<fld>=<val>|" of a fix message, extracting fld and val

    const char* sz;                     // input message buffer

    int         npos;                   // current pointer offset, during scan
    int         nlen;                   // len of current chunk to parse

    string      fld;                    // latest chunk parsed : fld=val^
    string      val;

    string      msgtype;

    FixReader(const char* z, int n)
        : sz(z)
        , npos(0)
        , nlen(n)
    {
    }

    int next()
    {
        // parse next FIX attribute <fld>=<val>^ [starting from npos'th position in sz]

        fld.clear();
        val.clear();

        if (npos+2 >= nlen)
            return 0;

        const char* pbeg = sz+npos;
        const char* peqs = strchr(pbeg, '=');
        const char* pval = peqs+1; 
        const char* psoh = strchr(pval, (char)0x01); 
        const char* pnxt = psoh+1;
        const char* pmax = sz+nlen;

        assert(pbeg<pmax);
        assert(pval<pmax);
        assert(pbeg<peqs);
        assert(pval<psoh);

        string fldid(pbeg, peqs);

        fld.assign(pbeg, peqs);
        val.assign(pval, psoh);

        if (0==fld.compare("35"))           // special case : 35 MsgType - remember message type from header
            msgtype = val;

        int nchunk = pnxt-pbeg;
        npos += nchunk;
        return nchunk;
    }

    void rewind()
    {
        // go back to previous chunk in FIX msg [after which, next() will restore current state]

        assert(fld.length()>0 && val.length()>0);               // rewind is one-shot
        if (fld.length()==0)
            return;

        int nchunk = fld.length() + 1 + val.length() + 1;       // rewind "<fld>=<val>|"

        fld.clear();
        val.clear();

        npos -= nchunk;
    }
};


struct MessageGenerator
{
    XNode*  ndfix;
    XNode*  ndheader;
    XNode*  ndtrailer;
    XNode*  ndmsgs;
    XNode*  ndcomps;
    XNode*  ndfields;

    mapsx   fields;                 // map field id => field node tree
    mapsx   messages;               // map msg id => message XNode* tree
    mapsx   components;             // map component name => component XNode* tree

    mapss   fields_by_name;         // map name -> id

    int     nsent;

    string  prelude;                // FIX message prelude eg. FIX.4.n
    string  soh;                    // FIX field delimiter ascii 01 as a string

    MessageGenerator(XNode* fix);

    int     gen_spec(XNode* spec, mapss& msg_atts, string& result);
    int     gen_msg(string msg_type, mapss& body_atts, string ssource, string starget, string& result);

    int     msg_bad(const char* sz, int len);               // checks sanity of prelude and checksum

    // analyze fix messages in any order [except for some specific constrains for header, group repeats etc ]

    XNode*  load_expanded(XNode* src_spec);
    int     show_expanded_spec(const char* szmsgtype, mapss& options);

    void    check_seen(mapsi& seen, XNode* xspec);
    void    trace_field_value(XNode* xfield, string val);
    void    trace_fix_xspec(FixReader& fix, XNode* xspec);      // trace the fix message according to xspec schema

};

#endif //_FIXTR_H_
