#include "CloudGamingController.h"
#include "Utils/Logger.hpp"
#include "Utils/Json.hpp"
#include "Utils/NoWarningCV.h"

namespace asst
{

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

CloudGamingController::CloudGamingController(const AsstCallback& callback, Assistant* inst, PlatformType type)
    : InstHelper(inst), m_callback(callback), m_stream(m_context)
{
    LogTraceFunction;
}

CloudGamingController::~CloudGamingController()
{
    LogTraceFunction;
    beast::error_code ec;
    m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
}

bool CloudGamingController::connect(const std::string&, const std::string&, const std::string&)
{
    LogInfo << "Connecting to Cloud Gaming backend at " << m_host << ":" << m_port;

    try {
        auto const results = tcp::resolver(m_context).resolve(m_host, m_port);
        m_stream.connect(results);

        auto response = send_request(http::verb::get, "/info");
        if (response.result() != http::status::ok) {
            LogError << "Failed to get info from cloud backend. Status: " << response.result_int();
            return false;
        }

        std::string body = beast::buffers_to_string(response.body().data());
        auto j = json::parse(body);
        m_screen_size = {j["width"].get<int>(), j["height"].get<int>()};
        
        if (m_screen_size.first <= 0 || m_screen_size.second <= 0) {
            LogError << "Invalid screen resolution from cloud backend.";
            return false;
        }

        m_inited = true;
        m_uuid = "cloud-game-controller";
        LogInfo << "Cloud Gaming backend connected. Resolution: " << m_screen_size.first << "x" << m_screen_size.second;
    }
    catch (const std::exception& e) {
        LogError << "Failed to connect to Cloud Gaming backend: " << e.what();
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

bool CloudGamingController::screencap(cv::Mat& image_payload, bool allow_reconnect)
{
    if (!inited()) return false;

    try {
        auto response = send_request(http::verb::get, "/screencap");
        if (response.result() != http::status::ok) {
            LogError << "Cloud screencap failed. Status: " << response.result_int();
            return false;
        }
        std::string body = beast::buffers_to_string(response.body().data());
        std::vector<char> data(body.begin(), body.end());
        image_payload = cv::imdecode(data, cv::IMREAD_COLOR);
        return !image_payload.empty();
    }
    catch (const std::exception& e) {
        LogError << "Exception during cloud screencap: " << e.what();
        return false;
    }
}

bool CloudGamingController::click(const Point& p)
{
    if (!inited()) return false;
    LogTrace << "Cloud click: " << p;

    json::value body = {{"x", p.x}, {"y", p.y}};
    auto response = send_request(http::verb::post, "/click", body.dump());
    return response.result() == http::status::ok;
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
    auto response = send_request(http::verb::post, "/swipe", body.dump());
    return response.result() == http::status::ok;
}

bool CloudGamingController::input(const std::string& text)
{
    if (!inited()) return false;
    LogTrace << "Cloud input: " << text;

    json::value body = {{"text", text}};
    auto response = send_request(http::verb::post, "/input", body.dump());
    return response.result() == http::status::ok;
}

bool CloudGamingController::press_esc()
{
    if (!inited()) return false;
    LogTrace << "Cloud press_esc";

    json::value body = {{"keycode", "esc"}};
    auto response = send_request(http::verb::post, "/key", body.dump());
    return response.result() == http::status::ok;
}

void CloudGamingController::back_to_home() noexcept
{
    if (!inited()) return;
    LogTrace << "Cloud back_to_home";

    json::value body = {{"keycode", "home"}};
    send_request(http::verb::post, "/key", body.dump());
}

std::pair<int, int> CloudGamingController::get_screen_res() const noexcept
{
    return m_screen_size;
}

ControlFeat::Feat CloudGamingController::support_features() const noexcept
{
    return ControlFeat::Screencap | ControlFeat::Click | ControlFeat::Swipe | ControlFeat::Input;
}

// Private helper to send HTTP requests
http::response<http::dynamic_body>
CloudGamingController::send_request(http::verb method, const std::string& target, const std::string& body, const std::string& content_type)
{
    http::response<http::dynamic_body> res;
    try {
        http::request<http::string_body> req{method, target, 11};
        req.set(http::field::host, m_host);
        req.set(http::field::user_agent, "MAA-CloudGamingController");
        if (!body.empty()) {
            req.set(http::field::content_type, content_type);
            req.body() = body;
            req.prepare_payload();
        }

        http::write(m_stream, req);
        http::read(m_stream, m_buffer, res);
    }
    catch (const std::exception& e) {
        LogError << "HTTP request to " << target << " failed: " << e.what();
        // In case of failure, reset the stream for the next attempt
        beast::error_code ec;
        m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        m_stream.close();
        auto const results = tcp::resolver(m_context).resolve(m_host, m_port);
        m_stream.connect(results);
    }
    return res;
}

}