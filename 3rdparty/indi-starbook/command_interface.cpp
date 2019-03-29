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

#include <exception>
#include <regex>
#include <inditelescope.h>
#include <iomanip>
#include "command_interface.h"


namespace starbook {

    CommandInterface::CommandInterface(Connection::Curl *new_connection) : connection(new_connection) {}

    static std::string read_buffer;

    static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
        INDI_UNUSED(userp);
        size_t real_size = size * nmemb;
        read_buffer.append(static_cast<char *>(contents), real_size);
        return real_size;
    }

    CommandResponse CommandInterface::SendCommand(std::string cmd) {
        CURLcode rc;
        CURL *handle = connection->getHandle();

        last_response.clear();
        read_buffer.clear();
        std::ostringstream cmd_url;

        cmd_url << "http://" << connection->host() << ":" << connection->port() << "/" << cmd;
        last_cmd_url = cmd_url.str();


        curl_easy_setopt(handle, CURLOPT_USERAGENT, "curl/7.58.0");
        /* send all data to this function  */
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &read_buffer);
        curl_easy_setopt(handle, CURLOPT_URL, cmd_url.str().c_str());

        rc = curl_easy_perform(handle);

        if (rc != CURLE_OK) {
            throw std::runtime_error(curl_easy_strerror(rc));
        }

        // all responses are hidden in HTML comments ...
        std::regex response_comment_re("<!--(.*)-->", std::regex_constants::ECMAScript);
        std::smatch comment_match;
        if (!regex_search(read_buffer, comment_match, response_comment_re)) {
            throw std::runtime_error("parsing error, response not found ");
        }

        last_response = comment_match[1].str();
        if (last_response.empty()) {
            throw std::runtime_error("parsing error, response empty");
        }

        return CommandResponse(comment_match[1].str());
    }

    ResponseCode CommandInterface::SendOkCommand(const std::string &cmd) {
        CommandResponse res = CommandInterface::SendCommand(cmd);
        return res.status;
    }

    ResponseCode CommandInterface::GotoRaDec(double ra, double dec) {
        std::ostringstream cmd;
        cmd << "GOTORADEC?" << starbook::Equ{ra, dec};
        return SendOkCommand(cmd.str());
    }

    ResponseCode CommandInterface::Align(double ra, double dec) {
        std::ostringstream cmd;
        cmd << "ALIGN?" << starbook::Equ{ra, dec};
        return SendOkCommand(cmd.str());
    }

    ResponseCode CommandInterface::Version(VersionResponse &res) {
        CommandResponse cmd_res = SendCommand("VERSION");
        if (cmd_res.status != OK) return cmd_res.status;
        res = ParseVersionResponse(cmd_res);
        return cmd_res.status;
    }

    ResponseCode CommandInterface::GetStatus(StatusResponse &res) {
        CommandResponse cmd_res = SendCommand("GETSTATUS");
        if (cmd_res.status != OK) return cmd_res.status;
        res = ParseStatusResponse(cmd_res);
        return cmd_res.status;
    }

    ResponseCode CommandInterface::GetPlace(PlaceResponse &res) {
        CommandResponse cmd_res = SendCommand("GETPLACE");
        if (cmd_res.status != OK) return cmd_res.status;
        res = ParsePlaceResponse(cmd_res);
        return cmd_res.status;
    }

    ResponseCode CommandInterface::GetTime(ln_date &res) {
        CommandResponse cmd_res = SendCommand("GETIME");
        if (cmd_res.status != OK) return cmd_res.status;
        res = ParseTimeResponse(cmd_res);
        return cmd_res.status;
    }

    ResponseCode CommandInterface::GetRound(long int &res) {
        CommandResponse cmd_res = SendCommand("GETROUND");
        if (cmd_res.status != OK) return cmd_res.status;
        res = ParseRoundResponse(cmd_res);
        return cmd_res.status;
    }

    ResponseCode CommandInterface::GetXY(XYResponse &res) {
        CommandResponse cmd_res = SendCommand("GETXY");
        if (cmd_res.status != OK) return cmd_res.status;
        res = ParseXYResponse(cmd_res);
        return cmd_res.status;
    }

    ResponseCode CommandInterface::SetTime(ln_date &local_time) {
        std::ostringstream cmd;
        cmd << "SETTIME?TIME=" << static_cast<starbook::DateTime &>(local_time);
        return SendOkCommand(cmd.str());
    }

    ResponseCode CommandInterface::SetPlace(LnLat posn, int tz) {
        std::ostringstream cmd;
        if (tz > 24 || tz < -24)
            throw std::domain_error("timezone should be between -24 and 24");
        cmd << "SETPLACE?" << posn
            << "&timezone="
            << std::setfill('0') << std::setw(2) << tz;
        return SendOkCommand(cmd.str());
    }


    ResponseCode CommandInterface::SetSpeed(int speed) {
        if (speed < MIN_SPEED || speed > MAX_SPEED)
            throw std::domain_error(
                    "speed should be between" + std::to_string(MIN_SPEED) + " and " + std::to_string(MAX_SPEED));

        std::ostringstream cmd;
        cmd << "SETSPEED?speed=" << speed;
        return SendOkCommand(cmd.str());
    }

    ResponseCode CommandInterface::Move(INDI_DIR_NS dir, INDI::Telescope::TelescopeMotionCommand command) {
        std::ostringstream cmd;
        cmd << "MOVE?NORTH="
            << (dir == DIRECTION_NORTH && command == INDI::Telescope::TelescopeMotionCommand::MOTION_START ? 1 : 0)
            << "&SOUTH="
            << (dir == DIRECTION_SOUTH && command == INDI::Telescope::TelescopeMotionCommand::MOTION_START ? 1 : 0);

        return SendOkCommand(cmd.str());
    }

    ResponseCode CommandInterface::Move(INDI_DIR_WE dir, INDI::Telescope::TelescopeMotionCommand command) {
        std::ostringstream cmd;
        cmd << "MOVE?WEST="
            << (dir == DIRECTION_WEST && command == INDI::Telescope::TelescopeMotionCommand::MOTION_START ? 1 : 0)
            << "&EAST="
            << (dir == DIRECTION_EAST && command == INDI::Telescope::TelescopeMotionCommand::MOTION_START ? 1 : 0);
        return SendOkCommand(cmd.str());
    }

    const std::string &CommandInterface::getLastCmdUrl() const {
        return last_cmd_url;
    }

    const std::string &CommandInterface::getLastResponse() const {
        return last_response;
    }

    StatusResponse CommandInterface::ParseStatusResponse(const CommandResponse &res) {
        StatusResponse result;

        lnh_equ_posn equ_posn = {{0, 0, 0},
                                 {0, 0, 0, 0}};

        starbook::HMS ra(res.payload.at("RA"));
        equ_posn.ra = ra;
        starbook::DMS dec(res.payload.at("DEC"));
        equ_posn.dec = dec;
        result.equ = {0, 0};
        ln_hequ_to_equ(&equ_posn, &result.equ);

        result.state = ParseState(res.payload.at("STATE"));
        result.executing_goto = res.payload.at("GOTO") == "1";

        return result;
    }

    VersionResponse CommandInterface::ParseVersionResponse(const CommandResponse &response) {
        VersionResponse result;

        std::regex param_re(R"(((\d+\.\d+)\w+))");
        std::smatch sm;
        if (!regex_search(response.payload.at("version"), sm, param_re)) {
            throw;
        }

        result.full_str = sm[1].str();
        result.major_minor = std::stof(sm[2]);
        return result;
    }

    StarbookState CommandInterface::ParseState(const std::string &value) {
        if (value == "SCOPE")
            return SCOPE;
        else if (value == "GUIDE")
            return GUIDE;
        else if (value == "USER")
            return USER;
        else if (value == "INIT")
            return INIT;
        else if (value == "CHART")
            return CHART;
        else if (value == "GUIDE")
            return GUIDE;
        else if (value == "ALTAZ")
            return ALTAZ;

        return UNKNOWN;
    }

    PlaceResponse CommandInterface::ParsePlaceResponse(const CommandResponse &response) {
        if (!response.status) throw std::runtime_error("can't parse place");
        return {{0, 0}, 0}; // TODO
    }

    ln_date CommandInterface::ParseTimeResponse(const CommandResponse &response) {
        if (!response.status) throw std::runtime_error("can't parse time");
        std::stringstream ss{response.payload.at("time")};
        DateTime time(0, 0, 0, 0, 0, 0);
        ss >> time;
        return time;
    }

    XYResponse CommandInterface::ParseXYResponse(const CommandResponse &response) {
        if (!response.status) throw std::runtime_error("can't parse xy");
        return {
                .x = std::stod(response.payload.at("X")),
                .y = std::stod(response.payload.at("Y"))
        };
    }

    long int CommandInterface::ParseRoundResponse(const CommandResponse &response) {
        if (!response.status) throw std::runtime_error("can't parse round");
        return std::stol(response.payload.at("ROUND"));
    }
}