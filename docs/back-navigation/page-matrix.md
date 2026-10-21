# 页面与返回路径矩阵

分类：**修复**为确认的逻辑改动；**共享**为使用此次输入/触区/生命周期修复；**保留**为原语义；**禁返**为关键状态；**忙碌限制**为原有同步执行，不能称为硬件保护。所有项目均为源码盘点，未宣称实机通过。

| 页面/状态 | 返回入口和目标 | 分类与备注 |
|---|---|---|
| HomeActivity 首页 | 没有顶部箭头；实体 Back 可继续阅读；Home 键仍遵循用户配置 | 保留；首帧/无标题清理空目标 |
| LibraryListActivity 书库 | 顶部、实体、边缘 → 清搜索 → 展开分组 → 首页 | 修复；屏幕返回跳过仅回标签焦点，实体保留焦点步骤；搜索/重建按钮独立 |
| FileBrowserActivity 文件夹 | 顶部/实体/边缘 → 上一级目录；根目录 → 首页 | 共享；慢点按只退一级；实体长按回根目录保留 |
| FileBrowserActivity 固件选择 | 上级目录或取消，结果返回 SD 更新页面 | 共享；不修改恢复模式策略 |
| OPDS 服务器列表 | 返回上层设置或首页，依进入方式 | UiList/原 onBackButton |
| OpdsBookBrowserActivity 浏览 | 顶部/实体/边缘 → 原 URL 历史/父目录 | 共享，取消双回调；Back 后不继续路由本帧 |
| OPDS 加载/错误/下载 | 原错误重试/实体 Back，下载进度内 Cancel；同步请求有轮询/阻塞边界 | 保留；无可路由的标题箭头时不凭空添加；慢网络见报告限制 |
| SettingsActivity 设置标签/列表 | 顶部/边缘 → 保存设置并回首页 | 修复屏幕仅回焦点现象；实体 Back 仍先回标签 |
| AboutActivity 关于 | 标题 Back/实体/边缘 → 设置 | 共享 UiList |
| TextSettingsActivity 字体/排版设置 | Back → 原父页面，保持设置持久化规则 | 共享 |
| StatusBarSettingsActivity 状态栏 | Back → 设置 | 共享 |
| HomeButtonSettingsActivity Home 键设置 | Back → 设置 | 共享 UiList |
| ClockSettingsActivity 时钟设置 | Back → 设置 | 共享 UiList |
| TimezonePickerActivity 时区 | Back → 时钟设置，沿原取消结果 | 共享 UiList |
| LanguageSelectActivity 语言 | Back → 设置 | 共享 UiList |
| KeyboardLayoutsActivity 键盘布局 | Back → 设置 | 共享 UiList |
| ButtonRemapActivity 实体前键映射 | 顶部/边缘 → 取消；实体前键用于录入映射，侧键重置/取消 | 修复遗漏的触摸返回；不把待录入的实体 Back 截作取消 |
| OpdsSettingsActivity / KOReaderSettingsActivity | Back → 设置；编辑器返回所属配置项 | 共享 UiList/键盘 |
| FontDownloadActivity 分组/字体/错误/完成 | 返回父组/字体列表；下载取消由原分块轮询处理 | 共享；manifest 同步加载不画可点返回；下载资源释放保持原逻辑 |
| ClockSyncActivity 时钟同步 | 成功/失败 Back → 父页面；SYNCING 尚不能即时取消 | 忙碌限制：隐藏 SYNCING 箭头；Wi-Fi 断开/重启沿用原 onExit |
| KOReaderAuthActivity 登录/注册认证 | 成功/失败 Back → 配置页面 | 忙碌限制：认证请求期间不画假箭头 |
| ClearCacheActivity 清缓存确认 | 弹窗取消/顶部/边缘 → 退出，不清理 | 修复取消后滞留；正在清理时同步执行、不显示返回 |
| OtaUpdateActivity 更新确认 | Cancel/Back → 设置；失败/无更新可返回 | 共享弹窗；检查更新同步等待无箭头；正在写入/完成重启阶段禁返 |
| SdFirmwareUpdateActivity SD 更新确认 | 取消 → 设置；恢复模式取消 → 文件选择器 | 共享生命周期；UPDATING 禁返；FAILED 可返回或重选 |
| NetworkModeSelectionActivity 传输方式 | Back → 原 onCancel | 共享 UiList |
| WifiSelectionActivity 扫描/网络列表 | Back → false 取消结果到原父页面，停止扫描 | 共享 |
| Wi-Fi 手动/自动连接中 | Back → disconnect，取消结果 | 修复手动连接不响应 |
| Wi-Fi 隐藏 SSID / 密码键盘 | 顶部、实体、边缘 → onCancel；原父流程决定回列表或结束 | 修复键盘吞触摸 |
| Wi-Fi 保存密码提示 | Back → 不保存、维持已连接的成功结果 | 保留原意，不误当断开连接 |
| Wi-Fi 忘记网络提示 | Back → 不删除凭证并重新扫描 | 保留原意 |
| Wi-Fi 已连接确认 | Back/完成 → 成功结果返回天气/传输父页面 | 共享；未改变连接保留策略 |
| Wi-Fi 连接失败 | 原取消/继续分支 → 忘记提示或列表 | 共享 |
| CrossPointWebServerActivity HTTP 传输 | Back/边缘 → 原服务关闭与退出路径；启动流程先由 Wi-Fi 子页面处理 | 共享；不改服务器清理与重启策略 |
| CalibreConnectActivity | Back → 原停止连接/传输取消并退出 | 共享；循环内 mappedInput.update 继续可识别已发布标题 |
| UsbDriveActivity 等待主机/启动失败 | Back → 原 restartToHome | 共享；实际 loop 主机测试 |
| USB Connected / IoError / 主机独占 | 不提供返回；弹出、断开、错误恢复走原重启路径 | **禁返**；ActivityManager exclusive-storage 分支不处理导航栈 |
| EPUB/TXT/XTC 正文 | 正文没有顶部 Back；实体返回、Home、阅读菜单沿原规则 | 保留；阅读区左边缘右滑仍不直接退出，避免与翻页冲突 |
| EpubReaderMenuActivity 阅读菜单 | 顶部/实体/边缘 → 原阅读 Activity 与位置 | 共享；不销毁栈中正文以实现返回 |
| EPUB/XTC 目录 | 顶部/实体/边缘 → 取消目录回正文；选章仍返回章节结果 | 共享；横竖屏安全区使用实际布局 |
| EpubReaderBookmarksActivity 书签 | Back → 正文/原父级 | 共享 UiList |
| EpubReaderPercentSelectionActivity 进度 | 顶部或页面 Cancel → 取消；OK 才应用 | 共享；仍走真实 UiAppHost/SDK 按钮路由 |
| EpubReaderFootnoteSelectActivity 脚注选择 | Back → 原书页 | 共享 |
| EPUB 已进入脚注/阅读工具层 | Back → 原脚注栈/关闭工具层，不一律回首页 | 保留原阅读生命周期；切页输入隔离 |
| DictionaryWordSelectActivity 选词 | Back → 取消选词 | 共享实体/边缘；原正文覆盖布局 |
| DictionaryDefinitionActivity 释义 | Back → 返回原阅读上下文 | 共享 |
| KOReaderSyncActivity 同步结果/无远端记录/失败/完成 | Back → returnToReader，保留原进度逻辑 | 共享；同步请求忙碌时不显示返回箭头 |
| QrDisplayActivity 二维码 | Back 或原任意点按 → 原父页面 | 共享，标题点击只生成 Back |
| EndOfBookOptions 读完菜单 | Back → 原末页/阅读逻辑 | 保留，不更改读完判断 |
| BmpViewerActivity 图片 | Back → 原文件浏览上下文 | 共享；方向改变取消旧接触 |
| IntervalSelectionActivity 数值选择 | Back/Cancel → 原父级；OK 提交 | 共享真实 SDK 按钮路由 |
| KeyboardEntryActivity 各用途键盘 | Back 优先于触摸键、光标和密码显示切换 | 修复覆盖所有键盘实例，日志不输出文本或密码 |
| ConfirmationActivity 通用确认 | Cancel/实体/边缘/点外部 → cancelled result，一次 pop | 修复共享弹窗手势吞事件；不执行默认确认 |
| OptionPopup 设置/删除/末页/OTA 等内嵌弹窗 | Back 只关闭最上层弹窗；下一次独立 Back 才处理页面 | 修复；标题的被遮盖区域不直接跨越弹窗导航 |
| FrontlightPanelActivity 前光面板 | Back/向上收起/点外部 → 原父页面 | 共享；状态栏点击排除标题返回触区；保留拖动亮度逻辑 |
| AppsMenuActivity 应用菜单 | Back → 首页应用入口 | 共享 UiList |
| NativeAppActivity / CounterActivity / DeviceInfoActivity / HelloTool 模板 | Back → 应用菜单，经原 onExit 保存/释放 | 共享；计数器持久化文件格式未变 |
| WeatherMenuActivity 天气设置 | Back → 应用菜单/原父级 | 共享 |
| CityPicker 城市/省份 | 城市级 Back → 省份；省份级 Back → 天气菜单 | 共享，分级返回保留 |
| 精确位置纬度/经度/名称键盘 | Cancel → 原回调取消序列，不提交未完成设置 | 共享键盘修复，不改 E5 坐标身份/缓存规则 |
| WeatherActivity 前台天气 | 底部 Back/实体/边缘 → 天气父级；取消 Request，Wi-Fi 退出清理 | 共享；底部空白不再命中；状态门控刷新保留 |
| WeatherActivity unattended | 无前台底部按钮；原电源/Back/Home 唤醒路由 | 保留；天气后台/深睡/低电量策略未改，尚需上机 |
| BootActivity / SleepActivity / FullScreenMessageActivity | 无通用可返回页，依启动/电源/消息生命周期 | 保留；禁止留下前一页标题目标 |
| CrashActivity 崩溃提示 | 原任意点击/返回 → 原恢复路径；标题本来就无 Back | 保留 |

`back-entry-inventory.csv` 是自动提取的所有生产 Activity/Apps/Weather 源码入口与行号，供合入时复核。矩阵覆盖了底层 UiList/UiTabList 继承路径，不能理解为每个页面都已经在硬件点过。
