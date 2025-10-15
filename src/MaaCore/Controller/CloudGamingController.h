#pragma once

#include "ControllerAPI.h"
#include "InstHelper.h"
#include "Common/AsstMsg.h"
#include <string>
#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace asst
{

class CloudGamingController : public ControllerAPI, protected InstHelper
{
public:
    CloudGamingController(const AsstCallback& callback, Assistant* inst, PlatformType type);
    virtual ~CloudGamingController();

    virtual bool connect(const std::string& adb_path, const std::string& address, const std::string& config) override;
    virtual bool inited() const noexcept override;
    virtual const std::string& get_uuid() const override;
    virtual bool screencap(cv::Mat& image_payload, bool allow_reconnect = false) override;
    virtual bool click(const Point& p) override;
    virtual bool swipe(const Point& p1, const Point& p2, int duration = 0, bool extra_swipe = false, double slope_in = 1, double slope_out = 1, bool with_pause = false) override;
    virtual bool input(const std::string& text) override;
    virtual bool press_esc() override;
    virtual void back_to_home() noexcept override;
    virtual std::pair<int, int> get_screen_res() const noexcept override;

    // 可以暂时不实现或提供空实现的接口
    virtual bool start_game(const std::string& client_type) override { return true; }
    virtual bool stop_game(const std::string& client_type) override { return true; }
    virtual size_t get_pipe_data_size() const noexcept override { return 0; }
    virtual size_t get_version() const noexcept override { return 114514; }
    virtual bool inject_input_event(const InputEvent& event) override { return false; }
    virtual ControlFeat::Feat support_features() const noexcept override;

private:
    // HTTP 请求的辅助函数
    boost::beast::http::response<boost::beast::http::dynamic_body>
    send_request(boost::beast::http::verb method, const std::string& target, const std::string& body = "", const std::string& content_type = "application/json");

    bool m_inited = false;
    std::string m_uuid;
    std::pair<int, int> m_screen_size = {0, 0};
    AsstCallback m_callback;

    // Beast 和 Asio 用于网络通信
    boost::asio::io_context m_context;
    boost::beast::tcp_stream m_stream;
    boost::beast::flat_buffer m_buffer;
    std::string m_host = "localhost";
    std::string m_port = "22888";
};

}