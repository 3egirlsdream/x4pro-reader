# X4 Pro 性能候选版 0.4.2-perf.1

设备 About 版本：`1.6.5-integrated.0.4.2-perf.1`。基于统一版 0.4.1，增加 TXT 有界预读、同页排版复用、进度与索引写入去重、索引批量读取和内存阶段计时；保留中文 TXT 换行、矢量字形缓存与字体启动读取修复；电源键默认单击休眠/唤醒。保留天气 0.2.0、桌面 0.3.0、游戏 0.2.0 和返回导航 back.1。原候选交付时未提交；现统一维护在本仓库，安装与验证状态见根 README。用户已报告 0.4.0 安装成功；本版本的安装状态以旁边交付目录的 deployment.json 为准。性能依据与实测边界见 `docs/performance-0.4.2-perf.1.zh-CN.md`。

首页为最近阅读顺序的六格分页书架，显示封面或书名、阅读进度。底部固定两行八格：全部应用、文件、文件传输、设置、天气、2048、五子棋、扫雷。计数器、设备信息及系统其他入口位于全部应用。五子棋带分片计算的本地 AI，三个游戏均支持独立存档。

保留默认简体中文、中文 TXT/EPUB、原生应用扩展、精确经纬度天气和低电量策略。顶部返回的触区、输入优先级、切页接触清除与忙碌状态显示统一修复。桌面、书库和游戏通过同一 ActivityManager 导航协议。

## 构建与安装

固定 CrossPoint：`93e98bb78702e29868a16a13b80c40e6b36ccdff`；SDK：`111fdcc7f0176c3ee38391a160ee296bf492dbd8`；Lucide：`c81680e066f45b640743ca78ae36cdedda3f0318`。构建命令 `pio run -e x4pro-gh_release`，只使用 `.pio/build/x4pro-gh_release/firmware.bin` application 镜像。

当前设备采用存储卡更新：保持 USB 供电，主机安全卸载后退出 USB Drive，进入设置 → 系统 → 从存储卡更新固件，选择 `CrossPoint-Install/x4pro-integrated-0.4.2-perf.1-candidate.bin`。等待自动重启，在关于页面核对完整版本。上个已安装版本的 application 文件保留在 `CrossPoint-Install/last-working/`，它只用于仍能进入 SD 更新的系统回退，不代表已验证原厂恢复或硬件救砖能力。

## 验证范围

本版主机 394 项 CTest、导航 101349 次断言、天气模型/缓存/坐标、《水浒传》前 120 页按需分页、真实 TTF 字形缓存与启动读取、单击电源及唤醒松键保护检查通过。游戏、桌面预览、610 项中文覆盖与固定 SDK UI 检查在 0.4.0 已通过；这些模块的游戏逻辑与布局在本版未改。主机预览采用模拟 HAL 和字体度量，并非设备截图。

实机仍需验证：长 TXT 冷开/重开耗时；桌面封面与实际触控；三个游戏进出、存档和 AI 响应；密码键盘/设置/书库返回；天气刷新闪烁；天气息屏及定时唤醒、阅读恢复、功耗和 24 小时续航。不得把主机毫秒值当作墨水屏显示耗时。既有同步网络请求仍有短暂不能取消的阶段，忙碌时隐藏无效返回箭头。

中文字体沿用 SD 卡 `Fonts/X4ProSansSC/X4ProSansSC-Regular.ttf`。应用开发见 `docs/weather-apps/developing-native-apps.md`；游戏实现见 `docs/games/`；桌面说明见 `README-Desktop.md`；导航说明见 `docs/back-navigation/`；开源许可与来源见 `NOTICE-weather.md` 及 `docs/weather-apps/`。
