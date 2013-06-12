fixtr
=====

    linux utility to show FIX protocol messages


    Overview

        Utilities for examining FIX messages -

            fixtr - trace fix messages in human readable format [from stdin]

            fixspec - show relevant part of xml FIX spec for a given message type [or eg. D | header | footer ]


    Info

        Linux C++ command line utility to trace FIX messages from stdin.

        Standalone code base, written in C++.

        Uses libxml2 SAX2 api to parse XML FIX definitions - These are in the XML FIX definition format used by QuickFix? and QuickFixJ.

        Includes newly created FIX 5.0SP2 XML spec, to support latest FIX standard.

        Tested on Linux, g++, makefile build


    Usage

        To summarize a D msg [ NewOrderSingle ] with a single trade -

            ./fixtr < ./test/single.FIX44.D.fix

        To summarize a E msg [OrderList ] containing a group repeat for each Order -

            ./fixtr < ./test/single.FIX44.E.fix


        Using FIX5 spec, insead of default FIX44.xml -

            ./fixtr -S=spec/FIX50SP2.xml  < test/single.FIX50SP2.D.txt


        To examine for formal spec for E message -

            ./fixspec E                       
            ./fixspec E -S=spec/FIX50SP2.xml

        To same message specification, verbose, showing enum values and optional fields -

            ./fixspec E -E

        Explain Header / Trailer which bracket all valid FIX messages -

            ./fixspec header
            ./fixspec trailer
    


    To build - run make
        see makefile [ builds on linux ubuntu, depends on installation of libxml2 ]


    FIX protocol schemas

        Uses FIX schema in QuickFix / QuickFix/J format XML files, see spec/FIX*.xml
        These were borrowed from quickfix/quickfixj, where they were created from the spec at fixprotocol.org

        I created a compatible FIX50SP2.xml file which was generated from the fixprotocol.org repository 


    Information

        see fixprotocol.org for authoritative FIX specification & documentation
        see quickfixengine.org / quickfixj.org for FIX implementations [in C++ / Java respectively]

    Author

        quantblog.wordpress.com
        justgord at gmail dot com
