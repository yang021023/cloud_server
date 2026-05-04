#include "grpc_service_impl.h"

#include "logger.h"

GrpcServiceImpl::GrpcServiceImpl(Database& database)
    : database_(database) {
}

grpc::Status GrpcServiceImpl::UploadSensorData(grpc::ServerContext*,
                                                const greenhouse::SensorData* request,
                                                greenhouse::UploadResponse* response) {
    if (!request) {
        response->set_success(false);
        response->set_message("invalid request");
        return grpc::Status::CANCELLED;
    }

    SensorDataRecord record;
    record.device_id = request->device_id();
    record.temperature = request->temperature();
    record.humidity = request->humidity();
    record.light = request->light();
    record.timestamp = request->timestamp();
    record.location = request->location();

    const bool ok = database_.IngestSensorData(record);
    response->set_success(ok);
    response->set_message(ok ? "data ingested" : "ingestion failed");
    return grpc::Status::OK;
}

grpc::Status GrpcServiceImpl::Heartbeat(grpc::ServerContext*,
                                        const greenhouse::HeartbeatRequest*,
                                        greenhouse::HeartbeatResponse* response) {
    response->set_success(true);
    return grpc::Status::OK;
}

grpc::Status GrpcServiceImpl::SetCurrentCrop(grpc::ServerContext*,
                                             const greenhouse::SetCropRequest* request,
                                             greenhouse::SetCropResponse* response) {
    if (!request || request->device_id().empty() || request->crop_name().empty()) {
        response->set_success(false);
        return grpc::Status::CANCELLED;
    }

    database_.SetCurrentCrop(request->device_id(), request->crop_name(), "edge");
    response->set_success(true);
    return grpc::Status::OK;
}

grpc::Status GrpcServiceImpl::GetCurrentCrop(grpc::ServerContext*,
                                            const greenhouse::GetCropRequest* request,
                                            greenhouse::GetCropResponse* response) {
    if (!request) {
        return grpc::Status::CANCELLED;
    }

    const auto crop = database_.GetCurrentCrop(request->device_id());
    auto* out = response->mutable_crop();
    out->set_device_id(crop.device_id);
    out->set_crop_name(crop.crop_name);
    out->set_source(crop.source);
    out->set_updated_at(crop.updated_at);
    return grpc::Status::OK;
}

grpc::Status GrpcServiceImpl::GetCropProfile(grpc::ServerContext*,
                                             const greenhouse::GetCropProfileRequest* request,
                                             greenhouse::GetCropProfileResponse* response) {
    if (!request) {
        return grpc::Status::CANCELLED;
    }

    const auto profile = database_.GetCropProfile(request->crop_name());
    auto* out = response->mutable_profile();
    out->set_crop_name(profile.crop_name);
    out->set_min_temperature(profile.min_temperature);
    out->set_max_temperature(profile.max_temperature);
    out->set_min_humidity(profile.min_humidity);
    out->set_max_humidity(profile.max_humidity);
    out->set_min_light(profile.min_light);
    out->set_max_light(profile.max_light);
    return grpc::Status::OK;
}

grpc::Status GrpcServiceImpl::GetAllCropProfiles(grpc::ServerContext*,
                                                 const greenhouse::GetAllCropProfilesRequest*,
                                                 greenhouse::GetAllCropProfilesResponse* response) {
    const auto profiles = database_.GetAllCropProfiles();
    for (const auto& p : profiles) {
        auto* out = response->add_profiles();
        out->set_crop_name(p.crop_name);
        out->set_min_temperature(p.min_temperature);
        out->set_max_temperature(p.max_temperature);
        out->set_min_humidity(p.min_humidity);
        out->set_max_humidity(p.max_humidity);
        out->set_min_light(p.min_light);
        out->set_max_light(p.max_light);
    }
    return grpc::Status::OK;
}

grpc::Status GrpcServiceImpl::GetPendingControlCommands(grpc::ServerContext*,
                                                        const greenhouse::GetPendingCommandsRequest*,
                                                        greenhouse::GetPendingCommandsResponse* response) {
    const auto commands = database_.GetControlCommands("", "pending");
    for (const auto& cmd : commands) {
        auto* out = response->add_commands();
        out->set_id(cmd.id);
        out->set_device_id(cmd.device_id);
        out->set_timestamp(cmd.timestamp);
        out->set_crop_name(cmd.crop_name);
        out->set_command(cmd.command);
        out->set_reason(cmd.reason);
        out->set_status(cmd.status);
    }
    return grpc::Status::OK;
}

grpc::Status GrpcServiceImpl::UpdateControlCommandStatus(grpc::ServerContext*,
                                                         const greenhouse::UpdateCommandStatusRequest* request,
                                                         greenhouse::UpdateCommandStatusResponse* response) {
    if (!request) {
        response->set_success(false);
        return grpc::Status::CANCELLED;
    }

    const bool ok = database_.UpdateControlCommandStatus(request->id(), request->status());
    response->set_success(ok);
    return grpc::Status::OK;
}

grpc::Status GrpcServiceImpl::ResolveLatestAlertByDevice(grpc::ServerContext*,
                                                           const greenhouse::ResolveAlertRequest* request,
                                                           greenhouse::ResolveAlertResponse* response) {
    if (!request) {
        response->set_success(false);
        return grpc::Status::CANCELLED;
    }

    const bool ok = database_.ResolveLatestAlertByDevice(request->device_id());
    response->set_success(ok);
    return grpc::Status::OK;
}
