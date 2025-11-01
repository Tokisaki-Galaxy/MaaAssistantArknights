#include "CloudGamingController.h"
#include "Utils/Logger.hpp"
#include <meojson/json.hpp>
#include "MaaUtils/NoWarningCV.hpp"
#include <algorithm>
#include <cctype>
#include <istream>
#include <ostream>
#include <sstream>
#include <utility>

namespace asst
{

namespace net = boost::asio;
using tcp = net::ip::tcp;

CloudGamingController::CloudGamingController(const AsstCallback& callback, Assistant* inst, [[maybe_unused]] PlatformType type)
    : InstHelper(inst), m_callback(callback)
{
    LogTraceFunction;
}

CloudGamingController::~CloudGamingController()
{
    LogTraceFunction;
}

namespace
{
std::string trim_address(std::string address)
{
    address.erase(std::remove_if(address.begin(), address.end(), [](unsigned char c) { return std::isspace(c) != 0; }), address.end());
    return address;
}

std::pair<std::string, std::string> parse_host_port(const std::string& address)
{
    if (address.empty()) {
        return { "localhost", "22888" };
    }

    std::string trimmed = trim_address(address);

    // Strip scheme if provided
    auto scheme_pos = trimmed.find("://");
    if (scheme_pos != std::string::npos) {
        trimmed = trimmed.substr(scheme_pos + 3);
    }

    // Strip path/query fragment
    auto path_pos = trimmed.find_first_of("/\?#");
    if (path_pos != std::string::npos) {
        trimmed = trimmed.substr(0, path_pos);
    }

    auto colon_pos = trimmed.rfind(':');
    if (colon_pos == std::string::npos) {
        return { trimmed.empty() ? "localhost" : trimmed, "22888" };
    }

    std::string host = trimmed.substr(0, colon_pos);
    std::string port = trimmed.substr(colon_pos + 1);
    if (host.empty()) {
        host = "localhost";
    }
    if (port.empty()) {
        port = "22888";
    }

    return { host, port };
}
}

bool CloudGamingController::connect(const std::string&, const std::string& address, const std::string&)
{
    if (!address.empty()) {
        auto [host, port] = parse_host_port(address);
        m_host = std::move(host);
        m_port = std::move(port);
    }

    LogInfo << "Connecting to Cloud Gaming backend at " << m_host << ":" << m_port;

    auto start_response = send_request("POST", "/start");
    if (start_response.first != 200) {
        LogError << "Failed to start Cloud Gaming session. Status: " << start_response.first;
        return false;
    }

    auto response = send_request("GET", "/info");
    if (response.first != 200) {
        LogError << "Failed to get info from cloud backend. Status: " << response.first;
        return false;
    }

    try {
        auto j_opt = json::parse(response.second);
        if (!j_opt) {
            LogError << "Failed to parse JSON from cloud backend info.";
            return false;
        }
        auto& j = j_opt.value();

        m_screen_size = {static_cast<int>(j.at("width").as_integer()), static_cast<int>(j.at("height").as_integer())};
        
        if (m_screen_size.first <= 0 || m_screen_size.second <= 0) {
            LogError << "Invalid screen resolution from cloud backend.";
            return false;
        }

        m_inited = true;
        m_uuid = "cloud-game-controller";
        LogInfo << "Cloud Gaming backend connected. Resolution: " << m_screen_size.first << "x" << m_screen_size.second;
    }
    catch (const std::exception& e) {
        LogError << "Failed to parse info from Cloud Gaming backend: " << e.what();
        m_inited = false;
        return false;
    }
    return true;
}

bool CloudGamingController::inited() const noexcept
{
    return m_inited;
}

const std::string& CloudGamingController::get_uuid() const
{
    return m_uuid;
}

bool CloudGamingController::screencap(cv::Mat& image_payload, [[maybe_unused]] bool allow_reconnect)
{
    if (!inited()) return false;

    auto response = send_request("GET", "/screencap");
    if (response.first != 200) {
        LogError << "Cloud screencap failed. Status: " << response.first;
        return false;
    }
    std::vector<char> data(response.second.begin(), response.second.end());
    image_payload = cv::imdecode(data, cv::IMREAD_COLOR);
    return !image_payload.empty();
}

bool CloudGamingController::click(const Point& p)
{
    if (!inited()) return false;
    LogTrace << "Cloud click: " << p;

    json::value body = {{"x", p.x}, {"y", p.y}};
    auto response = send_request("POST", "/click", body.dumps());
    return response.first == 200;
}

bool CloudGamingController::swipe(const Point& p1, const Point& p2, int duration, bool, double, double, bool)
{
    if (!inited()) return false;
    LogTrace << "Cloud swipe: " << p1 << " -> " << p2 << " in " << duration << "ms";

    json::value body = {
        {"x1", p1.x}, {"y1", p1.y},
        {"x2", p2.x}, {"y2", p2.y},
        {"duration", duration}
    };
    auto response = send_request("POST", "/swipe", body.dumps());
    return response.first == 200;
}

bool CloudGamingController::input(const std::string& text)
{
    if (!inited()) return false;
    LogTrace << "Cloud input: " << text;

    json::value body = {{"text", text}};
    auto response = send_request("POST", "/input", body.dumps());
    return response.first == 200;
}

bool CloudGamingController::press_esc()
{
    if (!inited()) return false;
    LogTrace << "Cloud press_esc";

    json::value body = {{"keycode", "esc"}};
    auto response = send_request("POST", "/key", body.dumps());
    return response.first == 200;
}

void CloudGamingController::back_to_home() noexcept
{
    if (!inited()) return;
    LogTrace << "Cloud back_to_home";

    json::value body = {{"keycode", "home"}};
    send_request("POST", "/key", body.dumps());
}

std::pair<int, int> CloudGamingController::get_screen_res() const noexcept
{
    return m_screen_size;
}

ControlFeat::Feat CloudGamingController::support_features() const noexcept
{
    return ControlFeat::PRECISE_SWIPE;
}

bool CloudGamingController::start_game([[maybe_unused]] const std::string& client_type)
{
    LogInfo << "Starting Cloud Gaming session...";
    auto response = send_request("POST", "/start");
    if (response.first == 200) {
        LogInfo << "Cloud Gaming session started successfully.";
        return true;
    }
    else {
        LogError << "Failed to start Cloud Gaming session. Status: " << response.first;
        return false;
    }
}

bool CloudGamingController::stop_game([[maybe_unused]] const std::string& client_type)
{
    LogInfo << "Stopping Cloud Gaming session...";
    auto response = send_request("POST", "/exit");
    if (response.first == 200) {
        LogInfo << "Cloud Gaming session stopped successfully.";
        return true;
    }
    else {
        LogError << "Failed to stop Cloud Gaming session. Status: " << response.first;
        return false;
    }
}

// Private helper to send HTTP requests using pure boost::asio
std::pair<unsigned int, std::string>
CloudGamingController::send_request(const std::string& method, const std::string& target, const std::string& body, const std::string& content_type)
{
    try {
        tcp::socket socket(m_context);
        tcp::resolver resolver(m_context);
        net::connect(socket, resolver.resolve(m_host, m_port));

        std::stringstream request_stream;
        request_stream << method << " " << target << " HTTP/1.1\r\n";
        request_stream << "Host: " << m_host << ":" << m_port << "\r\n";
        request_stream << "User-Agent: MAA-CloudGamingController\r\n";
        request_stream << "Accept: */*\r\n";
        request_stream << "Connection: close\r\n";
        if (!body.empty()) {
            request_stream << "Content-Type: " << content_type << "\r\n";
            request_stream << "Content-Length: " << body.length() << "\r\n";
        }
        request_stream << "\r\n";
        if (!body.empty()) {
            request_stream << body;
        }

        net::write(socket, net::buffer(request_stream.str()));

        net::streambuf response_buf;
        boost::system::error_code ec;
        net::read(socket, response_buf, ec);

        if (ec && ec != boost::asio::error::eof) {
            throw boost::system::system_error(ec);
        }

        std::istream response_stream(&response_buf);
        std::string http_version;
        unsigned int status_code;
        std::string status_message;

        response_stream >> http_version;
        response_stream >> status_code;
        std::getline(response_stream, status_message);

        std::string header;
        while (std::getline(response_stream, header) && header != "\r") {}

        std::stringstream body_ss;
        body_ss << response_stream.rdbuf();

        return {status_code, body_ss.str()};
    }
    catch (const std::exception& e) {
        LogError << "HTTP request to " << target << " failed: " << e.what();
        return {500, ""};
    }
}

}