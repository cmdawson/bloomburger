Bloomburger - simple class to wrap the Bloomberg C++ API

The idea was to make requests as simple as possible, so for example a basic last price request looks like

    Bloomburger BB;
    BB.connect("localhost", 8194);

    vector<string> securities, fields;
    securities.push_back("AUDUSD Curncy");
    securities.push_back("RXH3 Comdty");
    fields.push_back("PX_LAST");

    Bloomburger::BBData df = BB.fetch(securities, data);

It's also possible to do historical data requests and (maybe) intraday data. See the test.cpp file for more examples

The first thing that needs elaboration on is the result of the fetch above which is of tyoe 'Bloomburger::BBData'. This thing let's you access the results with syntax like

    SomeType px_last = df["TYZ3 Comdty"][0]["PX_LAST"]

and is implemented as a "map of vectors of maps". For a simple live prices request the [0] in the middle might seem a pointless (and it is), but for historical data requests the resulst will include vector of PX_LASTs, one for each date. 

So, what's SomeType? In most cases it will just be price data which is returned as a double, however in some cases it might be an int (e.g. VOLUME) or even a (date e.g. OPT_EXPIRE_DT). C++ is a typed language, and doesn't know in advance what type you're going to need in advance so SomeType is implemented as a boost::variant, which is similar to a union in C (except it allows more complex types within it). The boost::variant in question is typedef's as Bloomburger::BBVariant, so in fact we should have written

    Bloomburger::BBVariant px_last = df["TYZ3 Comdty"][0]["PX_LAST"]

You can use this pretty transparently, use px_last as if it's a double (or whatever for differnt fields) and C++ should cast it correctly. If you need to be more explicity then it's possible to do 

    double px_last = boost::get<double>(df["TYZ3 Comdty"][0]["PX_LAST"]);

A quick look at the implementation of Bloomburger::fetch shows the mechanisms of accessing the bloomberg API. Basically you make a "request" from a "service" and need to fill in all the appropriate request fields. Then you send of the request and wait for the response to come back. In Bloomburger, this happens in the function Bloomburger::eventLoop, which checks "messages" as they come back in and then delegates them off to functions like parseSecurity which take care of deciphering the god-awful structures and putting them into the BBdata map-of-vectors-of-maps thing.

A warning about the fetchIntraDay function - when I translated this from the python version the data structure it was using corresponded to something like a map-of-vectors-of-vectors-of-maps. The extra layer of vectors seemed unnecessary so I removed it, but there might have been a good reason it was there. It's probably better to take a good look at the data that's coming back to verify it's going into the right place. (I have no bb access at the moment so can't)









