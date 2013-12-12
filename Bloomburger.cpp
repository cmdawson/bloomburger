#include "Bloomburger.h"
#include <stdexcept>
#include <cstring>
#include <iostream>

using namespace BloombergLP::blpapi;
using namespace boost;
using namespace std;

namespace
{
    // Name is supposedly some string type "optimized for comparison"
    const Name SECURITY_DATA("securityData");
    const Name SECURITY_NAME("security");
    const Name DATE("date");
    const Name FIELD_ID("fieldId");
    const Name FIELD_DATA("fieldData");
    const Name FIELD_DESC("description");
    const Name FIELD_INFO("fieldInfo");
    const Name FIELD_ERROR("fieldError");
    const Name FIELD_MSG("message");
    const Name SECURITY_ERROR("securityError");
    const Name ERROR_MESSAGE("message");
    const Name FIELD_EXCEPTIONS("fieldExceptions");
    const Name ERROR_INFO("errorInfo"); 

    const Name BAR_DATA("barData");
    const Name BAR_TICK_DATA("barTickData");
    const Name OPEN("open");
    const Name HIGH("high");
    const Name LOW("low");
    const Name CLOSE("close");
    const Name VOLUME("volume");
    const Name NUM_EVENTS("numEvents");
    const Name TIME("time");
    const Name RESPONSE_ERROR("responseError");
}


Bloomburger::Bloomburger(void) : _session(0), _periodicity("DAILY")
{
}


Bloomburger::~Bloomburger(void)
{
    if (_session)
	delete _session; 
}

int Bloomburger::connect(const string& server, int port)
{
    _options.setServerHost(server.c_str());
    _options.setServerPort(port);

    if (_session)
    {
	_session->stop();
	delete _session;
    }

    _session = new Session(_options);

    if (!_session->start())
    {
	throw runtime_error("Failed to start Bloomberg session");
	return 1;
    }
    else if (!_session->openService("//blp/refdata"))
    {
	throw runtime_error("Failed to open //blp/refdata service");
	return 1;
    }

    return 0;
}

Bloomburger::BBData Bloomburger::fetchIntraDay(const string& security, const string& eventtype, const posix_time::ptime& start, const posix_time::ptime& end, const int interval)
{
    if (!_session)
	throw runtime_error("No Bloomberg session established");
    
    // Pass the securities, the start and end periods, 
    Service bbservice = _session->getService("//blp/refdata");
    if (!bbservice.isValid())
	throw runtime_error("Can't access Bloomberg Service in Bloomburger::fetchIntraDay");

    _secs.clear();
    _fields.clear();
    _fielderrors.clear();
    _pdata.clear();
    _nfieldexceptions = 0;

    _secs.resize(1);
    _fields.resize(5);

    _secs[0] = security;
    Request request = bbservice.createRequest("IntradayBarRequest");
    // Don't think API allows multiple securities for these

    request.set("security", security.c_str());
    request.set("eventType", eventtype.c_str());
    request.set("interval", interval);

    posix_time::ptime rightnow = posix_time::second_clock::local_time();

    if (start > end || end > rightnow)
	throw runtime_error("Invalid datetime range in Bloomburger::fetchIntraDay");

    // BB wants an iso string e.g. 2012-03-13T13:30:00.000
    request.set("startDateTime", posix_time::to_iso_extended_string(start).c_str());
    request.set("endDateTime", posix_time::to_iso_extended_string(end).c_str());

    // nfi what this does
    //request.set("gapFillInitialBar", true);

    _session->sendRequest(request);
    eventLoop();

    return _pdata;
}


Bloomburger::BBData Bloomburger::fetch(const vector<string>& securities, const vector<string>& fields)
{
    if (!_session)
	throw runtime_error("No Bloomberg session established");

    int nsecs=0, nfields=0;

    _fielderrors.clear();
    _pdata.clear();
    _nfieldexceptions = 0;

    _secs = securities;
    nsecs = _secs.size();

    _fields = fields;
    nfields = _fields.size();
    
    if (nsecs == 0)
	throw runtime_error("Empty securities list in Bloomburger::fetch");
    else if (nfields == 0)
	throw runtime_error("Empty field list in Bloomburger::fetch");

    Service bbservice = _session->getService("//blp/refdata");
    if (!bbservice.isValid())
	throw runtime_error("Can't access Bloomberg Service in Bloomburger::fetch");

    Request request = bbservice.createRequest("ReferenceDataRequest");

    Element esecs = request.getElement("securities");
    for (int i=0;i<nsecs;i++) 
	esecs.appendValue(_secs[i].c_str());

    Element efields = request.getElement("fields");
    for (int i=0;i<nfields;i++) 
	efields.appendValue(_fields[i].c_str());

    _session->sendRequest(request);

    eventLoop();

    return _pdata;
}

