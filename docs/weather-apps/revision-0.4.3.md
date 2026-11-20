# 天气更新容错修复 0.4.3

版本：`1.6.5-integrated.0.4.3`；目标：`x4pro-gh_release`；候选固件，尚未刷入设备。

## 改动

- 正常天气请求仍直接使用 Open-Meteo；连接失败或直连阶段达到 8 秒时，在剩余预算不少于 8 秒的情况下，使用 Google Public DNS HTTPS JSON 接口解析服务域名，然后尝试一次备用连接。
- 备用地址只来自本轮 HTTPS 解析结果，不将天气服务器 IP 固化进固件，不修改设备或路由器 DNS 设置。备用解析可能返回相同地址；服务节点完全不可达时仍可能失败。
- 备用连接保留证书链校验，并将 TLS SNI、证书匹配名称和 HTTP Host 都设为 `api.open-meteo.com`。不跳过证书或主机名校验。
- 一次更新共用 30 秒截止时间；连接 Wi-Fi 最多等待 8 秒，无有效设备时间时最多等待 6 秒，天气连接阶段最多 8 秒，备用解析阶段最多 5 秒。HTTPS 客户端 I/O 超时由 1 秒调整为最多 4 秒，让握手有更多余量。截止时间在每次异步客户端调用前后检查；底层 SDK 的阻塞调用仍可能推迟取消/超时处理，需实机验证。
- 区分 Wi-Fi、备用解析、TLS、服务端、429 限流、设备时间、数据、超时和缓存写入错误。429 和其他 HTTP 错误不循环重试。
- 手动刷新错误不会再被低电量自动更新暂停提示遮挡；低电量自动更新保护保持原有 15%/20% 迟滞策略。
- 新请求很快完成时也会刷新一次新结果；相同批次轮询及重复通知继续去重，避免持续闪屏。
- 一次更新结束后保存 `/.crosspoint/apps/weather/diagnostic.json`，不在轮询热路径写 SD，不包含 SSID、密码或位置。

## 资源与生命周期

主请求、HTTPS DNS 请求和备用请求顺序执行，始终只有一个 HTTP 客户端。复用原有 8192 字节响应缓冲区，新增固定大小的状态、诊断字段和 16 字节 IPv4 地址缓冲区，不创建常驻任务、额外完整响应缓冲区或后台联网服务。

DNS JSON 在开始备用 TLS 连接前释放。完成、超时、取消和退出均清理连接及本请求启动的 SNTP；天气 Activity 仍负责关闭 Wi-Fi，随后按原策略进入休眠。缓存只在成功解析并原子写入后替换，失败继续保留旧数据。

## 诊断文件

`stage`：0=Wi-Fi，1=设备时间，2=直连，3=HTTPS 备用解析，4=备用天气连接，5=结束。

`result`：0=空闲，1=执行中，2=成功，3=网络失败，4=数据无效，5=超时，6=取消，7=设备时间异常，8=Wi-Fi 失败，9=备用解析失败，10=TLS 失败，11=HTTP 服务错误，12=429 限流，13=缓存保存失败。

`elapsedMs` 为本轮耗时；`error` 为客户端返回码；`tlsError`/`tlsCode`/`tlsFlags` 为最终连接的 TLS 错误；`httpStatus` 为最终 HTTP 状态；`fallback` 表示是否进入备用解析阶段。成功使用备用连接时通常是 stage=4、result=2、httpStatus=200、fallback=true。序列化结果对应最终阶段，不能用其反推直连的原始错误，直连错误另有串口日志。

## 验证

主机测试命令：`python3 scripts/test_weather.py`。覆盖生产请求状态机的直连及备用成功、TLS 身份配置、分片响应、无效和超长响应、非法 DNS 地址、限流、服务错误、断网、时钟等待、内存分配失败、取消、各阶段截止时间和 millis 回绕；同时验证解析、缓存恢复、精确位置、低电量和快速请求完成后的帧去重。

构建命令：`pio run -e x4pro-gh_release`。

实机更新后，请先确认“关于”版本，再手动刷新。检查更新时间推进；如仍失败，记录明确错误文字，并通过 USB Drive 读取上述诊断文件。之后验证天气息屏、按键恢复及真实 30 分钟自动更新。电脑测试不代表 ESP32 的实际 TLS 耗时、触摸响应、休眠电流或续航。

## 参考

- [Google Public DNS JSON DoH API](https://developers.google.com/speed/public-dns/docs/doh/json)
- [ESP-IDF 5.5.5 ESP-TLS 的 common_name/SNI 实现](https://github.com/espressif/esp-idf/blob/v5.5.5/components/esp-tls/esp_tls_mbedtls.c)
