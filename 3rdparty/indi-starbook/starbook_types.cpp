/*
 Starbook mount driver

 Copyright (C) 2018 Norbert Szulc (not7cd)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */

#include "starbook_types.h"
#include <iomanip>
#include <regex>
#include <cmath>

using namespace std;

starbook::DMS::DMS(string dms) : ln_dms{0, 0, 0, 0} {
    static regex pattern(R"((-?)(\d+)\+(\d+))");
    smatch results;
    if (regex_search(dms, results, pattern)) {
        neg = (unsigned short) (results[1].str().empty() ? 0 : 1);
        degrees = (unsigned short) stoi(results[2].str());
        minutes = (unsigned short) stoi(results[3].str());
        seconds = 0;
    } else {
        throw;
    }
}

ostream &starbook::operator<<(ostream &os, const starbook::DMS &dms) {
    if (dms.neg != 0) os << "-";
    os << fixed << setprecision(0) << setfill('0')
       << setw(3) << dms.degrees
       << setw(0) << "+"
       << setw(2) << dms.minutes
       << setw(0);
    return os;
}

starbook::HMS::HMS(string hms) : ln_hms{0, 0, 0} {
    static regex pattern(R"((\d+)\+(\d+)\.(\d+))");
    smatch results;
    if (regex_search(hms, results, pattern)) {
        hours = (unsigned short) stoi(results[1].str());
        minutes = (unsigned short) stoi(results[2].str());
        seconds = (double) stoi(results[3].str());
    } else {
        throw;
    }
}

ostream &starbook::operator<<(ostream &os, const starbook::HMS &hms) {
    os << fixed << setprecision(0) << setfill('0')
       << setw(2) << hms.hours
       << setw(0) << "+"
       << setw(2) << hms.minutes
       << setw(0) << "." << floor(hms.seconds);
    return os;
}

starbook::Equ::Equ(double ra, double dec) : lnh_equ_posn{{0, 0, 0},
                                                         {0, 0, 0, 0}} {
    ln_equ_posn target_d = {ra, dec};
    ln_equ_to_hequ(&target_d, this);
}

ostream &starbook::operator<<(ostream &os, const starbook::Equ &equ) {
    os << "RA=";
    os << static_cast<const HMS &> (equ.ra);

    os << "&DEC=";
    os << static_cast<const DMS &> (equ.dec);
    return os;
}

constexpr char sep = '+';

ostream &starbook::operator<<(ostream &os, const starbook::DateTime &obj) {
    os << setfill('0') << std::fixed << setprecision(0)
       << obj.years << sep
       << setw(2) << obj.months << sep
       << setw(2) << obj.days << sep
       << setw(2) << obj.hours << sep
       << setw(2) << obj.minutes << sep
       << setw(2) << floor(obj.seconds);
    return os;
}

std::istream &starbook::operator>>(std::istream &is, starbook::DateTime &utc) {
    int Y, M, D, h, m, s;
    std::array<char, 5> ch = {};
    is >> Y >> ch[0] >> M >> ch[1] >> D >> ch[2] >> h >> ch[3] >> m >> ch[4] >> s;

    if (!is) return is;
    for (char i : ch)
        if (i != sep) {
            is.clear(ios_base::failbit);
            return is;
        }
    utc = DateTime(Y, M, D, h, m, static_cast<double>(s));
    return is;
}

starbook::CommandResponse::CommandResponse(const std::string &url_like) : status(OK), raw(url_like) {
    if (url_like.empty()) throw runtime_error("parsing error, no payload");
    if (url_like.rfind("OK", 0) == 0) {
        status = OK;
        return;
    } else if (url_like.rfind("ERROR", 0) == 0) {
        if (url_like.rfind("ERROR:FORMAT", 0) == 0)
            status = ERROR_FORMAT;
        else if (url_like.rfind("ERROR:ILLEGAL STATE", 0) == 0)
            status = ERROR_ILLEGAL_STATE;
        else if (url_like.rfind("ERROR:BELOW HORIZONE", 0) == 0) /* it's not a typo */
            status = ERROR_BELOW_HORIZON;
        else
            status = ERROR_UNKNOWN;
        return;
    } else {
        std::string str_remaining = url_like;
        std::regex param_re(R"((\w+)=(\-?[\w\+\.]+))");
        std::smatch sm;

        while (regex_search(str_remaining, sm, param_re)) {
            std::string key = sm[1].str();
            std::string value = sm[2].str();

            payload[key] = value;
            str_remaining = sm.suffix();
        }
        if (payload.empty()) throw std::runtime_error("parsing error, couldn't parse any field");
        if (!str_remaining.empty()) throw std::runtime_error("parsing error, couldn't parse full payload");
        status = OK;
    }
}
