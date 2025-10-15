#pragma once

#include "ControllerAPI.h"
#include "InstHelper.h"
#include "Common/AsstMsg.h"
#include "Common/AsstTypes.h"
#include "Controller/Platform/PlatformIO.h"
#include <string>
#include <boost/asio.hpp>

namespace asst
{

class CloudGamingController : public ControllerAPI, protected InstHelper
{
public:
    CloudGamingController(const AsstCallback& callback, Assistant* inst, [[maybe_unused]] PlatformType type);
    virtual ~CloudGamingController();

    virtual bool connect(const std::string& adb_path, const std::string& address, const std::string& config) override;
    virtual bool inited() const noexcept override;
    virtual const std::string& get_uuid() const override;
    virtual bool screencap(cv::Mat& image_payload, [[maybe_unused]] bool allow_reconnect = false) override;
    virtual bool click(const Point& p) override;
    virtual bool swipe(const Point& p1, const Point& p2, int duration = 0, bool extra_swipe = false, double slope_in = 1, double slope_out = 1, bool with_pause = false) override;
    virtual bool input(const std::string& text) override;
    virtual bool press_esc() override;
    virtual void back_to_home() noexcept override;
    virtual std::pair<int, int> get_screen_res() const noexcept override;

    virtual bool start_game([[maybe_unused]] const std::string& client_type) override { return true; }
    virtual bool stop_game([[maybe_unused]] const std::string& client_type) override { return true; }
    virtual size_t get_pipe_data_size() const noexcept override { return 0; }
    virtual size_t get_version() const noexcept override { return 0; }
    virtual bool inject_input_event([[maybe_unused]] const InputEvent& event) override { return false; }
    virtual ControlFeat::Feat support_features() const noexcept override;

private:
    std::pair<unsigned int, std::string>
    send_request(const std::string& method, const std::string& target, const std::string& body = "", const std::string& content_type = "application/json");

    bool m_inited = false;
    std::string m_uuid;
    std::pair<int, int> m_screen_size = {0, 0};
    AsstCallback m_callback;

    boost::asio::io_context m_context;
    std::string m_host = "localhost";
    std::string m_port = "22888";
};

}