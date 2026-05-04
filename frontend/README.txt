# 前端运行说明

## 环境要求
- Node.js 18+
- npm 9+

## 安装依赖
在 `frontend` 目录执行：

```bash
npm install
```

## 启动开发服务器

```bash
npm run dev
```

默认访问地址：
- http://localhost:5173

## 对接后端地址
当前前端默认请求：
- http://127.0.0.1:8080

请确保云端服务器已实现以下接口：
- POST /api/login
- GET /api/latest
- GET /api/history
- GET /api/analysis
- GET /api/devices
- GET /api/alerts
- POST /api/alerts/{id}/resolve
- GET /api/rule_logs

## 默认登录账号
- 用户名：admin
- 密码：admin123
