# 安装、升级与恢复

完整操作已维护在根目录 [README：解锁与首次刷系统](../../README.md#解锁破解与首次刷系统)和 [SD 卡升级](../../README.md#已安装-crosspoint通过-sd-卡升级)。

适用目标是 X4 Pro / ESP32-S3。样机原版本为国内 XTOS V7.5.11，通过维护者的 Xteink Unlocker OTA 流程安装过官方 CrossPoint 1.6.5，之后可使用 CrossPoint 的 SD 更新入口安装本分支 application。这个个案不能证明所有同版设备都兼容或有可用的返厂恢复路径。

SD 文件备份不包含内部 Flash，application 镜像不是 factory 镜像；不要写到地址 0、擦除分区或覆盖别人的 NVS。系统仍可启动时可通过 SD 更新回退到上一可用 application；本项目没有验证该样机的返厂/硬件救砖步骤，也不提供原厂镜像或管理员助手。

中文字体从仓库 resources/sdcard/Fonts/X4ProSansSC/ 复制到 SD /Fonts/X4ProSansSC/。保留自己的 .crosspoint 设置、进度和存档，不上传 Wi-Fi 配置或备份。字体来源和许可随目录提供。

天气设置入口、精确 WGS84 坐标、首次刷新及自动更新开关也见主 README。原厂安装与当前候选固件的主机验证、实机待验范围应分开理解。