Bloomburger::BBData Bloomburger::fetchHistory(const vector<string>& securities, const vector<string>& fields, const gregorian::date& start, const gregorian::date& end)
{
    if (!_session)
	throw runtime_error("No Bloomberg session established");

    int nsecs=0, nfields=0;

    _fielderrors.clear();
    _pdata.clear();
    _nfieldexceptions = 0;

    _secs = securities;
    nsecs = _secs.size();

    _fields = fields;
    nfields = _fields.size();

    if (nsecs == 0)
	throw runtime_error("Empty securities list in Bloomburger::fetchHistory");
    else if (nfields == 0)
	throw runtime_error("Empty field list in Bloomburger::fetchHistory");

    gregorian::date today = gregorian::day_clock::local_day();
    if (start > end || end > today)
	throw runtime_error("Invalid date range in Bloomburger::fetchHistory");
    
    if (!_session)
	throw runtime_error("Not connected");

    //BloombergLP::blpapi::Session::SubscriptionStatus status = _session->SubscriptionStatus;

    Service bbservice = _session->getService("//blp/refdata");
    if (!bbservice.isValid())
	throw runtime_error("Can't access Bloomberg Service in Bloomburger::fetchHistory");

    Request request = bbservice.createRequest("BloomburgerDataRequest");

    for (int i=0; i<nsecs; i++)
	request.getElement("securities").appendValue(_secs[i].c_str());	

    for (int i=0; i<nfields; i++)
	request.getElement("fields").appendValue(_fields[i].c_str());

    request.set("periodicitySelection", _periodicity.c_str());
    request.set("startDate", gregorian::to_iso_string(start).c_str());
    request.set("endDate", gregorian::to_iso_string(end).c_str());

    _session->sendRequest(request);

    eventLoop();

    return _pdata;
}


void Bloomburger::checkMessage(Message msg)
{
    Element securityData = msg.getElement(SECURITY_DATA);

    if (securityData.hasElement(SECURITY_ERROR))
    {
	Element security_error = securityData.getElement(SECURITY_ERROR);
	string errmsg(security_error.getElementAsString(ERROR_MESSAGE));
	(errmsg += ": ") += securityData.getElementAsString(SECURITY_NAME);
	throw runtime_error(errmsg.c_str());	
    }

    Element field_exceptions = securityData.getElement(FIELD_EXCEPTIONS);
    if (field_exceptions.numValues() > 0)
    {
	Element element = field_exceptions.getValueAsElement(0);
	Element error_info = element.getElement(ERROR_INFO);
	
	string errmsg = "Error ";
	((errmsg += element.getElementAsString(FIELD_ID)) += " ") += error_info.getElementAsString(ERROR_MESSAGE);

	throw runtime_error(errmsg.c_str());
    }
}

void Bloomburger::eventLoop(void)
{
    while (true)
    {
	Event event0 = _session->nextEvent();
	MessageIterator msgIter(event0);
	while (msgIter.next())
	{
	    const Message &msg = msgIter.message();

	    if ((event0.eventType() != Event::PARTIAL_RESPONSE) && (event0.eventType() != Event::RESPONSE))
		continue;

	    // best to put this in a try loop otherwise the session will be left
	    // with unparsed events which will be read out next time. 
	    try
	    {
		parseMessage(msg);
	    }
	    catch (runtime_error& e)
	    {
	    }
	}
	if (event0.eventType() == Event::RESPONSE)
	    break;
    }
}


void Bloomburger::parseField(Element& field, vector<BBFieldMap>& parent)
{
    int ftype = field.datatype();
    BBFieldMap fmap;

    if (ftype == BLPAPI_DATATYPE_SEQUENCE)  // returned for historical requests
    {
	for (size_t i=0;i<field.numValues();i++)
	{
	    Element elmt = field.getValueAsElement(i); 
	    for (size_t j=0;j<elmt.numElements();j++)
	    {
		fmap[elmt.getElement(j).name().string()] = extractFieldData(elmt.getElement(j));
	    }
	}
    }
    else
    {
	fmap[field.name().string()] =  extractFieldData(field);
    }
    parent.push_back(fmap);
}


