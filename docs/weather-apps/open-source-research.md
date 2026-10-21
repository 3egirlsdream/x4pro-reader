# GitHub 开源方案调研与复用（2026-10-20）

## 选择

采用 CrossPoint 1.6.5 的 X4 Pro 底座，移植少量自包含天气绘图代码，
自行实现中国城市设置、验证缓存、非阻塞请求及 X4 Pro 休眠接入。
没有发现已核实能够在这台 XTOS V7.5.11 设备上安全安装、恢复且覆盖全部
要求的现成版本。功能相似不等于本机兼容。

| 项目 | 核查结果 | 决策 |
|---|---|---|
| [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader/tree/1.6.5) | 固定提交 93e98bb；含 X4 Pro 构建、阅读、HAL、SD 字体、Activity；本地递归取得其固定 SDK | 阅读底座，保留阅读引擎 |
| [Lockscreens](https://github.com/t0nyz0/crosspoint-reader-lockscreens/tree/52f6c30ecfeaf09f38dfd1e7fd8be9073b2da255) | 实际读取天气、主循环、HAL、DashboardPower/UI；文档主列 X3/X4，天气依赖美国 ZIP；MIT | 复制并调整大数字及天气图标；参考唤醒分流，不复制 C3 GPIO13 电源控制 |
| [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk/tree/111fdcc7f0176c3ee38391a160ee296bf492dbd8) | 固定 SDK；有 Pro 多屏幕控制器检测、SDMMC、GT911、CW2017；休眠功耗仍需实测 | 使用现有 HAL；SDK 源码不变，HAL 增加计时和 ext1 休眠引脚保持 |
| [CrossPlay](https://github.com/ma-r-s/crossplay/blob/xteink/docs/building-apps.md) | 原生 Activity 应用及 FreeInkUI 开发资料，包含界面/输入/资源约束 | 参考组织方式，提供本项目最小注册表，不引入完整游戏固件 |
| [esphome-xteink](https://github.com/vjFaLk/esphome-xteink) | 作者报告 Pro 外设、OTA、休眠/唤醒实机测试；MIT；主要用于 Home Assistant | 硬件验证参考，不替代阅读器 |
| [CrossDiTo](https://github.com/dito94/CrossDiTo) | 作者说明为 X4 Pro 的 CrossInk 分支，含功耗优化；收益是工程估计 | 参考，不复制整套优化；不把估计当续航实测 |
| [Meteo Sleep](https://github.com/aktogde1/xteink-meteo-sleep) | 浏览器生成天气 BMP；Pro 尺寸可适配；MIT | 布局参考，不能单独满足设备自动唤醒联网 |
| [Tesserae](https://github.com/dmellok/tesserae) / [CrossInk](https://github.com/dmellok/CrossInk) | 自托管仪表盘；文档把 Pro 标为 TBD，描述 Xteink 息屏转换时取图；服务端 AGPL-3.0-or-later | 备选参考；首版不部署服务器，也不复制服务端代码 |

README 和上游声明已与需要移植的源码区分：只有本项目本地编译和主机测试
可以报告为已验证；其它设备的作者实测不会替代本机实测。

## 公共数据

GeoNames 提供 WGS84 地点坐标，固定为随源码保存的 JSON 子集并记录原 ZIP
SHA-256。350 个地点按 31 个大陆省级组分类，可离线选择，后续可编辑 JSON
并运行 `python3 scripts/build_weather_cities.py` 增加地点。不是 GPS 自动定位。

Open-Meteo 使用固定查询字段、摄氏度、米/秒、北京时间及 Unix 时间戳。
当前天气属于模型数据，界面有对应提示。测试时本机无代理直连天气 API
成功；这不等于阅读器芯片上的 TLS 和长期网络稳定性已经通过。

## 维护和发布

以固定 tag、SDK 和依赖版本为准，不自动追随 develop，也不启用来自其它
分支的自动固件更新。本定制版需通过受控的 SD/已验证更新入口安装，避免
上游自动升级覆盖本地扩展。完整许可证和代码来源见根目录 NOTICE-weather.md。

## Release / Issues 核查记录

- [正式 Release 1.6.5](https://github.com/crosspoint-reader/crosspoint-reader/releases/tag/1.6.5)
  指向 93e98bb，提供 X4 Pro 资产，说明 SD 字体、触摸和阅读修复；固定此版本。
  [Release 列表](https://github.com/crosspoint-reader/crosspoint-reader/releases) 还出现
  2.0.0rc 预发布，本项目没有迁移到 RC。
- [上游 #3767](https://github.com/crosspoint-reader/crosspoint-reader/issues/3767)
  记录 Pro 1.6.0→1.6.5 的 OTA 资产识别问题，维护者补充资产后关闭。
  这属于已运行 CrossPoint 的更新问题，不能证明 XTOS V7.5.11 首次安装兼容。
- [工具 #41](https://github.com/crosspoint-reader/crosspoint-tools/issues/41) 和
  [#28](https://github.com/crosspoint-reader/crosspoint-tools/issues/28)
  分别涉及国内锁定 Pro V7.4.4 安装失败/工具通道及完整 Flash 保存失败。
  旧版本的成功或失败均不构成本机 V7.5.11 恢复证据。
- [Lockscreens Issues](https://github.com/t0nyz0/crosspoint-reader-lockscreens/issues)
  可见开放列表为空；缺少报告不意味着 Pro 已验证。其 README/发布说明仍主列
  C3 的 X3/X4。没有采用其 USB 命令、分区或下载固件。
- GitHub 未认证 API 曾返回限流 403；通过实际 Git 克隆、固定源码和公开页面交叉读取。
  未能核实的发行、许可证细节或硬件支持不作为采用依据；参考仓库不做整体兼容性背书。

其它参考仓库的核查时提交见 [reference-revisions.json](reference-revisions.json)。
实际复制代码只来自已固定的 Lockscreens 绘图部分；其余仅参考设计。
CrossPoint 和 SDK 的完整构建配置、MIT 文本与子模块均保留在交付源码中。
