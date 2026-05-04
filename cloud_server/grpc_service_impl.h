#pragma once

#include "database.h"
#include "sensor.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <memory>

class GrpcServiceImpl final : public greenhouse::SensorService::Service {
public:
    explicit GrpcServiceImpl(Database& database);

    grpc::Status UploadSensorData(grpc::ServerContext* context,
                                   const greenhouse::SensorData* request,
                                   greenhouse::UploadResponse* response) override;

    grpc::Status Heartbeat(grpc::ServerContext* context,
                            const greenhouse::HeartbeatRequest* request,
                            greenhouse::HeartbeatResponse* response) override;

    grpc::Status SetCurrentCrop(grpc::ServerContext* context,
                                 const greenhouse::SetCropRequest* request,
                                 greenhouse::SetCropResponse* response) override;

    grpc::Status GetCurrentCrop(grpc::ServerContext* context,
                                 const greenhouse::GetCropRequest* request,
                                 greenhouse::GetCropResponse* response) override;

    grpc::Status GetCropProfile(grpc::ServerContext* context,
                                 const greenhouse::GetCropProfileRequest* request,
                                 greenhouse::GetCropProfileResponse* response) override;

    grpc::Status GetAllCropProfiles(grpc::ServerContext* context,
                                     const greenhouse::GetAllCropProfilesRequest* request,
                                     greenhouse::GetAllCropProfilesResponse* response) override;

    grpc::Status GetPendingControlCommands(grpc::ServerContext* context,
                                           const greenhouse::GetPendingCommandsRequest* request,
                                           greenhouse::GetPendingCommandsResponse* response) override;

    grpc::Status UpdateControlCommandStatus(grpc::ServerContext* context,
                                             const greenhouse::UpdateCommandStatusRequest* request,
                                             greenhouse::UpdateCommandStatusResponse* response) override;

    grpc::Status ResolveLatestAlertByDevice(grpc::ServerContext* context,
                                             const greenhouse::ResolveAlertRequest* request,
                                             greenhouse::ResolveAlertResponse* response) override;

private:
    Database& database_;
};
