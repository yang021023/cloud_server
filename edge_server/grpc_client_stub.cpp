#include "grpc_client_stub.h"

#include "sensor.grpc.pb.h"

#include <grpcpp/grpcpp.h>

namespace {
constexpr const char* kCloudAddress = "localhost:50051";
}

class GrpcClientStub::GrpcClientImpl {
public:
    GrpcClientImpl() {
        grpc::ChannelArguments args;
        channel_ = grpc::CreateCustomChannel(kCloudAddress, grpc::InsecureChannelCredentials(), args);
        stub_ = greenhouse::SensorService::NewStub(channel_);
    }

    bool UploadSensorData(const SensorData& data) const {
        grpc::ClientContext ctx;
        greenhouse::SensorData req;
        greenhouse::UploadResponse resp;

        req.set_device_id(data.device_id);
        req.set_temperature(static_cast<float>(data.temperature));
        req.set_humidity(static_cast<float>(data.humidity));
        req.set_light(static_cast<float>(data.light));
        req.set_timestamp(data.timestamp);
        req.set_crop_name(data.crop_name);
        req.set_location(data.location);

        const grpc::Status status = stub_->UploadSensorData(&ctx, req, &resp);
        return status.ok() && resp.success();
    }

    bool SendHeartbeat() const {
        grpc::ClientContext ctx;
        greenhouse::HeartbeatRequest req;
        greenhouse::HeartbeatResponse resp;
        const grpc::Status status = stub_->Heartbeat(&ctx, req, &resp);
        return status.ok() && resp.success();
    }

    bool SetCurrentCrop(const std::string& device_id, const std::string& crop_name) const {
        if (device_id.empty() || crop_name.empty()) return false;
        grpc::ClientContext ctx;
        greenhouse::SetCropRequest req;
        greenhouse::SetCropResponse resp;
        req.set_device_id(device_id);
        req.set_crop_name(crop_name);
        const grpc::Status status = stub_->SetCurrentCrop(&ctx, req, &resp);
        return status.ok() && resp.success();
    }

    std::string GetCurrentCrop(const std::string& device_id) const {
        if (device_id.empty()) return "default";
        grpc::ClientContext ctx;
        greenhouse::GetCropRequest req;
        greenhouse::GetCropResponse resp;
        req.set_device_id(device_id);
        const grpc::Status status = stub_->GetCurrentCrop(&ctx, req, &resp);
        if (!status.ok()) return "";
        return resp.crop().crop_name();
    }

    CropProfileDto GetCropProfile(const std::string& crop_name) const {
        CropProfileDto profile;
        grpc::ClientContext ctx;
        greenhouse::GetCropProfileRequest req;
        greenhouse::GetCropProfileResponse resp;
        req.set_crop_name(crop_name);
        const grpc::Status status = stub_->GetCropProfile(&ctx, req, &resp);
        if (!status.ok()) return profile;
        const auto& p = resp.profile();
        profile.crop_name = p.crop_name();
        profile.min_temperature = p.min_temperature();
        profile.max_temperature = p.max_temperature();
        profile.min_humidity = p.min_humidity();
        profile.max_humidity = p.max_humidity();
        profile.min_light = p.min_light();
        profile.max_light = p.max_light();
        return profile;
    }

    std::vector<CropProfileDto> GetAllCropProfiles() const {
        std::vector<CropProfileDto> items;
        grpc::ClientContext ctx;
        greenhouse::GetAllCropProfilesRequest req;
        greenhouse::GetAllCropProfilesResponse resp;
        const grpc::Status status = stub_->GetAllCropProfiles(&ctx, req, &resp);
        if (!status.ok()) return items;
        for (const auto& p : resp.profiles()) {
            CropProfileDto item;
            item.crop_name = p.crop_name();
            item.min_temperature = p.min_temperature();
            item.max_temperature = p.max_temperature();
            item.min_humidity = p.min_humidity();
            item.max_humidity = p.max_humidity();
            item.min_light = p.min_light();
            item.max_light = p.max_light();
            items.push_back(item);
        }
        return items;
    }

    std::vector<ControlCommandDto> GetPendingControlCommands() const {
        std::vector<ControlCommandDto> items;
        grpc::ClientContext ctx;
        greenhouse::GetPendingCommandsRequest req;
        greenhouse::GetPendingCommandsResponse resp;
        const grpc::Status status = stub_->GetPendingControlCommands(&ctx, req, &resp);
        if (!status.ok()) return items;
        for (const auto& cmd : resp.commands()) {
            ControlCommandDto item;
            item.id = cmd.id();
            item.device_id = cmd.device_id();
            item.timestamp = cmd.timestamp();
            item.crop_name = cmd.crop_name();
            item.command = cmd.command();
            item.reason = cmd.reason();
            item.status = cmd.status();
            items.push_back(item);
        }
        return items;
    }

    bool UpdateControlCommandStatus(int id, const std::string& status_str) const {
        grpc::ClientContext ctx;
        greenhouse::UpdateCommandStatusRequest req;
        greenhouse::UpdateCommandStatusResponse resp;
        req.set_id(id);
        req.set_status(status_str);
        const grpc::Status status = stub_->UpdateControlCommandStatus(&ctx, req, &resp);
        return status.ok() && resp.success();
    }

    bool ResolveLatestAlertByDevice(const std::string& device_id) const {
        grpc::ClientContext ctx;
        greenhouse::ResolveAlertRequest req;
        greenhouse::ResolveAlertResponse resp;
        req.set_device_id(device_id);
        const grpc::Status status = stub_->ResolveLatestAlertByDevice(&ctx, req, &resp);
        return status.ok() && resp.success();
    }

private:
    std::shared_ptr<grpc::ChannelInterface> channel_;
    std::unique_ptr<greenhouse::SensorService::Stub> stub_;
};

GrpcClientStub::GrpcClientStub()
    : impl_(std::make_unique<GrpcClientImpl>()) {
}

bool GrpcClientStub::UploadSensorData(const SensorData& data) const {
    return impl_->UploadSensorData(data);
}

bool GrpcClientStub::SendHeartbeat() const {
    return impl_->SendHeartbeat();
}

bool GrpcClientStub::SetCurrentCrop(const std::string& device_id, const std::string& crop_name) const {
    return impl_->SetCurrentCrop(device_id, crop_name);
}

std::string GrpcClientStub::GetCurrentCrop(const std::string& device_id) const {
    return impl_->GetCurrentCrop(device_id);
}

CropProfileDto GrpcClientStub::GetCropProfile(const std::string& crop_name) const {
    return impl_->GetCropProfile(crop_name);
}

std::vector<CropProfileDto> GrpcClientStub::GetAllCropProfiles() const {
    return impl_->GetAllCropProfiles();
}

std::vector<ControlCommandDto> GrpcClientStub::GetPendingControlCommands() const {
    return impl_->GetPendingControlCommands();
}

bool GrpcClientStub::UpdateControlCommandStatus(int id, const std::string& status) const {
    return impl_->UpdateControlCommandStatus(id, status);
}

bool GrpcClientStub::ResolveLatestAlertByDevice(const std::string& device_id) const {
    return impl_->ResolveLatestAlertByDevice(device_id);
}
