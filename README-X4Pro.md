# X4 Pro 阅读 + 天气 + 原生应用（返回导航候选版 0.2.0-back.1）

本源码在天气 0.2.0 上增加共享返回触控/导航修复，版本为 `1.6.5-weather.0.2.0-back.1`。新候选未刷机、未实机验收。详见 [返回修复报告](docs/back-navigation/report.md)、[页面矩阵](docs/back-navigation/page-matrix.md)、[实机步骤](docs/back-navigation/hardware-validation.md) 与 [集成说明](docs/back-navigation/integration.md)。

基于 CrossPoint 1.6.5（93e98bb）及固定 FreeInk SDK。已通过 OTA 安装正式 CrossPoint 1.6.5，用户确认短篇中文 TXT/EPUB 的显示、翻页和返回正常。
设备已升级到 0.2.0，用户从“关于”核对版本并确认默认中文；长篇 TXT、天气刷新及息屏正在复测。
此前 0.1.1 的计数器持久化与前台天气已通过用户验证。
完整原厂恢复路径仍未验证，用户已明确选择在此条件下继续安装。

## 本版内容

- 保留 CrossPoint 的 TXT/EPUB 阅读、书库、书签、阅读进度、触摸、前光和 USB 功能；实机回归待完成。
- 首页“应用”：天气、持久化计数器、设备信息、关机。
- 350 个大陆天气地点，31 个省级组；Open-Meteo HTTPS，当前温度、湿度、风速、天气、今天及后四天预报。
- 800×480 横屏黑白天气，城市和周期设置，15/30/60 分钟，初始自动更新关闭。
- 校验缓存及临时文件/备份恢复；换城市清除旧城市显示；网络失败保留最近成功数据。
- 定时深度休眠路由、无线关闭、按键取消、阅读恢复及低电量滞回保护代码。
- 自开发 C++ 前台应用注册表、独立 JSON 数据目录、计数器和第二个示例、最小模板。
- 565 个词条完整简体中文；新安装及本版首次迁移默认中文，随附 SD 字体提供中文字形。
- TXT 首屏按需分页、增量页码缓存；无线连接状态、常驻扫描按钮和完成提示。
- 手动 WGS84 经纬度和地点名称；天气状态变化时重绘，等待联网期间保留画面。

自动更新需先选城市、保存 Wi-Fi 并手动成功获取一次。<=15% 或电量读取异常暂停；
用户唤醒且 >=20% 才恢复。明确关机不设置本轮定时器。阅读中不启动天气后台刷新。
天气显示模型天气及获取时间；不是设备上的实时气象传感器读数。

## 交付目录

- 当前目录：完整工作源码，基线由 Git HEAD 和子模块固定；本地扩展尚未提交或推送。
- `../firmware/`：候选 application binary、SHA256、源码补丁和构建清单；不是整机恢复镜像。
- `../reader-sdcard/`：兼容 SDK 的 X4ProSansSC TrueType 字体、来源清单和 OFL 许可证。
- `../weather-preview/`：实际天气绘图代码驱动的主机预览，不是实机截图。
- `../device-backup/`：原存储卡文件备份、文件校验清单；不含整机 Flash 镜像。

## 文档入口

- [0.2.0 修订、限制与复测步骤](docs/weather-apps/revision-0.2.0.md)

- [安装与恢复状态](docs/weather-apps/installation-and-recovery.md)
- [开源调研和复用](docs/weather-apps/open-source-research.md)
- [应用开发接口、模板和验证步骤](docs/weather-apps/developing-native-apps.md)
- [测试报告及待完成实机验收](docs/weather-apps/validation.md)
- [许可证和署名](NOTICE-weather.md)

## 本地构建与主机验证

需要 Python 3、PlatformIO 6.2.0、C++20 编译器；预览另需 Pillow。
子模块已在本地取得；重新克隆时运行 `git submodule update --init --recursive`。

```sh
python3 -m pip install platformio==6.2.0 Pillow
pio run -e x4pro-gh_release
python3 scripts/test_weather.py
python3 scripts/test_txt_paging.py
python3 scripts/check_chinese.py --font ../reader-sdcard/fonts/X4ProSansSC/X4ProSansSC-Regular.ttf
python3 scripts/preview_weather.py --output ../weather-preview
cmake -S test -B build/host-test
cmake --build build/host-test -j 6
ctest --test-dir build/host-test --output-on-failure
```

初次构建会联网下载工具链；上游指定 pioarduino 55.03.311，原分区表保持不变。
编译输出 `.pio/build/x4pro-gh_release/firmware.bin` 为应用镜像。不要使用生成的
factory 合并镜像或默认 upload 命令对当前国内锁定机试刷。

设备前台 HTTPS 天气已通过用户验证；实际续航、息屏切换、按键冲突及 24 小时电池测试尚未完成。
正式底座的短篇阅读基线已通过；长篇 TXT、定制功能及后续更新能力仍需实机验证。