void Bloomburger::parseSecurity(Element& security)
{
    if (security.hasElement(FIELD_DATA))
    {
	Element fdata = security.getElement(FIELD_DATA);
	size_t nfields = fdata.numElements();

	string ticker = security.getElementAsString(SECURITY_NAME);	
	if (_pdata.find(ticker) == _pdata.end())
	    _pdata[ticker] = vector<BBFieldMap>();

	if (nfields > 0)
	{
	    for (size_t j=0;j<nfields;j++)
	    {
		Element field = fdata.getElement(j);
		parseField(field, _pdata[ticker]);
	    }
	}
	else
	{
	    parseField(fdata, _pdata[ticker]);
	}
    }

    Element fieldExceptions = security.getElement(FIELD_EXCEPTIONS);
    _nfieldexceptions += fieldExceptions.numValues();

    /*for (size_t k = 0; k < fieldExceptions.numValues(); ++k)
    {
	Element fieldException = fieldExceptions.getValueAsElement(k);
	Element errInfo = fieldException.getElement(ERROR_INFO);
	cout << "\tField exception on " 
	    << fieldException.getElementAsString(FIELD_ID) << " " 
	    << errInfo.getElementAsString(ERROR_MESSAGE) << endl;
    }*/
}


void Bloomburger::parseMessage(Message msg)
{
    if (msg.hasElement(SECURITY_DATA))
    {
	Element secdata = msg.getElement(SECURITY_DATA);
	if (secdata.hasElement(SECURITY_ERROR))
	{
	    string errmsg = secdata.getElement(SECURITY_ERROR).getElementAsString(ERROR_MESSAGE);
	    (errmsg += ": ") += secdata.getElementAsString(SECURITY_NAME);	
	    throw runtime_error(errmsg.c_str());	
	}

	if (secdata.hasElement(FIELD_DATA))
	{
	    parseSecurity(secdata);
	}
	else
	{
	    size_t nsecs = secdata.numValues();
	    for (size_t i=0;i<nsecs;i++)
	    {
		Element isec = secdata.getValueAsElement(i);
		parseSecurity(isec);
	    }
	}
    }
    else if (msg.hasElement(BAR_DATA))
    {
	Element bardata = msg.getElement(BAR_DATA);

	_pdata[_secs[0]] = vector<BBFieldMap>();
	
	Element bartickdata = bardata.getElement(BAR_TICK_DATA);
	int numBars = bartickdata.numValues();

	for (int i=0;i<numBars;i++)
	{
	    Element bar = bartickdata.getValueAsElement(i);
	    size_t nfields = bar.numElements(); // should be 8
	    vector<BBFieldMap> fm;
	    for (size_t j=0;j<nfields;j++)
	    {
		Element field = bar.getElement(j);
		parseField(field, fm);
	    }
	    _pdata[_secs[0]].push_back(fm[0]);
	    // Using this intermediate fm vector seems unnecessary, I am not sure
	    // why it was here originally. It could be that intraday bar data is 
	    // more complicated and needs more work to shoehorn it into BBData
	}
    }
}


Bloomburger::BBVariant Bloomburger::extractFieldData(const Element& elmt)
{
    Datetime dd;
    gregorian::date gdate; 
    char ctmp, *stmp;

    switch(elmt.datatype())
    {
	case BLPAPI_DATATYPE_BOOL:
	    return BBVariant(static_cast<long>(elmt.getValueAsBool()));

	case BLPAPI_DATATYPE_INT32:
	    return BBVariant(static_cast<long>(elmt.getValueAsInt32()));

	case BLPAPI_DATATYPE_INT64:
	    return BBVariant(static_cast<long>(elmt.getValueAsInt64()));

	case BLPAPI_DATATYPE_FLOAT32:
	    return BBVariant(static_cast<double>(elmt.getValueAsFloat32()));

	case BLPAPI_DATATYPE_FLOAT64:
	    return BBVariant(static_cast<double>(elmt.getValueAsFloat64()));

	case BLPAPI_DATATYPE_STRING:
	    return BBVariant(string(static_cast<const char *>(elmt.getValueAsString())));

	case BLPAPI_DATATYPE_DATE:
	    dd = elmt.getValueAsDatetime();
	    return BBVariant(gregorian::date(dd.year(), dd.month(), dd.day()));

	case BLPAPI_DATATYPE_TIME:
	    // if it's just a time, take it as from today
	    gdate = gregorian::day_clock::local_day();
	    dd = elmt.getValueAsDatetime();
	    return BBVariant(posix_time::ptime(gdate,posix_time::time_duration(dd.hours(),dd.minutes(),dd.seconds())));

	case BLPAPI_DATATYPE_DATETIME:	// TODO
	    dd = elmt.getValueAsDatetime();
	    // Sometimes returns a date of 01/01/0001. pretend this means today
	    try 
	    {
		gdate = gregorian::date(dd.year(),dd.month(),dd.day());
	    }
	    catch (out_of_range& oor)
	    {
		gdate = gregorian::day_clock::local_day();
	    }
	    return BBVariant(gdate);

	case BLPAPI_DATATYPE_CHAR:	
	    ctmp = static_cast<char>(elmt.getValueAsChar());
	    return BBVariant(string(&ctmp,&ctmp+1));

	default:
	    throw runtime_error("Unable to parse field value");
	    break;
    }
}

