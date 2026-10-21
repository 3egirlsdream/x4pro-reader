# X4 Pro Reader

面向阅星曈 / Xteink X4 Pro 的个人定制固件：电子书阅读、中文桌面、低功耗天气息屏，以及可以自行开发的原生应用。

本项目 fork 自 [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)，以 CrossPoint **1.6.5** 为固定底座，保留上游 MIT 许可证与提交历史。它是替换 XTOS 的独立固件，不是在原厂系统上安装的 APK，也不保留原厂界面和历史阅读数据。上游介绍保存在 [README.upstream.md](README.upstream.md)。

## 当前版本与验证状态

| 项目 | 当前内容 |
|---|---|
| 开发分支 | `x4pro`，个人仓库默认分支；`develop` 保留上游分支 |
| 最新候选 | `1.6.5-integrated.0.4.2-perf.1` |
| 目标硬件 | X4 Pro / ESP32-S3；本分支定制功能只以 X4 Pro 为交付目标 |
| 构建目标 | `x4pro-gh_release` |
| 固件类型 | application `.bin`，5,919,328 字节；应用分区上限 6,553,600 字节 |
| 主机检查 | 394 项 CTest、返回导航、TXT 缓存/存储、真实 FreeType、单击电源、天气检查通过 |
| 实机记录 | 国内版 XTOS V7.5.11 曾通过 OTA 安装官方 CrossPoint 1.6.5，随后安装定制版；最后明确收到安装成功反馈的是 0.4.0 |
| 当前候选实测 | 已复制到样机 SD 并回读校验；尚未收到安装、响应耗时、天气定时唤醒和 24 小时续航的完整验收结果 |

0.4.0 曾出现《水浒传.txt》开书超过 5 秒、翻页超过 10 秒及唤醒慢。0.4.1 修复中文长段落换行与重复字形生成，0.4.2-perf.1 继续减少正文读取、重复排版和无变化写卡。主机 I/O 数量下降不等于设备提速百分比，不能承诺墨水屏毫秒级完成刷新。详见[性能说明](docs/performance-0.4.2-perf.1.zh-CN.md)。

## 功能

- **阅读**：中文 TXT / EPUB、书库、翻页、书签、进度、触摸、实体按键与前光；保留底座其他格式能力。
- **桌面**：每页六本书，按最近阅读排序；有封面显示封面，无封面显示书名，并显示阅读进度。
- **常用应用**：底部四列两行，依次为全部应用、文件、文件传输、设置、天气、2048、五子棋、扫雷；其余入口在全部应用中。
- **天气**：当前温度、天气、湿度、风速及五天天气；大陆城市选择，或手动设置名称与 WGS84 经纬度；本地缓存断网可用。
- **天气息屏**：默认 30 分钟，可选 15 / 60 分钟；定时唤醒获取天气后关闭 Wi-Fi 再深睡，墨水屏保持画面。阅读期间不进行后台天气刷新。
- **电量与恢复**：电量 ≤15% 或读数异常时暂停定时更新，用户唤醒且 ≥20% 后恢复；阅读中休眠后恢复原书页，自开发应用休眠后回首页；明确关机不安排天气定时唤醒。
- **原生应用**：天气、计数器、设备信息，以及 2048、带本地对弈 AI 的五子棋、扫雷。游戏有独立存档，AI 分片运行，不持续空闲刷屏。
- **中文与输入**：默认简体中文、SD 中文字体回退、返回触区和输入生命周期修复；首次升级到 0.4.1 系列会迁移为单击电源休眠/唤醒，并关闭电源双击前光等待，之后手动设置保留。

## 下载与校验

本仓库直接保存当前候选 application 和中文字体，无需下载本地开发目录或设备备份。

- [候选固件](firmware/0.4.2-perf.1/x4pro-integrated-0.4.2-perf.1-candidate.bin)（打开文件后选择 Download raw file）
- [固件 SHA256](firmware/0.4.2-perf.1/SHA256SUMS) / [构建与测试清单](firmware/0.4.2-perf.1/build-manifest.json)
- [SD 中文字体目录](resources/sdcard/Fonts/X4ProSansSC/) / [字体许可](resources/sdcard/Fonts/X4ProSansSC/LICENSE.txt)

