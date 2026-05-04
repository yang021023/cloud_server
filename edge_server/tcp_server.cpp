#include "tcp_server.h"

#include "json_parser.h"
#include "logger.h"

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <ctime>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

class TcpServer::Impl {
public:
    explicit Impl(int port)
        : port_(port),
          acceptor_(io_context_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), static_cast<unsigned short>(port))),
          crop_config_("crop_config.txt"),
          network_monitor_(grpc_client_),
          offline_controller_(crop_config_, "offline_operations.log") {
    }

    ~Impl() {
        StopBackgroundWorkers();
    }

    bool Start() {
        if (!crop_config_.Load()) {
            Logger::Error("[EdgeServer] Failed to load crop_config.txt");
            return false;
        }

        const CropProfile default_profile = crop_config_.GetProfile("default");
        Logger::Info("[EdgeServer] crop_config loaded | crop=default" +
                     std::string(" | temp=") + std::to_string(default_profile.min_temperature) + "-" + std::to_string(default_profile.max_temperature) +
                     " | humidity=" + std::to_string(default_profile.min_humidity) + "-" + std::to_string(default_profile.max_humidity) +
                     " | light=" + std::to_string(default_profile.min_light) + "-" + std::to_string(default_profile.max_light));

        StartBackgroundWorkers();
        network_monitor_.Start();
        Logger::Info("[EdgeServer] TCP server listening on 0.0.0.0:" + std::to_string(port_));
        StartAccept();
        io_context_.run();
        network_monitor_.Stop();
        StopBackgroundWorkers();
        return true;
    }

