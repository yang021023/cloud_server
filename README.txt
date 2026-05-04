云端服务器运行说明

1. 功能
- SQLite 数据库存储
- REST API 服务（监听 8080，供前端使用）
- gRPC 服务（监听 50051，供边缘服务器使用）
- 默认管理员：admin / admin123
- 支持登录、注册、设备查询、历史数据、分析、报警、规则日志

2. 数据库
- 启动后自动创建 greenhouse.db

3. REST API 接口（供前端调用）
- POST /api/login
- POST /api/register
- GET /api/latest
- GET /api/history?device_id=xxx&limit=100
- GET /api/analysis?device_id=xxx
- GET /api/devices
- GET /api/alerts?device_id=xxx
- POST /api/alerts/{id}/resolve
- GET /api/rule_logs?device_id=xxx
- GET /api/crop_profiles
- GET /api/crop_profile?crop_name=xxx
- POST /api/crop
- POST /api/crop/validate

4. gRPC 接口（供边缘服务器调用）
- UploadSensorData：上传传感器数据
- Heartbeat：心跳检测
- SetCurrentCrop：设置设备作物
- GetCurrentCrop：获取设备作物
- GetCropProfile：获取作物配置
- GetAllCropProfiles：获取所有作物配置
- GetPendingControlCommands：获取待执行指令
- UpdateControlCommandStatus：更新指令状态
- ResolveLatestAlertByDevice：消解设备告警

5. 启动
- 需要安装 gRPC 和 Protobuf 依赖（通过 vcpkg 或手动安装）
- protoc 需要在 PATH 中
- grpc_cpp_plugin 需要在 PATH 中
- 运行 cmake 配置后编译启动