固件 SHA256：

```text
2cce441e469be6e08bff6def681854a473be82960b0372a223cc46781fc7c108
```

macOS 校验：`shasum -a 256 <固件.bin>`；Linux 使用 `sha256sum <固件.bin>`，与上面值逐字比较。

## 解锁（破解）与首次刷系统

这里的“破解”指在自己拥有的设备上替换出厂固件、解除其安装限制。是否能传书、是否能看到 USB 磁盘，都不能单独证明 USB 刷机串口开放。首次安装与已经运行 CrossPoint 后的升级是两个不同流程。

### 先确认设备和备份

1. 在原系统“设置 → 关于”记录完整型号、硬件与 XTOS 版本，确认是 **X4 Pro**。
2. 备份整张 SD 卡。SD 文件备份不包含内部 Flash 的 bootloader、分区或 NVS，不能作为整机恢复镜像。
3. 确认适用于本机型号和版本的恢复包、恢复入口与操作方法；不能写入其他机型镜像或别人的 NVS/校准数据。
4. 保持可靠 USB 供电；核对维护者针对当前设备版本的安装说明。本仓库样机记录不证明所有 V7.5.11 设备都安全可恢复。

### 路径 A：USB 串口刷机开放

按[上游安装说明](https://github.com/crosspoint-reader/crosspoint-reader#install-firmware)检查设备选择与官方 Web 安装器。使用支持 Web Serial 的浏览器、可传数据的 USB 线，先排除浏览器、线缆和 USB 端口问题。设备可识别时，按维护者流程选择 **X4 Pro 官方 CrossPoint**，先完成基线安装并验证阅读和更新入口，再按下文 SD 流程安装本候选。

本仓库提供的是后续更新用的 application 文件，不是首次安装的 factory 合并镜像；不要把它写到地址 0，也不要猜测分区地址或执行擦除 Flash。

### 路径 B：没有可用串口的 OTA 解锁

上游给出的入口是 [Xteink Unlocker 说明](https://github.com/crosspoint-reader/crosspoint-reader#usb-locked-devices-xteink-unlocker)及其链接的[解锁工具页面](https://crosspointreader.com/#unlock-tool)。下载方式和兼容列表以维护者当时提供的信息为准；网站不可访问时先回上游仓库核对，不使用来历不明的镜像工具。

这台国内版 XTOS V7.5.11 样机的已发生流程为：

1. 使用维护者提供的 Xteink Unlocker 0.2.41，选择官方 **CrossPoint 1.6.5 / X4 Pro**。
2. 按工具提示启动管理员助手。macOS 此流程需创建临时热点并处理路由/DNS；管理员密码仅在系统弹窗输入，不填入仓库、终端命令或日志。
3. 阅读器连接工具指定热点，再按工具引导通过原系统的“检查系统更新”安装。保持供电，等待重启到 CrossPoint。
4. 在 About 核对版本，验证短篇 TXT / EPUB、触摸、按键、前光及 SD 升级入口。
5. 停止临时服务/管理员助手，恢复电脑原有共享网络、热点、路由和 DNS 设置。
6. 通过 CrossPoint 的 SD 更新入口安装本项目固件；不要把本候选随意替换进首次解锁 OTA 流程。

使用前阅读上游说明中的固件支持范围和重新锁定后的限制。本仓库没有验证这台 V7.5.11 样机的**返厂恢复路径**，也不分发原厂恢复镜像、解锁助手二进制或一键擦写脚本。

## 已安装 CrossPoint：通过 SD 卡升级

1. 阅读器进入“文件传输 → USB Drive”，连接电脑；也可使用关机后取卡的读卡器方式。
2. 备份当前 `/.crosspoint/`。其中可能含 Wi-Fi 配置、阅读进度和应用数据，应只保存在自己的设备上。
3. 下载并校验候选 `.bin`，复制到 SD 根目录，命名如 `X4Pro-0.4.2-perf.1.bin`；或保留文件名放入 `CrossPoint-Install/`。
4. 将 `resources/sdcard/Fonts/X4ProSansSC/` 整个目录复制到 SD 的 `/Fonts/X4ProSansSC/`，不要覆盖自己的设置 JSON。
5. 复制完成后回读校验固件，在电脑安全卸载；若设备仍显示 USB Drive，退出该模式。
6. 保持 USB 供电，进入“设置 → 系统 → 从存储卡更新固件”（Settings → System → SD Card Firmware Update），选择该文件并确认，等待自动重启。
7. About 应显示 **`1.6.5-integrated.0.4.2-perf.1`**。阅读设置选择 **X4ProSansSC**，先用短篇中文 TXT / EPUB 验证显示、翻页和返回，再测试自己的长书。

保留上一可用 application。系统仍能进入 SD 更新时，可按同一流程回退；这不等于原厂恢复或硬件救砖。不要删除 `.crosspoint` 或格式化卡来“加速”，以免丢失进度和配置。直接安装上游新版会覆盖本分支定制功能，升级前需重新合并和验证。

## 天气与日常配置

1. 打开“天气”，先选省市；需要更精确时设置地点名称、纬度和经度。使用 **WGS84**，不是自动 GPS 定位；其他地图坐标系需先转换。
2. 配置并保存可上网的 Wi-Fi，手动刷新一次，确认地点、温度、预报和更新时间。
3. 城市、Wi-Fi 和首次成功获取均完成后，开启天气息屏/自动更新，选择 15、30 或 60 分钟。
4. 单击电源休眠；更新完成后关闭 Wi-Fi 和前光并深睡。再单击唤醒，按原恢复目标返回。

使用免费免密钥的 Open-Meteo HTTPS，摄氏度、米/秒、北京时间。仍需网络可达且受服务条款限制。失败时保留同地点的有效缓存与时间；换地点不会把旧地点缓存当成新天气。定时深睡和功耗需实机测电流与续航，不能仅凭屏幕保持画面判断已达标。

## 从源码构建

```sh
git clone --branch x4pro --recurse-submodules https://github.com/3egirlsdream/x4pro-reader.git
cd x4pro-reader
git submodule update --init --recursive
python3 -m venv .venv
# macOS / Linux；Windows 使用 .venv\Scripts\activate
source .venv/bin/activate
python -m pip install platformio==6.2.0
python -m pip install -r requirements.txt
pio run -e x4pro-gh_release
```

测试过的工具链：PlatformIO Core 6.2.0、pioarduino 55.03.311、Arduino 3.3.11、ESP-IDF 5.5.5、Xtensa GCC 14.2.0。首次构建会下载平台、库和工具，需要访问其上游。

| 组件 | 固定修订 |
|---|---|
| CrossPoint 1.6.5 基线 | `93e98bb78702e29868a16a13b80c40e6b36ccdff` |
| FreeInk SDK | `111fdcc7f0176c3ee38391a160ee296bf492dbd8` |
| Lucide | `c81680e066f45b640743ca78ae36cdedda3f0318` |

只将 `.pio/build/x4pro-gh_release/firmware.bin` 作为当前设备的 SD 升级镜像，不用同目录的 `firmware.factory.bin`。子模块使用仓库记录的提交，不运行 `git submodule update --remote` 使 SDK 漂移。预编译候选来自对应固定源码；自建镜像请重新计算自己的 SHA256。

## 开发自己的应用

应用以 **C/C++ 原生模块编译进固件**，本地运行；更新应用需重建、安装固件。没有 APK 安装器、通用后台服务或动态原生二进制插件。上游浏览器端 JavaScript 插件与此不同。

1. 复制 `examples/native-apps/HelloToolActivity.h` 到 `src/apps/HelloToolActivity.h`。
2. 在 `lib/I18n/translations/english.yaml` 与 `chinese-simplified.yaml` 增加同名 key：

   ```yaml
   STR_APP_HELLO_TOOL: "Hello tool"
   ```

   中文文件将值写成 `"我的工具"`；其他语言可回退英文，不手改生成的 I18n 文件。
3. 在 `src/apps/AppRegistry.cpp` 引入头文件，并在现有 `APPS` 数组中注册：

   ```cpp
   #include "HelloToolActivity.h"
   // 添加到现有 APPS 数组，复用其 create 工厂：
   {"hello-tool", StrId::STR_APP_HELLO_TOOL, UIIcon::Blocks, create<HelloToolActivity>},
   ```

4. `pio run -e x4pro-gh_release` 构建，校验 application 并用 SD 更新安装。
5. 在“全部应用”打开工具，验证按键/触摸、返回、退出重开和休眠恢复。

`NativeAppActivity` 提供进入、事件、绘制和退出生命周期；`renderer` / `GUI` 绘制文字与图形，`mappedInput` 提供逻辑按键、触摸和手势，中文依赖 SD 字体回退。`loadData(doc)` / `saveData(doc)` 将最多 8 KiB JSON 保存到 `/.crosspoint/apps/<唯一ID>/state.json`。ID 最长 40 字符，只允许字母、数字、连字符和下划线；状态改变时才保存。

基础模板的 `rows` 缓冲为 12 行，长列表使用按需行提供器。**全部应用菜单已用动态行提供器，不再受旧的 11 个应用上限限制**。首页仅显示注册顺序最前四个原生应用，其余在全部应用中，不产生第三行。

原生代码共享固件内存，不是故障隔离沙箱。不要阻塞主循环、无限刷新、在渲染中联网或绕过 HAL 操作 GPIO/SD；分配用 `makeUniqueNoThrow` 并检查失败，退出时停止任务、保存变化、释放资源。更多接口见[应用开发文档](docs/weather-apps/developing-native-apps.md)，游戏实现见 [docs/games](docs/games)。

## 测试与性能诊断

```sh
cmake -S test -B build/host-test
cmake --build build/host-test -j 6
ctest --test-dir build/host-test --output-on-failure
python3 scripts/test_weather.py
python3 scripts/test_back_navigation.py
python3 scripts/test_click_power.py
python3 scripts/test_performance_storage.py
python3 scripts/test_txt_paging.py --book /path/to/your-book.txt
python3 scripts/test_reading_performance.py --font resources/sdcard/Fonts/X4ProSansSC/X4ProSansSC-Regular.ttf
python3 scripts/test_games.py
```

带 ArduinoJson 的独立主机检查依赖 PlatformIO 下载完成的库目录；测试书使用自己的合法文件，本仓库不上传样机书籍或设备备份。

本版在内存记录启动、字体初始化、导航/渲染锁、分页、写卡、面板、封面和天气调用统计。退出阅读或天气后，可用 USB Drive 读取 `/.crosspoint/performance-last.csv`；逐页热路径不写诊断文件。统计自本次启动累计，深睡后重新开始，嵌套阶段不要相加成点击总耗时。

实机分别记录首次开书、重开、触摸/实体键连续翻页、返回、游戏启动、天气刷新、单击休眠/唤醒到原书页稳定的时间，检查页码与进度。同步网络阶段、渲染锁及已启动封面解码仍可能阻塞，见性能报告的剩余热点。

## 源码与开源来源

| 路径 | 内容 |
|---|---|
| `src/activities/`、`lib/` | 阅读、桌面、输入与系统底座 |
| `src/weather/` | 天气、缓存、地点、界面与深睡调度 |
| `src/apps/`、`examples/native-apps/` | 原生应用注册、存档、游戏和模板 |
| `resources/sdcard/` | SD 字体及许可 |
| `firmware/` | 候选 application、校验值与公开清单 |
| `docs/` | 开发、性能、导航修复和参考项目研究 |

CrossPoint 为 MIT，保留其 [LICENSE](LICENSE)。FreeInk SDK 与 Lucide 按固定子模块及各自许可证使用。天气绘图部分复用的 MIT 代码、GeoNames 城市数据 CC BY 4.0、Noto 字体 SIL OFL 署名见 [NOTICE-weather.md](NOTICE-weather.md)。天气来自 [Open-Meteo](https://open-meteo.com/)，城市目录来自 [GeoNames](https://www.geonames.org/)。

参考研究与实际复用边界见[开源调研](docs/weather-apps/open-source-research.md)。仓库不包含 Wi-Fi 密码、个人城市设置、阅读记录、存储卡/内部 Flash 备份或本机管理员助手。