private:
    struct Session : public std::enable_shared_from_this<Session> {
        Session(asio::ip::tcp::socket socket, Impl& owner)
            : socket(std::move(socket)), owner(owner) {
        }

        void Start() {
            Logger::Info("[EdgeServer] New TCP client connected.");
            ReadLine();
        }

        void ReadLine() {
            auto self = shared_from_this();
            asio::async_read_until(socket, buffer, '\n',
                [this, self](const asio::error_code& ec, std::size_t) {
                    if (ec) {
                        Logger::Info("[EdgeServer] TCP client disconnected.");
                        return;
                    }

                    std::istream stream(&buffer);
                    std::string line;
                    std::getline(stream, line);
                    if (!line.empty() && line.back() == '\r') {
                        line.pop_back();
                    }

                    owner.ProcessLine(line);
                    ReadLine();
                });
        }

        asio::ip::tcp::socket socket;
        asio::streambuf buffer;
        Impl& owner;
    };

    void StartAccept() {
        acceptor_.async_accept([this](const asio::error_code& ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                std::make_shared<Session>(std::move(socket), *this)->Start();
            } else {
                Logger::Error("[EdgeServer] accept failed: " + ec.message());
            }
            StartAccept();
        });
    }

    void StartBackgroundWorkers() {
        workers_running_ = true;

        crop_sync_thread_ = std::thread([this]() {
            while (workers_running_.load()) {
                const auto profiles = grpc_client_.GetAllCropProfiles();
                for (const auto& cloud_profile : profiles) {
                    CropProfile profile;
                    profile.min_temperature = cloud_profile.min_temperature;
                    profile.max_temperature = cloud_profile.max_temperature;
                    profile.min_humidity = cloud_profile.min_humidity;
                    profile.max_humidity = cloud_profile.max_humidity;
                    profile.min_light = cloud_profile.min_light;
                    profile.max_light = cloud_profile.max_light;
                    crop_config_.UpsertCropProfile(cloud_profile.crop_name, profile);
                }
                std::this_thread::sleep_for(std::chrono::seconds(3));
            }
        });

        command_sync_thread_ = std::thread([this]() {
            while (workers_running_.load()) {
                if (network_monitor_.IsOnline()) {
                    const auto commands = grpc_client_.GetPendingControlCommands();
                    for (const auto& cmd : commands) {
                        const bool success = ExecuteActuatorCommand(cmd);
                        const std::string status = success ? "success" : "failed";
                        AppendActuatorOperationLog(cmd, status);
                        Logger::Info("[EdgeServer] Cloud command | id=" + std::to_string(cmd.id) +
                                     " | device=" + cmd.device_id +
                                     " | command=" + cmd.command +
                                     " | result=" + status);
                        grpc_client_.UpdateControlCommandStatus(cmd.id, status);
                        if (success) {
                            grpc_client_.ResolveLatestAlertByDevice(cmd.device_id);
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::seconds(3));
            }
        });
    }

    void StopBackgroundWorkers() {
        workers_running_ = false;
        if (crop_sync_thread_.joinable()) {
            crop_sync_thread_.join();
        }
        if (command_sync_thread_.joinable()) {
            command_sync_thread_.join();
        }
    }

    bool ExecuteActuatorCommand(const ControlCommandDto& cmd) const {
        return cmd.command == "TURN_ON_FAN" ||
               cmd.command == "START_IRRIGATION" ||
               cmd.command == "TURN_ON_GROW_LIGHT" ||
               cmd.command == "KEEP_STABLE" ||
               cmd.command == "CHANGE_CROP";
    }

    void AppendActuatorOperationLog(const ControlCommandDto& cmd, const std::string& status) const {
        std::ofstream out("actuator_operations.log", std::ios::app);
        if (!out.is_open()) {
            Logger::Error("[EdgeServer] Failed to open actuator_operations.log");
            return;
        }

        out << "timestamp=" << static_cast<long long>(std::time(nullptr))
            << ",command_id=" << cmd.id
            << ",device_id=" << cmd.device_id
            << ",crop=" << cmd.crop_name
            << ",command=" << cmd.command
            << ",reason=" << cmd.reason
            << ",status=" << status
            << '\n';
    }

    std::string GetDeviceCrop(const std::string& device_id) {
        auto it = device_crop_cache_.find(device_id);
        if (it != device_crop_cache_.end() && !it->second.empty()) {
            return it->second;
        }

        const std::string crop = network_monitor_.IsOnline()
            ? grpc_client_.GetCurrentCrop(device_id)
            : std::string{};
        const std::string resolved_crop = crop.empty() ? "default" : crop;
        device_crop_cache_[device_id] = resolved_crop;
        return resolved_crop;
    }

    void ProcessLine(const std::string& line) {
        if (line.empty()) {
            return;
        }

        SensorData data;
        std::string error;
        if (!JsonParser::ParseSensorData(line, data, error)) {
            Logger::Error("[EdgeServer] JSON parse failed: " + error + " | raw=" + line);
            return;
        }

        if (deduplicator_.IsDuplicate(data)) {
            Logger::Info("[EdgeServer] Duplicate dropped | device=" + data.device_id + " | ts=" + std::to_string(data.timestamp));
            return;
        }

        device_monitor_.UpdateSeen(data.device_id, data.timestamp);
        if (!data.crop_name.empty()) {
            device_crop_cache_[data.device_id] = data.crop_name;
        }
        const std::string device_crop = GetDeviceCrop(data.device_id);

        Logger::Info("[EdgeServer] SensorData | device=" + data.device_id +
                     " | location=" + data.location +
                     " | crop=" + device_crop +
                     " | temp=" + std::to_string(data.temperature) +
                     " | humidity=" + std::to_string(data.humidity) +
                     " | light=" + std::to_string(data.light) +
                     " | ts=" + std::to_string(data.timestamp));

        const auto actions = rule_engine_.Evaluate(data);
        for (const auto& action : actions) {
            Logger::Info("[RuleEngine] device=" + data.device_id + " | action=" + action);
        }

        if (network_monitor_.IsOnline()) {
            if (!device_crop.empty()) {
                grpc_client_.SetCurrentCrop(data.device_id, device_crop);
                device_crop_cache_[data.device_id] = device_crop;
            }
            data.crop_name = device_crop;
            if (!grpc_client_.UploadSensorData(data)) {
                cache_manager_.Push(data);
                Logger::Error("[EdgeServer] Cloud upload failed in ONLINE mode | cached=true | cache_size=" + std::to_string(cache_manager_.Size()));
            }
        } else {
            data.crop_name = device_crop;
            cache_manager_.Push(data);
            const CropProfile profile = crop_config_.GetProfile(device_crop);
            const auto offline_actions = offline_controller_.HandleData(data, device_crop);
            Logger::Info("[EdgeServer] OFFLINE mode | device=" + data.device_id +
                         " | crop=" + device_crop +
                         " | temp_range=" + std::to_string(profile.min_temperature) + "-" + std::to_string(profile.max_temperature) +
                         " | humidity_range=" + std::to_string(profile.min_humidity) + "-" + std::to_string(profile.max_humidity) +
                         " | light_range=" + std::to_string(profile.min_light) + "-" + std::to_string(profile.max_light) +
                         " | cache_size=" + std::to_string(cache_manager_.Size()));
            for (const auto& action : offline_actions) {
                Logger::Info("[OfflineController] device=" + data.device_id + " | action=" + action);
            }
        }

        const auto statuses = device_monitor_.Snapshot();
        for (const auto& status : statuses) {
            Logger::Info("[DeviceMonitor] device=" + status.device_id +
                         " | online=" + std::string(status.online ? "true" : "false") +
                         " | last_seen=" + std::to_string(status.last_seen));
        }
    }

    int port_ = 0;
    asio::io_context io_context_;
    asio::ip::tcp::acceptor acceptor_;
    Deduplicator deduplicator_;
    DeviceMonitor device_monitor_;
    RuleEngine rule_engine_;
    CacheManager cache_manager_{"edge_cache.json"};
    GrpcClientStub grpc_client_;
    CropConfig crop_config_;
    NetworkMonitor network_monitor_;
    OfflineController offline_controller_;
    std::unordered_map<std::string, std::string> device_crop_cache_;
    std::atomic<bool> workers_running_{false};
    std::thread crop_sync_thread_;
    std::thread command_sync_thread_;
};

TcpServer::TcpServer(int port)
    : impl_(std::make_unique<Impl>(port)) {
}

TcpServer::~TcpServer() = default;

bool TcpServer::Start() {
    return impl_->Start();
}
