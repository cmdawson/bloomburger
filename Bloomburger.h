#ifndef _INC_BLOOMBURGER_HISTORICAL_H
#define _INC_BLOOMBURGER_HISTORICAL_H
#include <vector>
#include <string>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/variant.hpp>
#include <map>
#include "bbinclude.h"

// See BulkRefDataExample.cpp

class Bloomburger
{
public:
    typedef boost::variant<int,long,double,std::string,boost::gregorian::date,boost::posix_time::ptime> BBVariant;
    typedef std::map<std::string, BBVariant> BBFieldMap;
    typedef std::map<std::string, std::vector<BBFieldMap> > BBData;

private:
    BloombergLP::blpapi::SessionOptions _options;

    // unfortunately it turns out the bloomberg "C++ API" is unbelievably
    // inflexible when it comes to creating sesssions so this has to be a
    // pointer with all the horror that this potentially entails. It's not
    // really obvious if the C++ api is any easier to use than the original C.
    BloombergLP::blpapi::Session* _session;
    std::string _periodicity; 
    std::vector<std::string> _secs, _fields, _fielderrors;
    boost::gregorian::date _start, _end;
    int _nfieldexceptions;

    // Data is stored in a dictionary where each key is a security. For simple
    // data requests the dictionary values are lists of tuples
    //  [(field1, value1), ..
    // For composite or historical data you'll get lists of lists of tuples.
    // Note that a list of (key,value) tuples can be used in python to
    // construct the corresponding dictionary via dd = dict(list_of_kv_tuples)
    BBData _pdata;

    void checkMessage(BloombergLP::blpapi::Message msg);
    void parseMessage(BloombergLP::blpapi::Message msg);
    void parseSecurity(BloombergLP::blpapi::Element& security);
    void parseField(BloombergLP::blpapi::Element& field, std::vector<BBFieldMap>& parent);

    BBVariant extractFieldData(const BloombergLP::blpapi::Element& elmt);

    void eventLoop(void);

public:
    Bloomburger(void);
    ~Bloomburger(void);

    int connect(const std::string& server, int port);
    //bool disconnect(void);

    BBData fetchIntraDay(const std::string& security, const std::string& eventtype, const boost::posix_time::ptime& start, const boost::posix_time::ptime& end, const int interval);

    BBData fetchHistory(const std::vector<std::string>& securities, const std::vector<std::string>& fields, const boost::gregorian::date& start, const boost::gregorian::date& end);

    BBData fetch(const std::vector<std::string>& securities, const std::vector<std::string>& fields);

    void setPeriodicity(const std::string& periodicity) { _periodicity = periodicity; };
    std::string getPeriodicity(void) const { return _periodicity; };

    int numFieldExceptions(void) { return _nfieldexceptions; }
    //boost::python::object fieldExceptions(void);
};

#endif
