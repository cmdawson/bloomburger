#include "Bloomburger.h"

using namespace boost;
using namespace std;

int main(void)
{
    Bloomburger BB;
    vector<string> securities, fields;

    securities.push_back("AUDUSD Curncy");
    securities.push_back("RXH3 Comdty");

    fields.push_back("OPEN");
    fields.push_back("PX_LAST");

    gregorian::date from(2013,12,1), today = gregorian::day_clock::local_day();

    BB.connect("localhost", 8194);

    // Simple live data request
    Bloomburger::BBData df = BB.fetch(securities, fields);

    // The Bloomburger::BBData type is a 'map of vectors of maps', so you'd
    // access a particular datapoint like so
    cout << df["AUDUSD Curncy"][0]["PX_LAST"] << endl;


    // Simple historical data request
    Bloomburger::BBData df2 = BB.fetchHistory(securities, fields, from, today);
    // the vector df2["AUDUSD Curncy"][i] now has all the field data on the ith
    // date, and includes an extra field ("date" i think) to identify the date
    // for example
    for (auto& x : df2["RXH3 Comdty"][0])
	cout << x.first << " : " << x.second << endl;
    cout << "--------------------" << endl;
    for (auto& x : df2["RXH3 Comdty"][1])
	cout << x.first << " : " << x.second << endl;
    // ...

    // Intraday bars request,  since midnight, 30 minute intervals
    // Note these requests only take a single security, and that I might not
    // have parsed the hideous bloomberg messages properly. 
    posix_time::ptime midnight(today, posix_time::time_duration(0,0,0));
    posix_time::ptime rightnow = posix_time::second_clock::local_time();
    Bloomburger::BBData df3 = BB.fetchIntraDay(securities[0],"TRADE",midnight,rightnow,30);
    for (auto& x : df3["AUDUSD Curncy"][0])
	cout << x.first << " : " << x.second << endl;
    // ...
}
